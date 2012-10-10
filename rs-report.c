#include "rs-common.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
  SKIP_EMPTY=0x01,
  SKIP_TEMPLATES=0x02,
  SKIP_GOOD=0x04
};

void usage(const char* progName)
{
  printf("Usage: %s dumpfile [skip=etg] [sort=sdn]\n", progName);
}

int parseSkip(const char* skipSpec)
{
  int flags = 0;
  while (*skipSpec)
  {
    switch (*skipSpec)
    {
    case 'e':
      flags |= SKIP_EMPTY;
      break;
    case 't':
      flags |= SKIP_TEMPLATES;
      break;
    case 'g':
      flags |= SKIP_GOOD;
      break;
    default:
      break;
    }
    skipSpec++;
  }
  return flags;
}

void filterStorage(struct RecordStorage* rs, int skipFlags)
{
  size_t lastIdx = 0;
  for (size_t i = 0; i < rs->recordCount; i++)
  {
    struct RecordInfo* ri = rs->records[i];
    if ((skipFlags & SKIP_EMPTY && ri->fieldCount == 0) ||
      (skipFlags & SKIP_TEMPLATES && ri->isInstance) ||
      (skipFlags & SKIP_GOOD && ri->estMinSize >= ri->size))
      deleteRecordInfo(ri);
    else
    {
      rs->records[lastIdx] = ri;
      lastIdx++;
    }
  }
  rs->recordCount = lastIdx;
}

int compare_size(const void* p1, const void *p2)
{
  struct RecordInfo* ri1 = *(struct RecordInfo**) p1;
  struct RecordInfo* ri2 = *(struct RecordInfo**) p2;

  if (ri1->size > ri2->size)
    return -1;
  else if (ri1->size < ri2->size)
    return 1;

  return 0;
}

int compare_diff(const void* p1, const void *p2)
{
  struct RecordInfo* ri1 = *(struct RecordInfo**) p1;
  struct RecordInfo* ri2 = *(struct RecordInfo**) p2;
  int diff1 = (ri1->estMinSize < ri1->size) ? ri1->size - ri1->estMinSize : 0;
  int diff2 = (ri2->estMinSize < ri2->size) ? ri2->size - ri2->estMinSize : 0;

  return diff2 - diff1;
}

int compare_name(const void* p1, const void *p2)
{
  struct RecordInfo* ri1 = *(struct RecordInfo**) p1;
  struct RecordInfo* ri2 = *(struct RecordInfo**) p2;

  return strcmp(ri1->name, ri2->name);
}

void sortStorage(struct RecordStorage* rs, const char* sortSpec)
{
  while (*sortSpec)
  {
    switch (*sortSpec)
    {
    case 's':
      qsort(rs->records, rs->recordCount, sizeof(struct RecordInfo*), compare_size);
      break;
    case 'd':
      qsort(rs->records, rs->recordCount, sizeof(struct RecordInfo*), compare_diff);
      break;
    case 'n':
      qsort(rs->records, rs->recordCount, sizeof(struct RecordInfo*), compare_name);
      break;
    default:
      break;
    }
    sortSpec++;
  }
}

int main(int argc, char** argv)
{
  if (argc < 2)
  {
    usage(argv[0]);
    return 1;
  }

  int skipFlags = 0;
  const char* sortSpec = 0;

  for (int i = 2; i < argc; i++)
  {
    if (strstr(argv[i], "skip=") == argv[i])
      skipFlags = parseSkip(argv[i] + 5);
    else if (strstr(argv[i], "sort=") == argv[i])
      sortSpec = argv[i] + 5;
    else
    {
      printf("Unknown command-line option: %s\n", argv[i]);
      return 1;
    }
  }

  FILE* dumpFile = fopen(argv[1], "r");
  if (!dumpFile)
  {
    printf("Can't open dump file %s: %s\n", argv[1], strerror(errno));
    return 2;
  }

  struct RecordStorage* rs = loadRecordStorage(dumpFile);
  if (!rs)
  {
    printf("Can't load dump file %s: I/O error or invalid data in file\n", argv[1]);
    fclose(dumpFile);
    return 3;
  }
  fclose(dumpFile);

  filterStorage(rs, skipFlags);
  if (sortSpec)
    sortStorage(rs, sortSpec);

  for (size_t i = 0; i < rs->recordCount; i++)
    printRecordInfo(stdout, rs->records[i], true);

  deleteRecordStorage(rs);
  return 0;
}
