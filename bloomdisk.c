/*
 *  Copyright (c) 2021, Luis A. Gonzalez
 *  Copyright (c) 2012-2019, Jyri J. Virkki
 *
 *  bloomdisk is bloom filter librery based on the work
 *  of libbloom of Jyri J. Virkki
 *
 *  This version is useful when you dont have enough ram to store the filter
 *  It can work really well on SSD or in NVMe for scientific applications :)
 *
 *  This version is made to work in file, instead of RAM
 *  - There are several differences from original version
 *  ++ functions return 0 on fail and some different value on success
 *  ++ Max number of bits is 2^64, Max number of bytes is 2^64 << 8
 *  ++ Hash funtion is now xxhash instead of murmurhash2 and it returned
 *     a 64 bit number and is faster than the previous function
 *  ++ pthread dependency to work with Mutex (Usefull to be multithread Safe)
 *     if you are going to wotk with a single thread then is OK remove the mutex funtions
 *  ++ prefix name functions are bloomdisk instead of just bloom, this is
 *     to avoid confusions between the original funtions
 *
 */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <inttypes.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "bloomdisk.h"
#include "xxhash/xxhash.h"

#define MAKESTRING(n) STRING(n)
#define STRING(n) #n
#define BLOOM_MAGIC "bloomdisk"
#define BLOOM_VERSION_MAJOR 0
#define BLOOM_VERSION_MINOR 1

#define ONEMB 1048576

static int bloomdisk_test_bit_set_bit(FILE *fd, uint64_t bit, int set_bit)  {
  unsigned int byte = bit >> 3;
  unsigned char c;
  unsigned char mask;
  fseek(fd,byte,SEEK_SET);
  fread(&c,1,1,fd);
  mask = 1 << (bit % 8);
  if (c & mask) {
    return 1;
  } else {
    if (set_bit) {
      c = c | mask;
      fseek(fd,byte,SEEK_SET);
      fwrite(&c,1,1,fd);
    }
    return 0;
  }
}

static int bloomdisk_check_add(struct bloomdisk * bloom, const void * buffer, int len, int add) {
  if (bloom->ready == 0) {
    printf("bloom at %p not initialized!\n", (void *)bloom);
    return -1;
  }
  unsigned char hits = 0;
  uint64_t a = XXH64(buffer, len, 0x59f2815b16f81798);
  uint64_t b = XXH64(buffer, len, a);
  uint64_t x;
  int r;
  unsigned char i;
  for (i = 0; i < bloom->hashes; i++) {
    x = (a + b*i) % bloom->bits;
    //printf("Offset: %lu\n", x >> 3);
    pthread_mutex_lock(&bloom->threadmutex);
    r = bloomdisk_test_bit_set_bit(bloom->fd_data, x, add);
    pthread_mutex_unlock(&bloom->threadmutex);
    if (r) {
      hits++;
    } else if (!add) {
      // Don't care about the presence of all the bits. Just our own.
      return 0;
    }
  }
  if (hits == bloom->hashes) {
    return 1;                // 1 == element already in (or collision)
  }
  return 0;
}

