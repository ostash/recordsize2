#ifndef RECORDINFO_H
#define RECORDINFO_H

#include "FieldInfo.h"

struct RecordInfo
{
  struct FieldInfo** fields;
  char* name;
  char* fileName;
  size_t line;
  size_t size;
  size_t align;
  size_t fieldCount;
  // Index of first non-special field
  size_t firstField;
  // Estimated minimal size
  size_t estMinSize;
  bool hasBitFields;
  bool isInstance;
  bool hasVirtualBase;
};

struct RecordInfo* createRecordInfo(const tree type_decl, const tree record_type);
void deleteRecordInfo(struct RecordInfo* ri);
void printRecordInfo(const struct RecordInfo* ri, bool offsetDetails);

void estimateMinRecordSize(struct RecordInfo* ri);

void saveRecordInfo(FILE* file, const struct RecordInfo* ri);
struct RecordInfo* loadRecordInfo(FILE* file);

#endif
