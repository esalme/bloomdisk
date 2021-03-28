# bloomdisk
Fast and simple bloom filter stored in disk not in RAM

Sometimes RAM is not enough, for some scientific applications we can use Disk (SSD, NVMe) are good options for this librery

This version is useful when you dont have enough ram to store the filter, it can work directly on SSD or in NVMe, if your dataset is not going to change for while this version is for you.

**bloomdisk** is based on the work libbloom of Jyri J. Virkki ( https://github.com/jvirkki/libbloom )

#### This version have several modifications

- functions **return 0** on fail and some different value on success
- Max number of bits is 2^64, Max number of bytes is 2^64 << 8
- Hash funtion is now xxhash instead of murmurhash2 and it returned a 64 bit number and is faster than the previous function
- pthread dependency to work with Mutex (Usefull to be multithread Safe) if you are going to wotk with a single thread then is OK remove the mutex funtions
- prefix name functions are bloomdisk instead of just bloom, this is to avoid confusions between the original funtions.

#### How to build

To build just type `make`
```
$ make
gcc -O3 -c xxhash/xxhash.c -o xxhash.o
gcc -O3 -c bloomdisk.c -o bloomdisk.o -lm -lpthread
```

And just add the files `xxhash.o` and `bloomdisk.o` to your project

##### Test example

To build the test program type `make test`
```
$ make test
gcc -O3 bloom_test.c xxhash.o bloomdisk.o -o bloom_test -lm -lpthread
```
And execute the `bloom_test` file
```
$ ./bloom_test
Added 256 items to the bloom
bloom disk test
 ->version = 0.1
 ->entries = 1000
 ->current_entries = 256
 ->collisions = 0
 ->error = 0.000001
 ->bits = 28755
 ->bits per elem = 28.755175
 ->bytes = 3595 (3 KB, 0 MB, 0 GB)
 ->hash functions = 20
Collision: 0/4000000
```

##### Where my data is stored?
There is a string parameter in the `bloomdisk_init` function that string will be the base name of your files example_

```
r = bloomdisk_init(&bloom,1000,0.000001,"test");
```
Thre are two files `.blm` and `.dat` for this example files will be `test.blm` and `test.dat`

```
$ ls -lh test*
-rw-rw---- 1 albertobsd albertobsd   84 Mar 28 05:11 test.blm
-rw-rw---- 1 albertobsd albertobsd 8.6K Mar 28 05:10 test.dat
```

the .blm file have part of  information related to the bloomdisk structure, the .dat file is the bloom filter data