int bloomdisk_init(struct bloomdisk * bloom, uint64_t entries, long double error, char *base_filename) {
  char *buffer;
  int rwed;
  int r = 0;
  uint8_t ready;
  uint64_t entries_l;
  uint64_t bits;
  uint64_t bytes;
  uint8_t hashes;

  long double num;
  long double denom;
  long double dentries;
  long double allbits;
  char byte;
  memset(bloom, 0, sizeof(struct bloomdisk));
  if (entries < 1000 || error <= 0 || error >= 1) {
    return 0;
  }

  snprintf(bloom->filename_struct,69,"%s.blm",base_filename);
  snprintf(bloom->filename_data,69,"%s.dat",base_filename);
  snprintf(bloom->name,69,"%s",base_filename);

  bloom->current_entries = 0;
  bloom->collisions = 0;
  bloom->entries = entries;
  bloom->error = error;
  num = -log(bloom->error);
  denom = 0.480453013918201; // ln(2)^2
  bloom->bpe = (num / denom);
  dentries = (long double) entries;
  allbits = dentries * bloom->bpe;
  bloom->bits = (uint64_t)allbits;
  bloom->bytes = (uint64_t) (bloom->bits / 8);
  if (bloom->bits % 8) {
    bloom->bytes++;
  }
  //printf("calculated bytes: %"PRIu64 "\n",bloom->bytes);
  bloom->hashes = (unsigned char)ceil(0.693147180559945 * bloom->bpe);  // ln(2)
  bloom->major = BLOOM_VERSION_MAJOR;
  bloom->minor = BLOOM_VERSION_MINOR;

  bloom->fd_struct = fopen(bloom->filename_struct,"r+b");

  if(bloom->fd_struct == NULL)  { /* New File*/
    bloom->fd_struct = fopen(bloom->filename_struct,"a+b");
    bloom->fd_data = fopen(bloom->filename_data,"a+b");
    if(bloom->fd_struct == NULL)  {
      fprintf(stderr,"bloomdisk_init: cant open file: %s\n",bloom->filename_struct);
      return 0;
    }
    if(bloom->fd_data == NULL)  {
      fprintf(stderr,"bloomdisk_init: cant open file: %s\n",bloom->filename_data);
      return 0;
    }
    buffer = malloc(ONEMB);
    if(buffer == NULL)  {
      fprintf(stderr,"bloomdisk_init: error malloc()\n");
      return 0;
    }
    memset(buffer,0,ONEMB);
    bytes = bloom->bytes;
    //printf("calculated bytes: %"PRIu64 "\n",bytes);
    rwed = 1;
    while(bytes != 0 && rwed) {
      if(bytes >= ONEMB )  {
        rwed = fwrite(buffer,ONEMB,1,bloom->fd_data);
        bytes-= ONEMB;
      }
      else  {
        rwed = fwrite(buffer,bytes,1,bloom->fd_data);
        bytes-=bytes;
      }
      //printf("calculated bytes: %"PRIu64 "\n",bytes);
    }

    if(rwed != 1)  {
      fprintf(stderr,"bloomdisk_init: cant Initialize the file: %s\n",bloom->filename_data);
      return 0;
    }
    r = 1;
  }
  else  { /* File Already exist */
    bloom->fd_data = fopen(bloom->filename_data,"r+b");
    if(bloom->fd_data == NULL)  {
      fprintf(stderr,"bloomdisk_init: cant open file: %s\n",bloom->filename_data);
      return 0;
    }
    rwed = 1;
    rwed &= fread(&ready,sizeof(uint8_t),1,bloom->fd_struct);
    /*
    printf("Current rwed value %i\n",rwed);
    printf("readed ready: %i\n",ready);
    */
    rwed &= fread(&entries_l,sizeof(uint64_t),1,bloom->fd_struct);
    /*
    printf("Current rwed value %i\n",rwed);
    printf("readed entries: %"PRIu64 " \n",entries_l);
    */
    rwed &= fread(&bloom->current_entries,sizeof(uint64_t),1,bloom->fd_struct);
    /*
    printf("Current rwed value %i\n",rwed);
    printf("readed current entries: %"PRIu64 " \n",bloom->current_entries);
    */
    rwed &= fread(&bloom->collisions,sizeof(uint64_t),1,bloom->fd_struct);
    /*
    printf("Current rwed value %i\n",rwed);
    printf("readed collisions: %"PRIu64 " \n",bloom->collisions);
    */
    rwed &= fread(&bits,sizeof(uint64_t),1,bloom->fd_struct);
    /*
    printf("Current rwed value %i\n",rwed);
    printf("readed bits: %"PRIu64 " \n",bits);
    */
    rwed &= fread(&bytes,sizeof(uint64_t),1,bloom->fd_struct);
    /*
    printf("Current rwed value %i\n",rwed);
    printf("readed bytes: %"PRIu64 " \n",bytes);
    */
    rwed &= fread(&hashes,sizeof(uint8_t),1,bloom->fd_struct);
    /*
    printf("Current rwed value %i\n",rwed);
    printf("readed hashes: %i\n",hashes);
    */
    if(!rwed) {
      fprintf(stderr,"bloomdisk_init: something is wrong reading the previos values: %s\n",bloom->name);
      return 0;
    }
    if( entries_l != bloom->entries || bits != bloom->bits || bytes != bloom->bytes || hashes != bloom->hashes) {
      fprintf(stderr,"bloomdisk_init: bloom filter in file missmach the current setyp: %s\n",bloom->name);
      return 0;
    }
    r = 2;
  }
  bloom->ready = 1;
  return r;
}

