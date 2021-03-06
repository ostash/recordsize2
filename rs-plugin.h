#ifndef RS_PLUGIN_H
#define RS_PLUGIN_H

#include "rs-types.h"

#include <gcc-plugin.h>

struct FieldInfo* createFieldInfo(const tree field_decl);
struct RecordInfo* createRecordInfo(const tree type_decl, const tree record_type);
struct RecordStorage* createRecordStorage();

void saveFieldInfo(FILE* file, const struct FieldInfo* ri);
void saveRecordInfo(FILE* file, const struct RecordInfo* ri);
void saveRecordStorage(FILE* file, const struct RecordStorage* rs);

void estimateMinRecordSize(struct RecordInfo* ri);

#endif
