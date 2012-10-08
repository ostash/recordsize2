#include <gcc-plugin.h>
#include <cp/cp-tree.h>
#include <langhooks.h>

#include "RecordInfo.h"

int plugin_is_GPL_compatible;
static struct plugin_info recordsize_plugin_info = { "0.3", "Record size plugin" };

// Process class templates instantiations
static bool flag_process_templates = false;
// Print record layout
static bool flag_print_layout = false;
// Print all records with layout
static bool flag_print_all = false;
static const char* fileDumpName = 0;

static struct RecordInfo** records;
static size_t recordCount = 0;
static size_t recordCapacity = 256;

static bool loadDump()
{
  // Check whether dump already exists
  if (access(fileDumpName, F_OK) == -1)
    return true;

  FILE* file = fopen(fileDumpName, "r");
  if (!file)
  {
    error("Can't open RecordSize dump file %s: %s", fileDumpName, xstrerror(errno));
    return false;
  }

  if (fread(&recordCount, sizeof(recordCount), 1, file) != 1)
    goto out_dump;
  recordCapacity = recordCount;
  records = xcalloc(recordCount, sizeof(struct RecordInfo*));
  for (size_t i = 0; i < recordCount; i++)
    if ((records[i] = loadRecordInfo(file)) == 0)
      goto out_records;

  fclose(file);
  return true;

out_records:
  for (size_t i = 0; i < recordCount; i++)
    if (records[i])
      deleteRecordInfo(records[i]);
  free(records);
out_dump:
  error("Can't read RecordSize dump file %s: I/O error or invalid data in file", fileDumpName);
  fclose(file);
  return false;
}

static void saveDump()
{
  FILE* file = fopen(fileDumpName, "w");
  if (!file)
  {
    error("Can't open RecordSize dump file %s: %s", fileDumpName, xstrerror(errno));
    return;
  }

  fwrite(&recordCount, sizeof(recordCount), 1, file);
  for (size_t i = 0; i < recordCount; i++)
    saveRecordInfo(file, records[i]);

  fclose(file);
}

static void deleteRecords()
{
  for (size_t i = 0; i < recordCount; i++)
    deleteRecordInfo(records[i]);

  free(records);
}

static bool isProcessed(const char* typeName)
{
  for (size_t i = 0; i < recordCount; i++)
    if (strcmp(records[i]->name, typeName) == 0)
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
      printRecordInfo(ri, flag_print_layout);

    recordCount++;
    if (recordCount > recordCapacity)
    {
      recordCapacity *= 2;
      records = xrealloc(records, recordCapacity * sizeof(struct RecordInfo*));
    }

    records[recordCount - 1] = ri;
  }
}

static void processTemplate(const tree template)
{
  // We are not interested in anything except class templates
  if (TREE_CODE(TREE_TYPE(template)) != RECORD_TYPE)
   return;

  // TEMPLATE_DECL maintains chain of its instantiations
  for (tree instance = DECL_TEMPLATE_INSTANTIATIONS(template); instance; instance = TREE_CHAIN(instance))
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

  // Initialize storage for records (if they aren't loaded from dump)
  if (!records)
    records = xmalloc(recordCapacity * sizeof(struct RecordInfo*));

  // GNU C++ stores root node of AST in variable 'global_namespace' which is
  // NAMESPACE_DECL. It corresponds to top-level C++ namespace '::'
  traverseNamespace(global_namespace);

  if (fileDumpName)
    saveDump();
  // Free records
  deleteRecords();
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
        if (!loadDump())
          return 1;
      }
    }
  }

  register_callback(info->base_name, PLUGIN_INFO, NULL, &recordsize_plugin_info);
  register_callback(info->base_name, PLUGIN_OVERRIDE_GATE, &recordsize_override_gate, NULL);

  return 0;
}
