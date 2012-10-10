all: recordsize

recordsize:
	gcc -Wall -std=gnu99 -O0 -g -march=native -shared -fpic -I `gcc --print-file-name=plugin`/include -o recordsize.so \
	recordsize.c common.c RecordInfo.c FieldInfo.c

test1:
	g++ -fplugin=./recordsize.so -fplugin-arg-recordsize-process-templates -fplugin-arg-recordsize-print-all test1.h

test2:
	g++ -fplugin=./recordsize.so -fplugin-arg-recordsize-process-templates -fplugin-arg-recordsize-print-all test2.h

dump1:
	g++ -fdump-tree-all test1.h

dump2:
	g++ -fdump-tree-all test2.h
