#ifndef COMMON_H
#define COMMON_H

void deleteFieldInfo(struct FieldInfo* fi);
void deleteRecordInfo(struct RecordInfo* ri);

struct RecordInfo* loadRecordInfo(FILE* file);
struct FieldInfo* loadFieldInfo(FILE* file);

#endif
