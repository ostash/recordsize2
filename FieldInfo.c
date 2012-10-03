#include "FieldInfo.h"

#include <defaults.h>

enum
{
  FIELD_BASE = 0,
  FIELD_NONAME
};
static const char* fieldNames[] = {"base class", "unnamed"};

struct FieldInfo* createFieldInfo(const tree field_decl)
{
  struct FieldInfo* fi = (struct FieldInfo*) xcalloc(1, sizeof(struct FieldInfo));
  fi->isBase = DECL_ARTIFICIAL(field_decl);
  fi->isBitField = DECL_BIT_FIELD(field_decl);

  const char* fieldName;
  if (fi->isBase)
    fieldName = fieldNames[FIELD_BASE];
  else if (DECL_NAME(field_decl))
    fieldName = IDENTIFIER_POINTER(DECL_NAME(field_decl));
  else
    fieldName = fieldNames[FIELD_NONAME];

  fi->name = xstrdup(fieldName);

  fi->size = TREE_INT_CST_LOW(DECL_SIZE(field_decl));
  fi->offset = TREE_INT_CST_LOW(DECL_FIELD_OFFSET(field_decl)) * BITS_PER_UNIT;
  fi->bitOffset = TREE_INT_CST_LOW(DECL_FIELD_BIT_OFFSET(field_decl));
  fi->offsetAlign = DECL_OFFSET_ALIGN(field_decl);
  fi->align = DECL_ALIGN(field_decl);

  return fi;
}

void deleteFieldInfo(struct FieldInfo* fi)
{
  free(fi->name);
  free(fi);
}
