#ifndef FIELDINFO_H
#define FIELDINDO_H

#include <gcc-plugin.h>
#include <tree.h>

struct FieldInfo
{
  char* name;
  size_t size;
  size_t offset;
  size_t bitOffset;
  size_t offsetAlign;
  size_t align;
  // Field is base class or vptr
  bool isSpecial;
  bool isBitField;
};

struct FieldInfo* createFieldInfo(const tree field_decl);
void deleteFieldInfo(struct FieldInfo* fi);

#endif
