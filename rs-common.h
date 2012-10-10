#ifndef RS_COMMON_H
#define RS_COMMON_H

#include "rs-types.h"

#include <stdio.h>

void deleteFieldInfo(struct FieldInfo* fi);
void deleteRecordInfo(struct RecordInfo* ri);

struct RecordInfo* loadRecordInfo(FILE* file);
struct FieldInfo* loadFieldInfo(FILE* file);

#endif
