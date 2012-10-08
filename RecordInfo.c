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

void printRecordInfo(const struct RecordInfo* ri, bool printLayout)
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

  if (!printLayout || ri->fieldCount == 0)
    return;

  char* colNames[] = { "#", "Name", "Offset", "Size", "Align", "Special" ,"Bit" };
  int colWidths[] = { 3, 32, 6, 6, 3, 1, 1};
  const size_t colCount = sizeof(colNames) / sizeof(char*);
  for (size_t i = 1; i < colCount; i++)
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
      fi->offset, colWidths[3] ,fi->size, colWidths[4], fi->align, colWidths[5], fi->isSpecial,
      colWidths[6], fi->isBitField);
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
    endOfBases = ri->fields[ri->firstField - 1]->offset + ri->fields[ri->firstField - 1]->size;
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

  // Read field count
  if (fread(&ri->fieldCount, sizeof(ri->fieldCount), 1, file) != 1)
    goto out_ri;
  // Read fields
  ri->fields = xcalloc(ri->fieldCount, sizeof(struct FieldInfo*));
  for (size_t i = 0; i < ri->fieldCount; i++)
    if ((ri->fields[i] = loadFieldInfo(file)) == 0)
      goto out_fields;
  // Read record name length
  size_t len;
  if (fread(&len, sizeof(len), 1, file) != 1)
    goto out_fields;
  ri->name = xmalloc(len + 1);
  // Read record name
  if (fread(ri->name, len + 1, 1, file) != 1 || ri->name[len] != 0)
    goto out_name;
  // Read source file name length
  if (fread(&len, sizeof(len), 1, file) != 1)
    goto out_name;
  ri->fileName = xmalloc(len + 1);
  // Read source file name
  if (fread(ri->fileName, len + 1, 1, file) != 1 || ri->fileName[len] != 0)
    goto out_fileName;
  // Read source line
  if (fread(&ri->line, sizeof(ri->line), 1, file) != 1)
    goto out_fileName;
  // Read record size
  if (fread(&ri->size, sizeof(ri->size), 1, file) != 1)
    goto out_fileName;
  // Read record align
  if (fread(&ri->align, sizeof(ri->align), 1, file) != 1)
    goto out_fileName;
  // Read first non-base/vptr field index
  if (fread(&ri->firstField, sizeof(ri->firstField), 1, file) != 1)
    goto out_fileName;
  // Read estimated minimal size
  if (fread(&ri->estMinSize, sizeof(ri->estMinSize), 1, file) != 1)
    goto out_fileName;
  // Read whether record contains bit-fields
  if (fread(&ri->hasBitFields, sizeof(ri->hasBitFields), 1, file) != 1)
    goto out_fileName;
  // Read whether record is template instance
  if (fread(&ri->isInstance, sizeof(ri->isInstance), 1, file) != 1)
    goto out_fileName;
  // Read whether record has virtual base(s)
  if (fread(&ri->hasVirtualBase, sizeof(ri->hasVirtualBase), 1, file) != 1)
    goto out_fileName;

  return ri;

out_fileName:
  free(ri->fileName);
out_name:
  free(ri->name);
out_fields:
  for (size_t i = 0; i < ri->fieldCount; i++)
    if (ri->fields[i])
      deleteFieldInfo(ri->fields[i]);
  free(ri->fields);
out_ri:
  free(ri);
  return 0;
}
