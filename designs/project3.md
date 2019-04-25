# Design Document for Project 3: File System

## Group Members

- Linus Kipkoech <mailto:kipkoechlinuss@berkeley.edu>
- Alex Yang <mailto:a.yang96@berkeley.edu>
- Haggai Kaunda <mailto:haggaikaunda@berkeley.edu>
- Matt Bayley <mailto:bayley@berkeley.edu>

# Design Document for Project 3: File Systems

## Task 1: Buffer cache

Introduce a new file called cache.c and cache.h

### Data Structures and functions

- cache.h (Owned by cache.c):
- We will maintain a hash table of cache blocks and each entry controls hash information

```C
struct cacheEntry
  {
	struct hash_elem elem; // Contribute space for hashtable
    struct hash_elem elem;
    block_sector_t sect_id; //Corresponding file block on disk
    size_t index; //Cache  block index in cache
    struct condition access_cond; // control per-entry cache access
    bool dirty; //If memory cache block has been modified.
  };
```

- Global variables in cache.c:

```C
struct cacheEntry list[NUM_BLOCK] /*With NUM_BLOCK=64, we will set up this 64-block buffer cache*/

struct hash cacheHash;//Cace hash table. Key is a disk sector ID. Entry is the one defined above*/

struct bitmap freebit[NUM_BLOCK];/* indicate if a cache block is free or not*/
struct bitmap usebit[NUM_BLOCK];/* indicate if a cache block is accessed recently. This is for second chance clock algorithm*/

struct lock cacheLock; //one lock for the entire in synchronization
struct condition cacheAccessQueue;//Allow threads to wait in a queue
```

- Functions to be add in cache.c are:

```C
/* Initialize  the write-back buffer cache. Create a thread which periodically writes dirty cache block to the disk */
void
cache_init (void)

/* Access a sector through buffer cache. If this sector is not in cache, load this section into a cache block. Returns the address of this cache block containing the desired sector content. */
void *
cache_get_block (block_sector_t sector)

/* Read a sector through buffer cache and copy data into a buffer*/
void
cache_read_block (block_sector_t sector, void *buf)

/* Writes a sector of data from a buffer to a disk sector through this
write-back buffer cache. Namely the actual write is delayed. The default size of Pintos sector size is 512 bytes */
void
buffer_cache_write (block_sector_t sector, void *buf)


/* A function executed by a background thread that periodically writes back dirty blocks. */
void
write_back_func (void *arg)
```

- Modification in inode.c

```C
/*Modify the following function to read data from the disk through the buffer cache. The current version reads directly from a disk*/
   /* This function reads SIZE bytes from INODE into BUFFER, starting  at position OFFSET. Returns the number of bytes actually read*/

off_t
inode_read_at (struct inode *inode, void *buffer_, off_t size, off_t offset)

/*  Modify the following function to write data to the disk through the buffer cache. The current version writes directly to disk.
The function semantic is to  write SIZE bytes from BUFFER into INODE, starting at OFFSET. Returns the number of bytes actually written*/

off_t
inode_write_at (struct inode *inode, const void *buffer_, off_t size,
                off_t offset)

/*Modify this inode create function to initialize the file header and write the inode to the disk through the buffer cache*/
bool
inode_create (block_sector_t sector, off_t length, bool isdir)

/* Modify this inode close function and write back and release cache block if needed*/
void
inode_close (struct inode *inode)
```

### Algorithms

- The cache_get_block() function uses a hash table to check if the corresponding disk block is in the cache or not. If not, allocate a free cache block entry and load data from disk. During allocation, we will use the second chance clock algorithm or LRU. The read-ahread strategy is to be integrated here to fetch a few data blocks in advance to improve the cache hit ratio.
- The cache_get_block function is simiar to above read function.
- The cache_write \_block() function uses a hash table to check if the corresponding disk block is in the cache or not. If not, allocate a free cache block entry and copy data from the user block, mark this block as dirty. During allocation, we will use the second chance clock algorithm or LRU.
- The background thread function write_back_func() runs an infinite loop and each loop sleeps for a while, then scan through all dirty cache blocks, write them to disk, and then reset these dirty bits as false. If disk blocks to write are consecutive, then multiple writes can be coalesced into a single disk operation.

### Synchronization

- We add a lock and a condition variable to synchronize all concurrent accesses to the buffer cache, and allow one thread at a time to modify the cache.

