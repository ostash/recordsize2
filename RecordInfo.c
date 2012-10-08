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
  ri->estMinSize = SIZE_MAX;

  size_t fieldCapacity = 4;
  ri->fields = xmalloc(fieldCapacity * sizeof(struct FieldInfo*));

  // Fields/variables/constants/functions are chained via TYPE_FIELDS of record
  for (tree field = TYPE_FIELDS(record_type); field; field = TREE_CHAIN(field))
  {
    // We're intersted in fields only
    if (TREE_CODE(field) != FIELD_DECL)
      continue;

    struct FieldInfo* fi = createFieldInfo(field);

    ri->fieldCount++;

    // Allocate more storage for fields if needed
    if (ri->fieldCount > fieldCapacity)
    {
     fieldCapacity *= 2;
     ri->fields = xrealloc(ri->fields, fieldCapacity * sizeof(struct FieldInfo*));
    }

    ri->fields[ri->fieldCount - 1] = fi;

    // Mark record as containing bit-fields
    if (fi->isBitField)
      ri->hasBitFields = true;

    // Field is base/vptr
    if (fi->isSpecial)
    {
      // If we encounter special field somewhere after regular fields
      // it means class has virtual base.
      if (ri->firstField != SIZE_MAX)
        ri->hasVirtualBase = true;
    }
    else if (ri->firstField == SIZE_MAX)
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

void printRecordInfo(const struct RecordInfo* ri, bool offsetDetails)
{
  char recordFlags[6] = "\0";
  if (ri->hasBitFields || ri->isInstance || ri->hasVirtualBase)
  {
    char* rf = recordFlags;
    *rf++ = '[';
    if (ri->hasBitFields)
      *rf++ = 'B';
    if (ri->isInstance)
      *rf++ = 'T';
    if (ri->hasVirtualBase)
      *rf++ = 'V';
    *rf++ = ']';
    *rf++ = ' ';
    *rf = 0;
  }

  printf("Record %s%s at %s:%zu; size %zu bits, align %zu bits, total %zu field(s)\n", recordFlags, ri->name,
    ri->fileName, ri->line, ri->size, ri->align, ri->fieldCount);
  if (ri->estMinSize < ri->size)
    printf("Warning: estimated minimal size is only %zu\n", ri->estMinSize);

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

void estimateMinRecordSize(struct RecordInfo* ri)
{
  // At the moment we can't handle some cases
  if (ri->hasBitFields || ri->hasVirtualBase)
    return;

  // Handle records with bases only
  if (ri->firstField == SIZE_MAX)
  {
    ri->estMinSize = ri->size;
    return;
  }

  // We assume that field alignment is always power of two, so we can always reorder them
  // to have record 'packed'
  size_t fieldsSize = 0;
  size_t maxFieldAlign = 0;
  for (size_t i = ri->firstField; i < ri->fieldCount; i++)
  {
    fieldsSize += ri->fields[i]->size;
    if (ri->fields[i]->align > maxFieldAlign)
      maxFieldAlign = ri->fields[i]->align;
  }

  size_t endOfBases = 0;
  // If we have bases there can be a need for additional padding between bases and first field
  if (ri->firstField != 0)
  {
    endOfBases = ri->fields[ri->firstField - 1]->offset + ri->fields[ri->firstField - 1]->bitOffset +
      ri->fields[ri->firstField - 1]->size;
    if (endOfBases % maxFieldAlign)
      endOfBases = (endOfBases / maxFieldAlign + 1) * maxFieldAlign;
  }

  ri->estMinSize = endOfBases + fieldsSize;
  // Let us add final padding if needed
  if (ri->estMinSize % ri->align)
    ri->estMinSize = (ri->estMinSize / ri->align + 1) * ri->align;
}

void saveRecordInfo(FILE* file, const struct RecordInfo* ri)
{
  fwrite(&ri->fieldCount, sizeof(ri->fieldCount), 1, file);
  for (size_t i = 0; i < ri->fieldCount; i++)
    saveFieldInfo(file, ri->fields[i]);

  size_t len = strlen(ri->name);
  fwrite(&len, sizeof(len), 1, file);
  fwrite(ri->name, len + 1, 1, file);

  len = strlen(ri->fileName);
  fwrite(&len, sizeof(len), 1, file);
  fwrite(ri->fileName, len + 1, 1, file);

  fwrite(&ri->line, sizeof(ri->line), 1, file);
  fwrite(&ri->size, sizeof(ri->size), 1, file);
  fwrite(&ri->align, sizeof(ri->align), 1, file);

  fwrite(&ri->firstField, sizeof(ri->firstField), 1, file);

  fwrite(&ri->estMinSize, sizeof(ri->estMinSize), 1, file);

  fwrite(&ri->hasBitFields, sizeof(ri->hasBitFields), 1, file);
  fwrite(&ri->isInstance, sizeof(ri->isInstance), 1, file);
  fwrite(&ri->hasVirtualBase, sizeof(ri->hasVirtualBase), 1, file);
}

struct RecordInfo* loadRecordInfo(FILE* file)
{
  struct RecordInfo* ri = (struct RecordInfo*) xcalloc(1, sizeof(struct RecordInfo));

  fread(&ri->fieldCount, sizeof(ri->fieldCount), 1, file);
  ri->fields = xmalloc(ri->fieldCount * sizeof(struct FieldInfo*));
  for (size_t i = 0; i < ri->fieldCount; i++)
    ri->fields[i] = loadFieldInfo(file);

  size_t len;
  fread(&len, sizeof(len), 1, file);
  ri->name = xmalloc(len + 1);
  fread(ri->name, len + 1, 1, file);

  fread(&len, sizeof(len), 1, file);
  ri->fileName = xmalloc(len + 1);
  fread(ri->fileName, len + 1, 1, file);

  fread(&ri->line, sizeof(ri->line), 1, file);
  fread(&ri->size, sizeof(ri->size), 1, file);
  fread(&ri->align, sizeof(ri->align), 1, file);

  fread(&ri->firstField, sizeof(ri->firstField), 1, file);

  fread(&ri->estMinSize, sizeof(ri->estMinSize), 1, file);

  fread(&ri->hasBitFields, sizeof(ri->hasBitFields), 1, file);
  fread(&ri->isInstance, sizeof(ri->isInstance), 1, file);
  fread(&ri->hasVirtualBase, sizeof(ri->hasVirtualBase), 1, file);

  return ri;
}
