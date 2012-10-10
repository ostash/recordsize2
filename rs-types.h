#ifndef RS_TYPES_H
#define RS_TYPES_H

#include <stdbool.h>
#include <stddef.h>

struct FieldInfo
{
  char* name;
  size_t size;
  size_t offset;
  size_t align;
  // Field is base class or vptr
  bool isSpecial;
  bool isBitField;
};

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

struct RecordStorage
{
  struct RecordInfo** records;
  size_t recordCount;
  size_t recordCapacity;
};

#endif
