#ifndef RS_COMMON_H
#define RS_COMMON_H

#include "rs-types.h"

#include <stdio.h>

void deleteFieldInfo(struct FieldInfo* fi);
void deleteRecordInfo(struct RecordInfo* ri);
void deleteRecordStorage(struct RecordStorage* rs);

struct FieldInfo* loadFieldInfo(FILE* file);
struct RecordInfo* loadRecordInfo(FILE* file);
struct RecordStorage* loadRecordStorage(FILE* file);

void printRecordInfo(const struct RecordInfo* ri, bool printLayout);

#endif
