FillingTheCache : CacheFuncs.o EvictionSet.o main.o
	cc -o FillingTheCache CacheFuncs.o EvictionSet.o main.o -l ssl -l gnutls -l gmp -l crypto -l m

main.o : main.c
	cc -c main.c

EvictionSet.o : EvictionSet.c
	cc -c EvictionSet.c

CacheFuncs.o : CacheFuncs.c
	cc -c CacheFuncs.c

clean :
	rm -f Cachefuncs.o EvictionSet.o main.o FillingTheCache
	
