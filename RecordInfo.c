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
  ri->isInstance = CLASSTYPE_TEMPLATE_INSTANTIATION(record_type);
  ri->firstField = SIZE_MAX;

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
    if (fi->isBitField)
      ri->hasBitFields = true;

    if (ri->firstField == SIZE_MAX && !fi->isSpecial)
      ri->firstField = ri->fieldCount - 1;
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

void printRecordInfo(struct RecordInfo* ri, bool offsetDetails)
{
  char recordFlags[5] = "\0";
  if (ri->hasBitFields || ri->isInstance)
  {
    char* rf = recordFlags;
    *rf++ = '[';
    if (ri->hasBitFields)
      *rf++ = 'B';
    if (ri->isInstance)
      *rf++ = 'T';
    *rf++ = ']';
    *rf++ = ' ';
    *rf = 0;
  }

  printf("Record %s%s at %s:%zu; size %zu bits, align %zu bits, total %zu field(s)\n", recordFlags, ri->name,
    ri->fileName, ri->line, ri->size, ri->align, ri->fieldCount);

  if (ri->fieldCount == 0)
    return;

  char* colNames[] = { "#", "Name", "Offset", "BitOffset", "OffsetAlign", "Size", "Align", "Special" ,"Bit" };
  int colWidths[] = { 3, 32, 6, 3, 3, 6, 3, 1, 1};
  const size_t colCount = sizeof(colNames) / sizeof(char*);
  for (size_t i = 2; i < colCount; i++)
  {
    int len = strlen(colNames[i]);
    if (len > colWidths[i])
      colWidths[i] = len;
  }

  if (offsetDetails)
    printf("%*s|%-*s|%-*s|%-*s|%-*s|%-*s|%-*s|%-*s|%-*s\n", colWidths[0], colNames[0], colWidths[1], colNames[1],
      colWidths[2], colNames[2], colWidths[3], colNames[3], colWidths[4], colNames[4], colWidths[5], colNames[5],
      colWidths[6], colNames[6], colWidths[7], colNames[7], colWidths[8], colNames[8]);
  else
    printf("%*s|%-*s|%-*s|%-*s|%-*s|%-*s|%-*s\n", colWidths[0], colNames[0], colWidths[1], colNames[1],
      colWidths[2], colNames[2], colWidths[5], colNames[5], colWidths[6], colNames[6], colWidths[7], colNames[7],
      colWidths[8], colNames[8]);

  for (size_t i = 0; i < ri->fieldCount; i++)
  {
    struct FieldInfo* fi = ri->fields[i];
    if (offsetDetails)
      printf("%*zu|%-*s|%*zu|%*zu|%*zu|%*zu|%*zu|%*d|%*d\n", colWidths[0], i, colWidths[1], fi->name, colWidths[2],
        fi->offset, colWidths[3], fi->bitOffset, colWidths[4], fi->offsetAlign, colWidths[5] ,fi->size, colWidths[6],
        fi->align, colWidths[7], fi->isSpecial, colWidths[8], fi->isBitField);
    else
      printf("%*zu|%-*s|%*zu|%*zu|%*zu|%*d|%*d\n", colWidths[0], i, colWidths[1], fi->name, colWidths[2],
        fi->offset + fi->bitOffset, colWidths[5] ,fi->size, colWidths[6],
        fi->align, colWidths[7], fi->isSpecial, colWidths[8], fi->isBitField);
  }

}
