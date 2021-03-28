/*
gcc -O3 -c xxhash/xxhash.c -o xxhash.o
gcc -O3 -c bloom_disk/bloom_disk.c -o bloom_disk.o -lm -lpthread
gcc -O3 test_bloom_disk.c xxhash.o bloom_disk.o -o test_bloom_disk -lm -lpthread
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>
#include "bloomdisk.h"

struct bloomdisk bloom;

int main()  {
  int n = 4000000;
  int i = 0;
  int j = 0;
  int r = 0;
  char rawdata[32];
  FILE *urandom;
  urandom = fopen("/dev/urandom","rb");
  if(urandom == NULL) {
    exit(0);
  }
  r = bloomdisk_init(&bloom,1000,0.000001,"test");
  switch(r) {
    case 0: /* Failed */
      fprintf(stderr,"bloomdisk_init: Can't init\n");
      exit(0);
    break;
    case 1: /* Fresh file */
      for(i = 0 ; i < 256; i++) {
        memset(rawdata,i,32);
        bloomdisk_add(&bloom,rawdata,32);
      }
      bloomdisk_save(&bloom);
      printf("Added %"PRIu64 " items to the bloom\n",bloom.current_entries);
    break;
    case 2: /*Bloom was already made from other process*/
      printf("File alreade exist and have: %"PRIu64 " of %"PRIu64 " \nWith %"PRIu64 " internal collisions\n",bloom.current_entries,bloom.entries,bloom.collisions);
      /*
        you can check the current bloom->current_entries value to eval if it needs
        add more items or missing items.
      */
    break;
  }
  bloomdisk_print(&bloom);
  /*
    We performance a bloom test agints random data
  */
  for(i = 0 ; i < n; i++) {
    fread(rawdata,1,32,urandom);
    if(bloomdisk_check(&bloom,rawdata,32)) {
      j++;
    }
  }
  printf("Collision: %i/%i\n",j,n);
  bloomdisk_free(&bloom);
  return 0;
}
