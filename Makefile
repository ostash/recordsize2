all: recordsize

recordsize:
	gcc -Wall -std=gnu99 -O0 -g -march=native -shared -fpic -I `gcc --print-file-name=plugin`/include -o recordsize.so \
	recordsize.c RecordInfo.c FieldInfo.c

test1:
	g++ -fplugin=./recordsize.so test1.h

test2:
	g++ -fplugin=./recordsize.so test2.h

dump1:
	g++ -fdump-tree-all test1.h

dump2:
	g++ -fdump-tree-all test2.h
