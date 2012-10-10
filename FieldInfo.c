#include "FieldInfo.h"

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
