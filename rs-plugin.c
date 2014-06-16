#include "rs-plugin.h"

#include <tree.h>
#include <cp/cp-tree.h>
#include <defaults.h>

enum
{
  FIELD_BASE = 0,
  FIELD_NONAME
};
static const char* fieldNames[] = {"base/vptr", "unnamed"};

struct FieldInfo* createFieldInfo(const tree field_decl)
{
  struct FieldInfo* fi = (struct FieldInfo*) xcalloc(1, sizeof(struct FieldInfo));
  fi->isSpecial = DECL_ARTIFICIAL(field_decl);
  fi->isBitField = DECL_BIT_FIELD(field_decl);

  const char* fieldName;
  if (fi->isSpecial)
    fieldName = fieldNames[FIELD_BASE];
  else if (DECL_NAME(field_decl))
    fieldName = IDENTIFIER_POINTER(DECL_NAME(field_decl));
  else
    fieldName = fieldNames[FIELD_NONAME];

  fi->name = xstrdup(fieldName);

  fi->size = TREE_INT_CST_LOW(DECL_SIZE(field_decl));

  // Offset calculation is a little bit wierd. According to GCC docs:
  // "... DECL_FIELD_OFFSET is position, counting in bytes, of the
  // DECL_OFFSET_ALIGN-bit sized word ..." and ".. DECL_FIELD_BIT_OFFSET is the
  // bit offset of the first bit of the field within this word"
  fi->offset = TREE_INT_CST_LOW(DECL_FIELD_OFFSET(field_decl)) * BITS_PER_UNIT +
    TREE_INT_CST_LOW(DECL_FIELD_BIT_OFFSET(field_decl));

  fi->align = DECL_ALIGN(field_decl);

  return fi;
}

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
  ri->fields = (struct FieldInfo**)xmalloc(fieldCapacity * sizeof(struct FieldInfo*));

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
     ri->fields = (struct FieldInfo**)xrealloc(ri->fields, fieldCapacity * sizeof(struct FieldInfo*));
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

struct RecordStorage* createRecordStorage()
{
  struct RecordStorage* rs = (struct RecordStorage*)xmalloc(sizeof(struct RecordStorage));
  rs->recordCount = 0;
  rs->recordCapacity = 256;
  rs->records = (struct RecordInfo**)xmalloc(rs->recordCapacity * sizeof(struct RecordInfo*));

  return rs;
}

void saveFieldInfo(FILE* file, const struct FieldInfo* fi)
{
  size_t len = strlen(fi->name);
  fwrite(&len, sizeof(len), 1, file);
  fwrite(fi->name, len + 1, 1, file);

  fwrite(&fi->size, sizeof(fi->size), 1, file);
  fwrite(&fi->offset, sizeof(fi->offset), 1, file);
  fwrite(&fi->align, sizeof(fi->align), 1, file);

  fwrite(&fi->isSpecial, sizeof(fi->isSpecial), 1, file);
  fwrite(&fi->isBitField, sizeof(fi->isBitField), 1, file);
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

void saveRecordStorage(FILE* file, const struct RecordStorage* rs)
{
  fwrite(&rs->recordCount, sizeof(rs->recordCount), 1, file);
  for (size_t i = 0; i < rs->recordCount; i++)
    saveRecordInfo(file, rs->records[i]);
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