- For each entry of cache, we will also set up the conditional synchronization queue. So that the background thread that writes back the cache back, and the other concurrent threads which access the same block can be synchronized.

### Rationale

- We use the kernel provided hashmap implementation for fast cache block lookup.

- We put a lock with condition variable on the buffer cache, that simplifies many synchronization issues as discussed below.

### Q/A based on Section 3.1.2. (Topics for the design document)

- When one process is actively reading or writing data in a buffer cache block, how are other processes prevented from evicting that block?

  - Answer: We will use the second chance LRU replacement policy so that data blocks that are being actively accessed will stay in the cache. Also the synchronization is placed for accessing the overall cache system so that only thread can get into the critical section to manipulate cache meta data.

- During the eviction of a block from the cache, how are other processes prevented from attempting to access the block?

  - Answer: We will write out the disk content if a block needs to be evicted is dirty. Then remove this block key from the cache hash lookup table. Since the lock/condition variable synchronization is placed for accessing the overall cache system so that only thread can get into the critical section. The other threads that need to access this evicted block will not be able to access until the cache block replacement is completed.

- If a block is currently being loaded into the cache, how are other processes prevented from also loading it into a different cache entry? How are other processes prevented from accessing the block before it is fully loaded?

  - Answer: When the current block is being loaded into the cache, the other process cannot access the cache hash table due the critical section synchronization. Once it is done, the data block is accessible through the cache hash table. Thus the other processes will not load the same block to different cache blocks. The other threads have to wait in the conditional variable queue to access this block until the cache block loading is completed.

## Task 2: Extensible Files

### Data Structure and Functions

- The change in inode.h (owned by inode.c):
- Inode struct will be changed using the multi-level indexing similar to UNIX design.

```C
struct inode_disk
  {
    off_t ofs;       /* Offset of entry in parent directory. */
    bool isDir;      /* If this file is a directory, value is true */
    off_t fileLength;    /* File size in terms of bytes. */

    /* index structure for data blocks. */
    block_sector_t dirPointer [NUM_Direct];  /* Direct pointers. Num_Direct=128-7-21=100. We reserve 21 fields to be used later*/
    block_sector_t indirectPointer;         /* Indirect pointer. */
    block_sector_t doublyIndirect;     /* Doubly indirect pointer. */
    unsigned magic;                       /* Note: magic has a different offset now. */
    uint8_t unused[21];/* That makes 512 inode sector. These unused numbers will decrease as we may add more inode fields during development
  };
```

- Changes in inode.c

  - Remove struct inode_disk data from struct inode. That is because each file involves more than one on-disk inode blocks.

- The functions to be changed in inode.c:

  - Modify bool inode_create (block_sector_t sector, off_t length) to create the inode structure to support the desired initial file length.
  - Modify void inode_close (struct inode \*inode)to write the file blocks out through cache and free in-memory inode structure.
  - Modify off*t inode_read_at (struct inode *inode, void _buffer_, off_t size, off_t offset) to identify the correct logical sector block numbers to read, and then load the proper inodes to compute the physical sector IDs. Then use the buffer cache to fetch.
  - Modify off*t inode_write_at (struct inode *inode, const void _buffer_, off_t size, off_t offset) to identify the correct logical sector block numbers to write, and then load the proper inodes to find the physical sector IDs. Then use the buffer cache to write.
  - Add function block_sector_t inode_get_inumber ( struct inode \*inode) to return the inode number of the inode structure.
  - Add the following new function:

  ```C
   /* If this inode structure is for a directory, return true. Otherwise false. */
  bool
  inode_isdir (const struct inode *inode)
  ```

