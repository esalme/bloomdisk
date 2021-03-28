/*
 *  Copyright (c) 2021, Luis A. Gonzalez
 *  Copyright (c) 2012-2019, Jyri J. Virkki
 *  All rights reserved.
 *
 *  bloomdisk is bloom filter librery based on the work
 *  of libbloom of Jyri J. Virkki
 *
 *  This version is useful when you dont have enough ram to store the filter
 *  It can work really well on SSD or in NVMe for scientific applications :)
 *
 *  This version is made to work in file, instead of RAM
 *  - There are several differences from original version
 *
 *  See the README to see full changes and usage examples
 *
 *  This file is under BSD license. See LICENSE file.
 */

#ifndef _BLOOMDISK_H
#define _BLOOMDISK_H

#include <stdint.h>
#include <inttypes.h>
#include <pthread.h>

#ifdef __cplusplus
extern "C" {
#endif

/** ***************************************************************************
 * Structure to keep track of one bloom filter.  Caller needs to
 * allocate this and pass it to the functions below. First call for
 * every struct must be to bloom_init().
 *
 */
struct bloomdisk
{
  // These fields are part of the public interface of this structure.
  // Client code may read these values if desired. Client code MUST NOT
  // modify any of these.
  uint64_t entries;         /* Number total of allowed elements*/
  uint64_t current_entries; /* Number of elements already inserted*/
  uint64_t collisions;      /* Number of current collions*/
  uint64_t bits;            /* Number of bits effectively used  */
  uint64_t bytes;           /* Number of bytes of data file in disk*/
  uint8_t hashes;           /* Number of hash operations performed */
  long double error;
  // Fields below are private to the implementation. These may go away or
  // change incompatibly at any moment. Client code MUST NOT access or rely
  // on these.
  unsigned char ready;
  unsigned char major;
  unsigned char minor;
  long double bpe;
  char name[70];
  char filename_struct[70];
  char filename_data[70];
  FILE *fd_data;
  FILE *fd_struct;
  pthread_mutex_t threadmutex;
};

/** ***************************************************************************
 * Initialize the bloom filter for use.
 *
 * The filter is initialized with a bit field and number of hash functions
 * according to the computations from the wikipedia entry:
 *     http://en.wikipedia.org/wiki/Bloom_filter
 *
 * Optimal number of bits is:
 *     bits = (entries * ln(error)) / ln(2)^2
 *
 * Optimal number of hash functions is:
 *     hashes = bpe * ln(2)
 *
 * Parameters:
 * -----------
 *     bloom   - Pointer to an allocated struct bloom (see above).
 *     entries - The expected number of entries which will be inserted.
 *               Must be at least 1000 (in practice, likely much larger).
 *     error   - Probability of collision (as long as entries are not
 *               exceeded).
 *
 * Return:
 * -------
 *     2 - on success but the bloom data files already exist you need to skip
 *         the "add funtions" in based of your current_entries number
 *     1 - on success new fresh files, in blank just to be filled
 *     0 - on failure
 *
 */
int bloomdisk_init(struct bloomdisk * bloom, uint64_t entries,long double error,char *base_filename);


/** ***************************************************************************
 * Check if the given element is in the bloom filter. Remember this may
 * return false positive if a collision occurred.
 *
 * Parameters:
 * -----------
 *     bloom  - Pointer to an allocated struct bloom (see above).
 *     buffer - Pointer to buffer containing element to check.
 *     len    - Size of 'buffer'.
 *
 * Return:
 * -------
 *     1 - element is present (or false positive due to collision)
 *     0 - element is not present
 *    -1 - bloom not initialized
 *
 */
int bloomdisk_check(struct bloomdisk * bloom, const void * buffer, int len);


/** ***************************************************************************
 * Add the given element to the bloom filter.
 * The return code indicates if the element (or a collision) was already in,
 * so for the common check+add use case, no need to call check separately.
 *
 * Parameters:
 * -----------
 *     bloom  - Pointer to an allocated struct bloom (see above).
 *     buffer - Pointer to buffer containing element to add.
 *     len    - Size of 'buffer'.
 *
 * Return:
 * -------
 *     1 - element (or a collision) had already been added previously
 *         in case in this cas the collisions counter is incremented by one
 *     0 - element was not present and was added
 *    -1 - bloom not initialized
 *
 */
int bloomdisk_add(struct bloomdisk * bloom, const void * buffer, int len);


/** ***************************************************************************
 * Print (to stdout) info about this bloom filter. Debugging aid.
 */
void bloomdisk_print(struct bloomdisk * bloom);


/** ***************************************************************************
 *
 * Upon return, the bloom struct is no longer usable. You may call bloom_init
 * again on the same struct to reinitialize it again.
 *
 *  This funtions performe the internal file close, you MUST use this funtion
 *  at the end of your program
 *
 * Parameters:
 * -----------
 *     bloom  - Pointer to an allocated struct bloom (see above).
 *
 * Return: none
 *
 */
void bloomdisk_free(struct bloomdisk * bloom);

/** ***************************************************************************
 * This function is made to save the internal estructure in the bloom file.blm
 *
 * This funtion is useful when you are making a long bloom file
 * Call this funtion when you finish your "Adding" process
 *
 */
int bloomdisk_save(struct bloomdisk * bloom);


/** ***************************************************************************
 * Returns version string compiled into library.
 *
 * Return: version string
 *
 */
const char * bloomdisk_version();

#ifdef __cplusplus
}
#endif

#endif