int bloomdisk_check(struct bloomdisk * bloom, const void * buffer, int len) {
  return bloomdisk_check_add(bloom, buffer, len, 0);
}

int bloomdisk_add(struct bloomdisk * bloom, const void * buffer, int len) {
  int r =  bloomdisk_check_add(bloom, buffer, len, 1);
  pthread_mutex_lock(&bloom->threadmutex);
  bloom->current_entries++;
  if(r == 1)  {
    bloom->collisions++;
  }
  pthread_mutex_unlock(&bloom->threadmutex);
  return r;
}

void bloomdisk_print(struct bloomdisk * bloom)  {
  printf("bloom disk %s\n",bloom->name);
  if (!bloom->ready) { printf(" *** NOT READY ***\n"); }
  printf(" ->version = %d.%d\n", bloom->major, bloom->minor);
  printf(" ->entries = %"PRIu64"\n", bloom->entries);
  printf(" ->current_entries = %"PRIu64"\n", bloom->current_entries);
  printf(" ->collisions = %"PRIu64"\n", bloom->collisions);
  printf(" ->error = %Lf\n", bloom->error);
  printf(" ->bits = %"PRIu64"\n", bloom->bits);
  printf(" ->bits per elem = %Lf\n", bloom->bpe);
  printf(" ->bytes = %"PRIu64, bloom->bytes);
  unsigned int KB = bloom->bytes / 1024;
  unsigned int MB = KB / 1024;
  unsigned int GB = MB / 1024;
  printf(" (%u KB, %u MB, %u GB)\n", KB, MB,GB);
  printf(" ->hash functions = %d\n", bloom->hashes);
}

/*
int bloomdisk_reset(struct bloomdisk * bloom)
{
  if (!bloom->ready) return 0;
  memset(bloom->bf, 0, bloom->bytes);
  return 1;
}
*/

int bloomdisk_save(struct bloomdisk * bloom) {
  if(bloom->ready)  {
    fseek(bloom->fd_struct,0,SEEK_SET);
    fwrite(&bloom->ready,sizeof(uint8_t),1,bloom->fd_struct);
    fwrite(&bloom->entries,sizeof(uint64_t),1,bloom->fd_struct);
    fwrite(&bloom->current_entries,sizeof(uint64_t),1,bloom->fd_struct);
    fwrite(&bloom->collisions,sizeof(uint64_t),1,bloom->fd_struct);
    fwrite(&bloom->bits,sizeof(uint64_t),1,bloom->fd_struct);
    fwrite(&bloom->bytes,sizeof(uint64_t),1,bloom->fd_struct);
    fwrite(&bloom->hashes,sizeof(uint8_t),1,bloom->fd_struct);
  }
}


void bloomdisk_free(struct bloomdisk * bloom) {
  if(bloom->ready)  {
    fseek(bloom->fd_struct,0,SEEK_SET);
    fwrite(&bloom->ready,sizeof(uint8_t),1,bloom->fd_struct);
    fwrite(&bloom->entries,sizeof(uint64_t),1,bloom->fd_struct);
    fwrite(&bloom->current_entries,sizeof(uint64_t),1,bloom->fd_struct);
    fwrite(&bloom->collisions,sizeof(uint64_t),1,bloom->fd_struct);
    fwrite(&bloom->bits,sizeof(uint64_t),1,bloom->fd_struct);
    fwrite(&bloom->bytes,sizeof(uint64_t),1,bloom->fd_struct);
    fwrite(&bloom->hashes,sizeof(uint8_t),1,bloom->fd_struct);
    fclose(bloom->fd_data);
    fclose(bloom->fd_struct);
  }
  memset(bloom, 0, sizeof(struct bloomdisk));
}

const char * bloomdisk_version()
{
  return MAKESTRING(BLOOM_VERSION);
}