- Changes in free-map.c

  - Add struct lock freemapLock; //Synchronize access to the free-map.
  - Modify function “void free_map_init (void)to initize the lock.
  - Modify the following functions to add acquire_lock(&freemapLock) and release_lock (&freemapLock)
  - function “bool free_map_allocate (size_t cnt, block_sector_t \*sectorp)”
  - void free_map_release (block_sector_t sector, size_t cnt)
  - void free_map_open (void)
  - void free_map_create (void
  - Add new function “bool free_map_allocate1 (size_t cnt, block_sector_t \*sectorp)” to support the allocation of free blocks which may not be consecutive.
  - Add new function “void free_map_release (block_sector_t sector, size_t cnt)” to support the release of free blocks which may not be consecutive.

* The functions to be changed in syscall.c to support the new system call with 1 argument: “ int number(int fd)”

  - Modify void void syscall_check_argments( uint32_t\* args) to add code to check the argument for the system call SYS_INUMBER.
  - Modify void syscall_handleTask3cases( uint32_t* args, struct intr_frame *f ) to add a case to handle this system call by calling file_inumber ().

* The function to be changed in file.c to support the new system call “ int number(int fd)”

  - Add void uint32_t file_inumber (struct file \*file) which returns the inumber of a file structure using inode_inumber ().

### Algorithms

- Function off*t inode_read_at (struct inode *inode, void _buffer_, off_t size, off_t offset) needs to convert “offset” into the correct logical sector block numbers to read. If this offset is longer than the file length, it will just read the end of file. Then we compute the starting logical block ID, which is start=offset/BLOCK_SECTOR_SIZE.
  - If the offset is relative small (in this current design, start <=99), and it can be addressed using the direct block pointers, then we can just the information from this inode to translate.
  - If 100<=start <= 99+128, we have to load the indirect block pointer to obtain the physical sector ID.
  - If start >=100+128, we use the doubly indirect pointer.
- Function off*t inode_write_at (struct inode *inode, const void _buffer_, off_t size, off_t offset) involves a similar mapping process as above and we will consider code sharing during development.

### Synchronization

- We add a lock to data manipulation or access in free-map.c so that threads that access the free-map of sector blocks will be synchronized.

### Rationale

- We choose the UNIX-like multi-level inode structure with upto double indirect pointers. The justification is listed as follows.
  - Sector size=512 bytes. Need to support 8MB file (largest) which is 16,384 Sectors
  - Assume sector number takes 4 bytes, can store 128 in one sector. We also need to store other fields such as indirect pointer, and double indrector pointer. We assume 100 direct pointers can be used for now.
  - The maximum size in number of sectors with direct and single indirect block pointers is 100+ 128. That is not enough for 8MB size.
  - The max size with direct + single indirect + double indirect pointers is: 100 + 128+128\*128 = 16,612 sectors. That is sufficient to meet the maximum 8MB file size requirement.

### Q/A based on Section 3.1.2. Topics for the design document

- You are already familiar with handling memory exhaustion in C, by checking for a NULL return value from malloc. In this project, you will also need to handle disk space exhaustion. When your filesystem is unable to allocate new disk blocks, you must have a strategy to abort the current operation and rollback to a previous good state.

- Answer: In setting up the file length and allocating sectors, we will check if there is sufficient space. If not, the operation will return unsuccessful. In inode_write_at(), when the offset to write exceeds the file length, and there is no disk sector space to allocate extra sectors for this file, this write operation will return unsuccessful.
  We hope that meets the requirement, and otherwise we will learn more on the requirement of “rollback”.

## Task 3: Subdirectories (and system calls)

### Data structures and functions

- In thread.h (part owned by thread.c)

  - Add field struct dir \*cwd to the thread struct in thread.h. This will not only help us resolve
    relative paths but it will also track the current working directory of every thread.
    <pre>
    struct thread {
      ...
      struct dir *cwd;
      ...
     }
    </pre>

- In inode.c to the _struct inode_disk_
  - Add field bool isdir; to tell directories and files apart.
  - Add field block_sector parent; the sector representing the parent directory of the inode_disk.
    <pre>
    struct inode_disk {
      ...
      block_sector_t parent;
      bool isdir;
      ...
     }
     </pre>
- In filesys.c

  - Add struct dir_filename; This will be used when we need to find which directory a file belongs to.
    <pre>
    struct dir_filename {
      struct *dir;
      char *relative_file_name
     }
    </pre>
  - Add function _struct dir_filename \*get_dir_filename(const char path_name)_ to split file into it's directory
    and filename in it's directory. Further explanation below.
  - Modify functions _filesys_open()_, _filesys_create()_ and _filesys_remove()_ to use _get_dir_filename()_ to extract
    the directory that contains a given file.

- In file.c
  - Add function bool isdir(struct \*file) which will check it a given file is a directory.
- In directory.c

  - Add function bool mkdir(const char \*dir) to create a new directory.

### System Calls.

- In syscall.c
  - Modify the syscall handling routines to handle the new system calls. This will entail Modifying the folling
    funcitons: 1) syscall_handleTask3cases: to handle the new system calls. 2) syscall_check_argments: to check that the
    arguments of the new system calls are valid, 3) syscall_validate_ref: to validate the arguments pointer.
  - Add function bool syscall_chdir(const char \*dir). This will handle changing the cwd of a process.
  - Add function syscall_readdir(struct *file, char *name). This function will delegate the task of reading
    the the entries of a directory to dir_readir.
  - To handle the syscall mkdir and isdir, we will use the functions bool mkdir() which we define in directory.c. and
    isdir(struct \*file) which we define in file.c respectively.
  - As for the syscall inumber, since we keep track of all the files opened by a process, we have access to
    each inode corresponding to each opened file and so we use the sector number as the inode number.

