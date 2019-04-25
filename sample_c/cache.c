#include "filesys/cache.h"
#include <bitmap.h>
#include <hash.h>
#include <string.h>
#include "filesys/filesys.h"
#include "threads/thread.h"
#include "threads/malloc.h"
#include "threads/palloc.h"
#include "threads/synch.h"
#include "devices/timer.h"

#define TIME_DELAY 50000  //Latency for two consecutive lazy I/O writes
#define NUM_BLOCKS  64
/* P3 addition for handling cache buffer
 */
/*Define the cache hash table entry */
struct cacheEntry {
    block_sector_t sect_id;
    size_t index; //Cache block id inside cache
    struct condition access_cond;
    bool dirty;
};


void * cacheBlockAddr (size_t ); 
void cache_reset (void);
void cache_read (block_sector_t, void *);
void cache_write (block_sector_t , void *);
int cache_getmiss(void);
int cache_gethit(void);
void cache_reset(void);
int cache_replacement (void); 
void write_back_func (void *);


static char  cache_blocks[NUM_BLOCKS *BLOCK_SECTOR_SIZE]; /*Cache memory */
struct cacheEntry cacheInfoList[NUM_BLOCKS]; /*With NUM_BLOCK=64, we will set up this 64-block buffer cache*/
struct bitmap *refbit;/*indicate if a cache block is accessed recently. This is for second chance clock algorithm*/
struct bitmap *usebit;/*indicate if a cache block is being used by reader or writer recently. We synchronize */
int clock_position=0;                       /* Next position to scan for second-chance clock algorithm. */

struct lock cacheLock; //one lock for the entire cache for synchronization
struct condition cacheAccessQueue;//Allow threads to wait in a queue
int missCount=0;//Counter on cache miss so far
int hitCount=0;//Counter on cache hits so far


static bool cache_lookup (block_sector_t sectorno, struct cacheEntry **);

/* Returns the address of  i-th cache block. */
void *
cacheBlockAddr (size_t i) {
  return &cache_blocks[i*BLOCK_SECTOR_SIZE]; 
}

/* Periodic write-back data to the disk. */
void
write_back_func (void *aux UNUSED) {
  do {
    timer_msleep (TIME_DELAY);
    cache_flush_write();
  } while (true);
}


/* Cache setup initialization*/
void
cache_init (void)
{
  int i;
  clock_position = 0;
  refbit = bitmap_create (NUM_BLOCKS );
  usebit = bitmap_create (NUM_BLOCKS );
  for(i=0;i<NUM_BLOCKS;i++) {
	cacheInfoList[i].sect_id=-1;//indicate it is not used yet
  }
  lock_init (&cacheLock);
  cond_init (&cacheAccessQueue);
  thread_create ("WriteBackThr", PRI_MAX, write_back_func, NULL);
  missCount = 0;
  hitCount = 0;
}

/* Use this function for Project 3 testing.
   Clean up the cache and stats.  */
void
cache_reset (void)
{
  cache_flush_write ();

  bitmap_set_all (refbit, false);
  bitmap_set_all (usebit, false);
  size_t i;
  for(i=0;i<NUM_BLOCKS;i++) {
	cacheInfoList[i].sect_id=-1;//indicate it is not used yet
  }
  missCount = 0;
  hitCount = 0;
  clock_position = 0;
  lock_init (&cacheLock);
  cond_init (&cacheAccessQueue);
}


/*Return cache miss stats*/
int
cache_getmiss( void)
{
 return missCount;
}
/*Return cache hit stats*/
int
cache_gethit( void)
{
 return hitCount;
}

/* Get the address of memory block that holds the sector disk block. 
   If this sector is not in memory, allocate cache and load from disk. */
void *
cache_get_block (block_sector_t sector_no)
{
  struct cacheEntry *e;
  bool succ;

  lock_acquire (&cacheLock);
  succ = cache_lookup(sector_no, &e);
  lock_release (&cacheLock);

  if (!succ)
    block_read (fs_device, sector_no, cacheBlockAddr (e->index));

  return cacheBlockAddr (e->index);
}

/* Scan the cache and write out all direty blocks. */
void
cache_flush_write (void)
{
  int  i;
  struct cacheEntry *e;
  lock_acquire (&cacheLock);
  for (i=0; i < NUM_BLOCKS ; i++) {
    e=& cacheInfoList[i];
    if (e->dirty && !bitmap_test (usebit, i)) {
      	block_write (fs_device, e->sect_id, cacheBlockAddr (i));
        e->dirty = false;
    }
  }
  lock_release (&cacheLock);
}

