#include "RecordInfo.h"

#include <cp/cp-tree.h>


struct RecordInfo* createRecordInfo(const tree type_decl, const tree record_type)
{
  struct RecordInfo* ri = (struct RecordInfo*) xcalloc(1, sizeof(struct RecordInfo));

  ri->name = xstrdup(type_as_string(record_type, 0));
  ri->fileName = xstrdup(DECL_SOURCE_FILE(type_decl));
  ri->line = DECL_SOURCE_LINE(type_decl);
  ri->size = TREE_INT_CST_LOW(TYPE_SIZE(record_type));
  ri->align = TYPE_ALIGN(record_type);

  size_t fieldCapacity = 4;
  ri->fields = xmalloc(fieldCapacity * sizeof(struct FieldInfo*));

  for (tree field = TYPE_FIELDS(record_type); field; field = TREE_CHAIN(field))
  {
    // We're intersted in fields only
    if (TREE_CODE(field) != FIELD_DECL)
      continue;

    struct FieldInfo* fi = createFieldInfo(field);

    ri->fieldCount++;
    if (ri->fieldCount > fieldCapacity)
    {
     fieldCapacity *= 2;
     ri->fields = xrealloc(ri->fields, fieldCapacity * sizeof(struct FieldInfo*));
    }

    ri->fields[ri->fieldCount - 1] = fi;
  }

  return ri;
}

void deleteRecordInfo(struct RecordInfo* ri)
{
  free(ri->name);
  free(ri->fileName);

  for (size_t i = 0; i < ri->fieldCount; i++)
    deleteFieldInfo(ri->fields[i]);

  free(ri->fields);
  free(ri);
}

void printRecordInfo(struct RecordInfo* ri)
{
  printf("Record %s at %s:%d; size %zu bits, align %zu bits, total %zu field(s)\n", ri->name,
    ri->fileName, ri->line, ri->size, ri->align, ri->fieldCount);

  if (ri->fieldCount == 0)
    return;

  char* colNames[] = { "#", "Name", "Offset", "Size", "Align", "Base" ,"Bit" };
  int colWidths[] = { 3, 32, 6, 6, 3, 1, 1};
  const size_t colCount = sizeof(colNames) / sizeof(char*);
  for (size_t i = 2; i < colCount; i++)
  {
    int len = strlen(colNames[i]);
    if (len > colWidths[i])
      colWidths[i] = len;
  }

  printf("%*s|%-*s|%-*s|%-*s|%-*s|%-*s|%-*s\n", colWidths[0], colNames[0], colWidths[1], colNames[1],
    colWidths[2], colNames[2], colWidths[3], colNames[3], colWidths[4], colNames[4], colWidths[5], colNames[5],
    colWidths[6], colNames[6]);

  for (size_t i = 0; i < ri->fieldCount; i++)
  {
    struct FieldInfo* fi = ri->fields[i];
    printf("%*zu|%-*s|%*zu|%*zu|%*zu|%*d|%*d\n", colWidths[0], i, colWidths[1], fi->name, colWidths[2],
      fi->offset, colWidths[3], fi->size, colWidths[4], fi->align, colWidths[5], fi->isBase, colWidths[6],
      fi->isBitField);
  }

}