### Algorithms

- To handle relative pathnames, we will track each processes curent working directory through the field
  struct dir \*cwd in the thread struct.
- We will use the function _get_dir_filename()_ to traverse the directory hierachy and resolve path names.
  This function will interprete each path name as having a directory_name part and a file_name part. eg if we have
  a path name given by 'a/b/c/d' then the directory_name part is 'a/b/c/' and the file name parth is 'd'.
  The function will work as follows.
  - It will use _get_next_part()_ which is provided in the specification to tokenize the path name one at a time.
  - If the path name contains special file names . and .., will use the current working directory of each thread
    and the parent directory(which can be extracted from the current working directory's inode_disk data) to resolve these
    special file names into directories.
  - Other wise we traverse the directory hierachy using dir_lookup().
  - On compeletion, this function will resolve path name into a struct dir represented by the directory_name part of the
    path and a char file_name represented by the file name part of the path. These two values will be returned as members of a
    struct dir_filename.

### Sunchronization

We will not allow a process to delete directory that's either the current working directory of or open by another process.
To enforce this, we will use the open_cnt in the struct inode. To enforce this, we will keep the current working directory of a
process open at any time.

### Rationale

When resolving the relative path names, we could have done this in syscall.c before calling any of the functions operate on files
and expect a file name as an argument. This would have meant that the syscall handler would be coupled with the file syste.
In that it would need to be aware of how the file system is structed. But by implementing the path resolution in filesys.c,
we can effectively decouple the system call handler from the file system. This design choice allows the file system structure to
change without the knowledge of the syscall handler.

### Q/A based on Section 3.1.2. Topics for the design document

- How will your filesystem take a relative path like ../my_files/notes.txt and locate the corresponding directory? Also, how will you locate absolute paths like /cs162/solutions.md?

  - Answer: From the correct thread structure, we can find the i-node of the current working directory. From the parent field of this i-node, we can find the parent i-node directory header. From there we can search through the hierarchy of the directory structure to find the i-node of directory “my-files. Then we can find the file name nodes.txt in the directory content.

- Will a user process be allowed to delete a directory if it is the cwd of a running process? The test suite will accept both “yes” and “no”, but in either case, you must make sure that new files cannot be created in deleted directories.
  - Answer: We will not allow the deletion of a work directory of some thread threads. We will make sure the new files cannot be created in deleted directories in filesys_create() and filesys_mkdir().
- How will your syscall handlers take a file descriptor, like 3, and locate the corresponding file or directory struct?

  - Answer: The file structure corresponding for a file descriptor contains a pointer to the i-node.

### Section 3.1.3. Additional Questions.

- For this project, there are 2 optional buffer cache features that you can implement: write-behind and read-ahead. A buffer cache with write-behind will periodically flush dirty blocks to the filesystem block device, so that if a power outage occurs, the system will not lose as much data. Without write-behind, a write-back cache only needs to write data to disk when (1) the data is dirty and gets evicted from the cache, or (2) the system shuts down. A cache with read-ahead will predict which block the system will need next and fetch it in the background. A read-ahead cache can greatly improve the performance of sequential file reads and other easily-predictable file access patterns. Please discuss a possible implementation strategy for write-behind and a strategy for read-ahead.

  - We plan to run a background thread in cache.c that periodically writes dirty cache blocks to the file system device.

  - In addition to the background low-profile writing, we will also conduct the disk writing when a dirty cache block needs to be evicted.

  - When fetching sector block data from disk to cache, we will conduct perfecting of 2 or more extra consecutive sector blocks from the same file. That should improve the sequential file access file to accomplish read-ahead.
