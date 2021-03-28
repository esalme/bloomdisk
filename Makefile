default:
	gcc -O3 -c xxhash/xxhash.c -o xxhash.o
	gcc -O3 -c bloomdisk.c -o bloomdisk.o -lm -lpthread

test:
	gcc -O3 bloom_test.c xxhash.o bloomdisk.o -o bloom_test -lm -lpthread

clean:
	rm *.o
cleantest:
	rm bloom_test
	rm test.blm
	rm test.dat