/* Read a disk block possibly from cache and copy to into a buffer area*/
void
cache_read (block_sector_t sectorno, void *buf)
{
  void *addr = cache_get_block (sectorno);
  lock_acquire (&cacheLock);
  memcpy (buf, addr, BLOCK_SECTOR_SIZE);
  lock_release(&cacheLock);
}

/* Write disk block from a buffer area through cache */
void
cache_write (block_sector_t sectorno, void *buf)
{
  struct cacheEntry *e;

  lock_acquire (&cacheLock);
  cache_lookup (sectorno, &e);

  void *addr = cacheBlockAddr (e->index);
  memcpy (addr, buf, BLOCK_SECTOR_SIZE);
  int index=e->index;
  cacheInfoList[index].dirty = true;
  bitmap_mark (refbit, index); //mark it is referred for CLOCK second-chance algorithm
  bitmap_reset (usebit, index); //setup as false as we finish reading
  cond_signal (& cacheInfoList[index].access_cond, &cacheLock);
  cond_signal (&cacheAccessQueue, &cacheLock);
  lock_release (&cacheLock);
}

/*
Clock algorithm for cache block replacement 
*/
int cache_replacement (void) {
    int position;
    while (bitmap_test (refbit, clock_position) || bitmap_test (usebit, clock_position)) {
      bitmap_reset (refbit, clock_position);
      clock_position = (clock_position + 1) % NUM_BLOCKS ;
    }

    /* write old content to disk if dirty */
    position=clock_position;
    struct cacheEntry *old = &cacheInfoList[position];
    if (old->dirty) {
      block_write (fs_device, old->sect_id, cacheBlockAddr (old->index));
    }
    clock_position = (clock_position + 1) % NUM_BLOCKS ;
    
    return position;
}

/* Assume cacheLock is acquired.
   Find the cache entry for this sector number. If found, return true,
	and if missed, allocate space and return false*/ 
static bool
cache_lookup (block_sector_t sectorno, struct cacheEntry **entry)
{
  struct cacheEntry *e; 

  /* cacheLock is already acquired*/
  while (bitmap_all (usebit, 0, NUM_BLOCKS )){
     /* Wait if all the cache blocks are in use. */
    cond_wait (&cacheAccessQueue, &cacheLock);
  }
  int i=0, matchPos=-1;
  block_sector_t cur, freePos=-1;
  for(i=0;i<NUM_BLOCKS;i++) {
	 cur = cacheInfoList[i].sect_id; 
	if( cur == sectorno){
		matchPos=i;
		break;
	} else if (cur== freePos){ //Found a free spot
		freePos=cur;
	} 
	
  }

  if (matchPos != -1) {
    hitCount++;
    e=& cacheInfoList[matchPos];
  } else{
    missCount++;
   if((int) freePos== -1){
	//We have not found a free spot yet, do a block replacement */
	freePos=cache_replacement();
    }

    /* Initialize new entry. */
    e=& cacheInfoList[freePos];
    cond_init (&e->access_cond);
    e->sect_id = sectorno;
    e->dirty = false;
    e->index = freePos;
  }

  /* Wait for your turn to acquire entry. */
  while (bitmap_test (usebit, e->index))
    cond_wait (&e->access_cond, &cacheLock);
  bitmap_mark (usebit, e->index);
  bitmap_mark (refbit, e->index);

  *entry = e;
  return (matchPos!=-1);
}


/*Release the lock of individual cache entry*/
void
cache_lock_release (void *cacheAddr, bool dirty)
{
  //int index = (cache_block - cache_base) / BLOCK_SECTOR_SIZE;
  struct cacheEntry *e; 
  int index = (cacheAddr - cacheBlockAddr(0)) / BLOCK_SECTOR_SIZE;

  ASSERT (index >= 0 && index < NUM_BLOCKS );
 // ASSERT (bitmap_test (usebit, index)); 

  lock_acquire (&cacheLock);
  e=& cacheInfoList[index];
  bitmap_mark (refbit, index);
  bitmap_reset (usebit, index);
  if (dirty)
    e->dirty = true;
  cond_signal (&e->access_cond, &cacheLock);
  cond_signal (&cacheAccessQueue, &cacheLock);
  lock_release (&cacheLock);
}

