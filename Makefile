all: recordsize

recordsize:
	gcc -Wall -std=gnu99 -O0 -g -march=native -shared -fpic -I `gcc --print-file-name=plugin`/include -o recordsize.so \
	recordsize.c RecordInfo.c FieldInfo.c

test:
	g++ -fplugin=./recordsize.so test1.h

dump:
	g++ -fdump-tree-all test1.h
