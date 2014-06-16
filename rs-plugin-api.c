#include <gcc-plugin.h>
#include <cp/cp-tree.h>
#include <langhooks.h>

#include <sys/file.h>

#include "rs-common.h"
#include "rs-plugin.h"

int plugin_is_GPL_compatible;
static struct plugin_info recordsize_plugin_info = { "0.3", "Record size plugin" };

// Process class templates instantiations
static bool flag_process_templates = false;
// Print record layout
static bool flag_print_layout = false;
// Print all records with layout
static bool flag_print_all = false;
static const char* fileDumpName = 0;
static FILE* fileDump = 0;
static struct RecordStorage* storage = 0;

static bool initStorage()
{
  // If dump file name present we want to open it for r/w and create if it
  // doesn't exist
  if (fileDumpName)
  {
    if (!(fileDump = fopen(fileDumpName, "a+")))
    {
      fprintf(stderr, "Can't open RecordSize dump file %s: %s\n", fileDumpName, xstrerror(errno));
      return false;
    }
    // Lock it
    flock(fileno(fileDump), LOCK_EX);
    // It could happen that dump is empty because we've just created it
    struct stat fileDumpStat;
    fstat(fileno(fileDump), &fileDumpStat);
    if (fileDumpStat.st_size == 0)
    {
      storage = createRecordStorage();
      return true;
    }
    // Let's read dump otherwise
    if ((storage = loadRecordStorage(fileDump)) == 0)
    {
      fprintf(stderr, "Can't read RecordSize dump file %s: I/O error or invalid data in file\n", fileDumpName);
      fclose(fileDump);
      return false;
    }
  }
  else
    storage = createRecordStorage();

  return true;
}

static void finalizeStorage()
{
  if (fileDump)
  {
    // Clear dump file
    ftruncate(fileno(fileDump), 0);
    // Save updated dump
    saveRecordStorage(fileDump, storage);
    // This will unlock dump file as well
    fclose(fileDump);
  }

  deleteRecordStorage(storage);
}

static bool isProcessed(const char* typeName)
{
  for (size_t i = 0; i < storage->recordCount; i++)
    if (strcmp(storage->records[i]->name, typeName) == 0)
      return true;

  return false;
}

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

  if (!isProcessed(type_as_string(aggregate_type, 0)))
  {
    struct RecordInfo* ri = createRecordInfo(type, aggregate_type);
    estimateMinRecordSize(ri);

    if (flag_print_all || ri->estMinSize < ri->size)
      printRecordInfo(stderr, ri, flag_print_layout);

    storage->recordCount++;
    if (storage->recordCount > storage->recordCapacity)
    {
      storage->recordCapacity *= 2;
      storage->records = (struct RecordInfo**)xrealloc(storage->records, storage->recordCapacity * sizeof(struct RecordInfo*));
    }

    storage->records[storage->recordCount - 1] = ri;
  }
}

static void processTemplate(const tree templateTree)
{
  // We are not interested in anything except class templates
  if (TREE_CODE(TREE_TYPE(templateTree)) != RECORD_TYPE)
   return;

  // TEMPLATE_DECL maintains chain of its instantiations
  for (tree instance = DECL_TEMPLATE_INSTANTIATIONS(templateTree); instance; instance = TREE_CHAIN(instance))
  {
    // instance is tree_list
    // TREE_VALUE(instance) is instantiated/specialized record_type

    tree record_type = TREE_VALUE(instance);

    // Instance can be partial or specialization. Even if it full specialization,
    // we still want to process only instantiated (really used) classes.
    if (!CLASSTYPE_TEMPLATE_INSTANTIATION(record_type))
      continue;

    // Now we are sure this is complete class template instantiation
    processType(TYPE_NAME(record_type));
  }
}

static void processName(const tree name)
{
  // For record size calculations we are interested in TYPE_DECLs and
  // TEMPLATE_DECLs
  // We are also don't want to process builtin compiler types as we won't be
  // able to modify them
  if (DECL_IS_BUILTIN(name))
    return;

  switch (TREE_CODE(name))
  {
  case TYPE_DECL:
    processType(name);
    break;
  case TEMPLATE_DECL:
    if (flag_process_templates)
      processTemplate(name);
    break;
  default:;
  }
}

static void traverseNamespace(const tree ns)
{
  if (!ns)
    return;

  // Namespace content is stored in NAMESPACE_LEVEL field of NAMESPACE_DECL node
  struct cp_binding_level* level = NAMESPACE_LEVEL(ns);

  // 'names' is a chain of all *_DECLs within namespace (except for USING_DECL
  // and NAMESPACE_DECL
  for (tree name = level->names; name; name = TREE_CHAIN(name))
    processName(name);

  // Nested namespaces are chained via 'namespaces' field
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

  // Initialize storage for records
  if (!initStorage())
    return;

  // GNU C++ stores root node of AST in variable 'global_namespace' which is
  // NAMESPACE_DECL. It corresponds to top-level C++ namespace '::'
  traverseNamespace(global_namespace);

  // Finalize storage for records
  finalizeStorage();
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
      if (strcmp(info->argv[i].key, "process-templates") == 0)
        flag_process_templates = true;
      if (strcmp(info->argv[i].key, "print-layout") == 0)
        flag_print_layout = true;
      if (strcmp(info->argv[i].key, "print-all") == 0)
      {
        flag_print_layout = true;
        flag_print_all = true;
      }
      if (strcmp(info->argv[i].key, "dumpfile") == 0)
      {
        flag_process_templates = true;
        fileDumpName = info->argv[i].value;
      }
    }
  }

  register_callback(info->base_name, PLUGIN_INFO, NULL, &recordsize_plugin_info);
  register_callback(info->base_name, PLUGIN_OVERRIDE_GATE, &recordsize_override_gate, NULL);
  register_callback(info->base_name, PLUGIN_FINISH_UNIT, &recordsize_override_gate, NULL);

  return 0;
}

#ifndef __cplusplus
expanded_location expand_location(source_location arg) __attribute__ ((weak));
expanded_location _Z15expand_locationj(source_location arg) __attribute__((weak));

expanded_location expand_location(source_location arg)
{
  return _Z15expand_locationj(arg);
}

expanded_location _Z15expand_locationj(source_location arg)
{
  return expand_location(arg);
}

const char* type_as_string(tree t, int i) __attribute__ ((weak));
const char* _Z14type_as_stringP9tree_nodei(tree, int) __attribute__ ((weak));

const char* type_as_string(tree t, int i)
{
  return _Z14type_as_stringP9tree_nodei(t, i);
}

const char* _Z14type_as_stringP9tree_nodei(tree t, int i)
{
  return type_as_string(t, i);
}

#endif
