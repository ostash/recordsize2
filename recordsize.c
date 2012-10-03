#include <gcc-plugin.h>
#include <cp/cp-tree.h>
#include <langhooks.h>

#include "RecordInfo.h"

int plugin_is_GPL_compatible;
static struct plugin_info recordsize_plugin_info = { "0.3", "Record size plugin" };

// Print namespace statistics
static bool flag_print_ns = false;

enum
{
  CNT_FUNCTION,
  CNT_VAR,
  CNT_CONST,
  CNT_TYPE,
  CNT_NAMESPACE,
  CNT_TEMPLATE,
  CNT_OTHER,
  CNT_LAST
};

static const char* declNames[] = {
  "Functions",
  "Variables",
  "Constans",
  "Types",
  "Namespaces",
  "Templates",
  "Other",
   };

struct NamespaceStats
{
  size_t namesCount[CNT_LAST];
  size_t builtinCount[CNT_LAST];
};

static void processType(const tree type)
{
  // Type could be (here, in level->names)
  // 1) Just regular declaration of struct/union/enum
  // 2) Typedef for 1)
  // 3) Typedef which specializes template

  // Case 1) should have DECL_ARTIFICIAL equal to true
  // Cases 2) and 3) will have DECL_ARTIFICIAL equal to false

  // We could simply ignore case 2) as type will be reported when we process
  // original unaliased type (and that is good, because we should fix original,
  // not alias).

  // Case 3) should be ignored as well, because typedef itself doesn't
  // instantiate template. Template instantiations should be tracked via
  // TEMPLATE_DECLs.

  if (!DECL_ARTIFICIAL(type))
    return;

  tree aggregate_type = TREE_TYPE(type);
  // Here we got our aggregate with all members, but we are intersted only in
  // records (structs, classes) and can ignore unions and enums.
  if (TREE_CODE(aggregate_type) != RECORD_TYPE)
    return;

  // We want to ignore forward declaration as well (they're not complete)
  if (!COMPLETE_TYPE_P(aggregate_type))
    return;

  struct RecordInfo* ri = createRecordInfo(type, aggregate_type);
  printRecordInfo(ri);
  deleteRecordInfo(ri);
}

static void processTemplate(const tree template)
{
  for (tree instance = DECL_TEMPLATE_INSTANTIATIONS(template); instance; instance = TREE_CHAIN(instance))
  {
    // instance is tree_list
    // TREE_VALUE(instance) is instantiated/specialized record_type/function_decl

    tree record_type = TREE_VALUE(instance);

    // Ignore function templates
    if (TREE_CODE(record_type) != RECORD_TYPE)
      break;

    // We want only fully instantiated class templates
    if (!CLASSTYPE_TEMPLATE_INSTANTIATION(record_type))
      continue;

    // processType wants TYPE_DECL
    processType(TYPE_NAME(TREE_VALUE(instance)));
  }
}

static void processName(const tree name, struct NamespaceStats* stats)
{
  if (!stats)
  {
    if (!DECL_IS_BUILTIN(name))
    {
      if (TREE_CODE(name) == TYPE_DECL)
        processType(name);
      else if (TREE_CODE(name) == TEMPLATE_DECL)
        processTemplate(name);
    }
    return;
  }

  size_t* count = stats->namesCount;
  bool isBuiltin = DECL_IS_BUILTIN(name);
  if (isBuiltin)
    count = stats->builtinCount;

  // For record size calculations we are interested in TYPE_DECLs
  switch (TREE_CODE(name))
  {
  case FUNCTION_DECL:
    count[CNT_FUNCTION]++;
    break;
  case VAR_DECL:
    count[CNT_VAR]++;
    break;
  case CONST_DECL:
    count[CNT_CONST]++;
    break;
  case TYPE_DECL:
    if (!isBuiltin)
      processType(name);
    count[CNT_TYPE]++;
    break;
  case NAMESPACE_DECL:
    count[CNT_NAMESPACE]++;
    break;
  case TEMPLATE_DECL:
    if (!isBuiltin)
      processTemplate(name);
    count[CNT_TEMPLATE]++;
    break;
  default:
    count[CNT_OTHER]++;
  }
}

static void printNamespaceStats(struct NamespaceStats* stats)
{
  size_t totalCount = 0;
  puts(" Non-builtin names stats:");
  for (size_t i = 0; i < CNT_LAST; i++)
  {
    printf("  %-16s%7zu\n", declNames[i], stats->namesCount[i]);
    totalCount += stats->namesCount[i];
  }
  printf(" Total non-builtin names: %zu\n", totalCount);

  totalCount = 0;
  puts(" Builtin names stats:");
  for (size_t i = 0; i < CNT_LAST; i++)
  {
    printf("  %-16s%7zu\n", declNames[i], stats->builtinCount[i]);
    totalCount += stats->builtinCount[i];
  }
  printf(" Total builtin names: %zu\n", totalCount);
}

static void traverseNamespace(const tree ns)
{
  if (!ns)
    return;

  struct cp_binding_level* level = NAMESPACE_LEVEL(ns);
  struct NamespaceStats* stats = 0;

  if (flag_print_ns)
  {
    printf("%s, contains %zu names\n", cxx_printable_name(ns, 0xff), level->names_size);
    stats =  xcalloc(1, sizeof(struct NamespaceStats));
  }

  for (tree name = level->names; name; name = TREE_CHAIN(name))
    processName(name, stats);

  if (flag_print_ns)
  {
    printNamespaceStats(stats);
    free(stats);
  }

  for (tree childNs = level->namespaces; childNs; childNs = TREE_CHAIN(childNs))
    traverseNamespace(childNs);

}

static void recordsize_override_gate(void *gcc_data, void *plugin_data)
{
  // This callback is executed before each optimization pass
  // We'd like to work with most unmodified AST, that is before first pass
  static bool firstTime = true;
  if (!firstTime)
    return;

  firstTime = false;

  traverseNamespace(global_namespace);
}

int plugin_init(struct plugin_name_args* info, struct plugin_gcc_version* ver)
{
  // Language frontends have differences in representing AST
  // At the moment we support only GNU C++
  if (strcmp(lang_hooks.name, "GNU C++") != 0)
  {
    fprintf(stderr, "Recordsize plugin supports only GNU C++ frontend\n");
    return 1;
  }

  if (info->argc)
  {
    for (int i = 0; i < info->argc; ++i)
    {
      if (strcmp(info->argv[i].key, "print-ns") == 0)
        flag_print_ns = 1;
    }
  }

  register_callback(info->base_name, PLUGIN_INFO, NULL, &recordsize_plugin_info);
  register_callback(info->base_name, PLUGIN_OVERRIDE_GATE, &recordsize_override_gate, NULL);

  return 0;
}
