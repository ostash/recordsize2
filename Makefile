CC = gcc
CXX = g++
CFLAGS = -Wall -std=gnu99 -O0 -g -march=native
PLUGININCLUDE = `$(CC) --print-file-name=plugin`/include

all: recordsize report

recordsize:
	$(CC) $(CFLAGS) -shared -fpic -I $(PLUGININCLUDE)  -o recordsize.so \
	rs-plugin-api.c rs-plugin.c rs-common.c

report:
	$(CC) $(CFLAGS) -o rs-report rs-report.c rs-common.c -liberty

test1:
	$(CXX) -fplugin=./recordsize.so -fplugin-arg-recordsize-process-templates -fplugin-arg-recordsize-print-all test1.h

test2:
	$(CXX) -fplugin=./recordsize.so -fplugin-arg-recordsize-process-templates -fplugin-arg-recordsize-print-all test2.h

dump1:
	$(CXX) -fdump-tree-all test1.h

dump2:
	$(CXX) -fdump-tree-all test2.h
