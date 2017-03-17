all: minls minget

minls: minls.c minls.h libminCommon.a
	gcc minls.c -fPIC -o minls -L. -lminCommon  -Wall

minget: minget.c minget.h libminCommon.a
	gcc minget.c -fPIC -o minget -L. -lminCommon  -Wall

libminCommon.a: minCommon.c minCommon.h
	gcc -fPIC -c minCommon.c -Wall
	ar r libminCommon.a minCommon.o
	rm minCommon.o

clean:
	rm -f minls minget libminCommon.a
