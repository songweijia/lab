LFPATH=/home/weijia/workspace/libfabric/build4
CFLAGS=-I${LFPATH}/include -ggdb -DDEBUG -O0
# CFLAGS=-I${LFPATH}/include 
LDFLAGS=-L${LFPATH}/lib -Wl,-rpath=${LFPATH}/lib
LDLIBS=-lfabric -lm
PROGS=test lfperf

all: ${PROGS}

#test: test.c
#	gcc -o test ${CFLAGS} test.c ${LDFLAGS}

clean:
	rm -rf ${PROGS}
