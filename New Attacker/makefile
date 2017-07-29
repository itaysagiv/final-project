MapTheCache : CacheFuncs.o CacheMapping.o
	gcc -o MapTheCache CacheFuncs.o CacheMapping.o

CacheMapping.o : CacheMapping.c
	gcc -c CacheMapping.c -o CacheMapping.o

CacheFuncs.o : CacheFuncs.c
	gcc -c CacheFuncs.c -o CacheFuncs.o

Clean:
	rm CacheFuncs.o CacheMapping.o
