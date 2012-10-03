all: recordsize

recordsize:
	gcc -Wall -std=gnu99 -O0 -g -march=native -shared -fpic -I `gcc --print-file-name=plugin`/include -o recordsize.so \
	recordsize.c

test:
	g++ -fplugin=./recordsize.so test1.h
