#ifndef RS_PLUGIN_H
#define RS_PLUGIN_H

#include "rs-types.h"

#include <gcc-plugin.h>

struct FieldInfo* createFieldInfo(const tree field_decl);
struct RecordInfo* createRecordInfo(const tree type_decl, const tree record_type);

void saveFieldInfo(FILE* file, const struct FieldInfo* ri);
void saveRecordInfo(FILE* file, const struct RecordInfo* ri);

void estimateMinRecordSize(struct RecordInfo* ri);

void printRecordInfo(const struct RecordInfo* ri, bool printLayout);

#endif
