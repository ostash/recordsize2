#include "rs-common.h"

#include <libiberty.h>
#include <stdlib.h>
#include <string.h>

void deleteFieldInfo(struct FieldInfo* fi)
{
  free(fi->name);
  free(fi);
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

void deleteRecordStorage(struct RecordStorage* rs)
{
  for (size_t i = 0; i < rs->recordCount; i++)
    deleteRecordInfo(rs->records[i]);

  free(rs);
}

struct FieldInfo* loadFieldInfo(FILE* file)
{
  struct FieldInfo* fi = xmalloc(sizeof(struct FieldInfo));

  // Read field name length
  size_t len;
  if (fread(&len, sizeof(len), 1, file) != 1)
    goto out_fi;
  fi->name = xmalloc(len + 1);
  // Read field name
  if (fread(fi->name, len + 1, 1, file) != 1 || fi->name[len] != 0)
    goto out_name;
  // Read field size
  if (fread(&fi->size, sizeof(fi->size), 1, file) != 1)
    goto out_name;
  // Read field offset
  if (fread(&fi->offset, sizeof(fi->offset), 1, file) != 1)
    goto out_name;
  // Read field align
  if (fread(&fi->align, sizeof(fi->align), 1, file) != 1)
    goto out_name;
  // Read whether field is base/vptr
  if (fread(&fi->isSpecial, sizeof(fi->isSpecial), 1, file) != 1)
    goto out_name;
  // Read where field is bit-field
  if (fread(&fi->isBitField, sizeof(fi->isBitField), 1, file) != 1)
    goto out_name;

  return fi;

out_name:
  free(fi->name);
out_fi:
  free(fi);
  return 0;
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

struct RecordStorage* loadRecordStorage(FILE* file)
{
  struct RecordStorage* rs = (struct RecordStorage*) xcalloc(1, sizeof(struct RecordStorage));

  // Read record count
  if (fread(&rs->recordCount, sizeof(rs->recordCount), 1, file) != 1)
    goto out_rs;
  rs->recordCapacity = rs->recordCount;
  rs->records = xcalloc(rs->recordCount, sizeof(struct RecordInfo*));
  for (size_t i = 0; i < rs->recordCount; i++)
    if ((rs->records[i] = loadRecordInfo(file)) == 0)
      goto out_records;

  return rs;

out_records:
  for (size_t i = 0; i < rs->recordCount; i++)
    if (rs->records[i])
      deleteRecordInfo(rs->records[i]);
  free(rs->records);
out_rs:
  free(rs);
  return 0;
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
