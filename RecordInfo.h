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
  size_t firstField;
  bool hasBitFields;
  bool isInstance;
  bool hasVirtualBase;
};

struct RecordInfo* createRecordInfo(const tree type_decl, const tree record_type);
void deleteRecordInfo(struct RecordInfo* ri);
void printRecordInfo(struct RecordInfo* ri, bool offsetDetails);

#endif
