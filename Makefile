CC = gcc
CXX = g++
CFLAGS = -Wall -std=gnu99 -O0 -g -march=native
CXXFLAGS = -Wall -O0 -g -march=native
PLUGININCLUDE = `$(CC) --print-file-name=plugin`/include

help:
	@echo "Please specify target that corresponds to your GCC version."
	@echo "Available targets: gcc45 gcc46 gcc47 gcc48 gcc49"

clean:
	rm -fr test{1,2}.h.gch recordsize.so rs-report

gcc45: recordsize_c report
gcc46: recordsize_c report
gcc47: recordsize_c report
gcc48: recordsize_cpp report
gcc49: recordsize_cpp report

recordsize_c:
	$(CC) $(CFLAGS) -shared -fpic -I $(PLUGININCLUDE)  -o recordsize.so \
	rs-plugin-api.c rs-plugin.c rs-common.c

recordsize_cpp:
	$(CXX) $(CXXFLAGS) -shared -fpic -I $(PLUGININCLUDE)  -o recordsize.so \
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
