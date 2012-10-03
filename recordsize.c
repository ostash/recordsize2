#include <gcc-plugin.h>
#include <cp/cp-tree.h>
#include <langhooks.h>

int plugin_is_GPL_compatible;
static struct plugin_info recordsize_plugin_info = { "0.3", "Record size plugin" };

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

static void processName(const tree name, struct NamespaceStats* stats)
{
  size_t* count = stats->namesCount;
  if (DECL_IS_BUILTIN(name))
    count = stats->builtinCount;

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
    count[CNT_TYPE]++;
    break;
  case NAMESPACE_DECL:
    count[CNT_NAMESPACE]++;
    break;
  case TEMPLATE_DECL:
    count[CNT_TEMPLATE]++;
    break;
  default:
    count[CNT_OTHER]++;
  }
}

static void traverseNamespace(const tree ns)
{
  if (!ns)
    return;

  struct cp_binding_level* level = NAMESPACE_LEVEL(ns);
  printf("%s, contains %zu names\n", cxx_printable_name(ns, 0xff), level->names_size);

  struct NamespaceStats* stats = xcalloc(1, sizeof(struct NamespaceStats));

  for (tree name = level->names; name; name = TREE_CHAIN(name))
    processName(name, stats);

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

  free(stats);

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

  register_callback(info->base_name, PLUGIN_INFO, NULL, &recordsize_plugin_info);
  register_callback(info->base_name, PLUGIN_OVERRIDE_GATE, &recordsize_override_gate, NULL);

  return 0;
}
