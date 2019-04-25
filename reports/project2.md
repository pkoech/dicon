Final Report for Project 2: User Programs
=========================================

### Task 1
For Task 1, our change compared to the initial design is to add the following two lines in thread.h.
```C
struct thread  *parent;                /* Parent thread ID, so Child can find paraent*/
struct list childProcessList;  /* children process list. To be updated by thread_create()*/
```
The parent field stores the thread ID of the parent. That is something we learned from our meeting with the TA. In this way, the child thread (user process) can find the parent thread easily for the matching of process wait call executed by a parent and process exit called executed by a child.

The childProcessList keeps tracks the list of processes created by the corresponding process of this Pinto kernel thread. Initially we place under the PCB, but during the “main” thread initialization, we find the malloc produces an error, and thus we decide to have very basic information recorded for the main thread. To be consistent, we will keep track of child process list in the above field.

We have added a function call prepare_stack() to prepare the stack during the argument passing.

### Task 2
For Task 2,  we have added the following new fields in struct PCB in process.h in addition two features status and exitSema discussed in our earlier project design document.
```C
    struct list_elem elem;    /* Contribute the space for this children process list. */
   struct list fileOpenList;    /*List of files opened by this process */
    int  nextFileDesc;  /* next available file descriptor */
```
The fileOpenList keeps tracks what files have opened, and nextFileDesc keeps track of what number can be allocated  as a file descriptor for the file-related system calls.
For functions of Task 2,  we have added the following functions in addition to other functions discussed in the initial design report.
```C
struct PCB findChildPCB (pid_t pid) /* Locate the PCB data structure of a thread from the childProcessList*/

static void
prepare_stack (void file_name_, size_t flen, struct intr_frame ifp,  char argv[], size_t argc); /*To push the command string and pointers to the stack in process.c */

struct FCB *findFileStr(int fd); /* Find the corresponding internal file structure of a file descriptor in Syscall.c  */

int addFileOpenList(struct file *file_);/* Add an opened file to the file open list of each process */

void validate_mem (void *start, size_t size) /*Validate if the address of a memory buffer  is lega. We have sampled the 3 poins: beginning, middle, and end address*/

void validate_string (void *str) /*Validate if the address of a string is legal*/

void syscall_validate_ref( uint32_t* args) /*Validate data area address referenced in the arguments*/

void syscall_check_argments( uint32_t* args)  /*Validate arguments' addresses */

static bool syscall_handleTask2( uint32_t* args, struct intr_frame *f);/*Handle system calls in  Task 2*/
```
### Task 3
For Task 3,  added the following functions
```C
void syscall_handleTask3cases( uint32_t* args, struct intr_frame *f ) /*Handle system calls  defined in  Task 3*/
```






#### A reflection on the project – what exactly did each member do? What went well, and what could be improved?
We all worked together on the design and split up individually to work on seperate tasks. Eventually Alex merged everything together. We still need to cut back on procrastination.

###  Student Testing Report

#### seek-something() Test

```C
void
test_main (void) {
  int handle, byte_cnt;
  unsigned index;
  char buf[10];

  CHECK ((handle = open ("sample.txt")) > 1, "open \"sample.txt\"");
  index = 10;
  buf[0]= 'a';
  buf[1] = 'b';
  seek(handle,index);
  write(handle,buf,2);
  seek(handle,index+1);
  byte_cnt = read(handle, buf, 1);
  if (byte_cnt == 0)
    fail ("read() returned %d instead of 0", byte_cnt);
  if (buf[0] != 'b')
    fail ("seek() failed to move pointer");
}
```

#### Provide a description of the feature your test case is supposed to test.
We mainly test the following function
```C
 void seek (int fd, unsigned position);
```
which repositions the access pointer to the file content with a distance to the beginning of the file in bytes.

#### Provide an overview of how the mechanics of your test case work, as well as a qualitative description of the expected output.

This test positions the file access pointer at Position 10 using seek() function, and writes two characters ‘a’ and ‘b’. Then it positions the file access pointer at Position 11 using seek() again, and reads one byte. What Function read() fetches is expected to be ‘b’. If successful and the test prints “open sample.txt” and declares “pass”. If failed, the test will show “seek() failed to move pointer”.
#### Provide the output of your own Pintos kernel when you run the test case.
output
```C
Copying tests/userprog/seek-something to scratch partition...
Copying ../../tests/userprog/sample.txt to scratch partition...
qemu -hda /tmp/MlDSuDb8Xk.dsk -m 4 -net none -nographic -monitor null
PiLo hda1
Loading...........
Kernel command line: -q -f extract run seek-something
Pintos booting with 4,088 kB RAM...
382 pages available in kernel pool.
382 pages available in user pool.
Calibrating timer...  419,430,400 loops/s.
hda: 5,040 sectors (2 MB), model "QM00001", serial "QEMU HARDDISK"
hda1: 176 sectors (88 kB), Pintos OS kernel (20)
hda2: 4,096 sectors (2 MB), Pintos file system (21)
hda3: 105 sectors (52 kB), Pintos scratch (22)
filesys: using hda2
scratch: using hda3
Formatting file system...done.
Boot complete.
Extracting ustar archive from scratch device into file system...
Putting 'seek-something' into the file system...
Putting 'sample.txt' into the file system...
Erasing ustar archive...
Executing 'seek-something':
(seek-something) begin
(seek-something) open "sample.txt"
(seek-something) end
seek-something: exit(0)
Execution of 'seek-something' complete.
Timer: 69 ticks
Thread: 30 idle ticks, 38 kernel ticks, 1 user ticks
hda2 (filesys): 94 reads, 217 writes
hda3 (scratch): 104 reads, 2 writes
Console: 1001 characters output
Keyboard: 0 keys pressed
Exception: 0 page faults
Powering off...
```
result
```C
PASS
```
#### Identify two non-trivial potential kernel bugs, and explain how they would have affected your output of this test case.

1. If the kernel has a bug so that seek() implementation does not
move the file access offset correctly. Assume it does not change
the access pointer at all, then after writing 2 bytes with write() call
in this test, and the actual pointer will be at Position. The next call
read() will fetch the old content from the file, which would not
normally not match the expected value ‘b’.
2. If the kernel has a bug in seek implementation such that the seek()
function always resets the seek position to 0, then the read call()
will just get ‘a’ and place it into buf[0]. Again it will not match the
expected value ‘b’.

#### tell-check() Test
```C
void
test_main (void){
  int handle, byte_cnt;
  unsigned index, pos;
  char buf[10];

  CHECK ((handle = open ("sample.txt")) > 1, "open \"sample.txt\"");
  index = 10;
  buf[0]= 'a';
  buf[1] = 'b';
  seek(handle,index);
  pos = tell(handle);
  write(handle,buf,2);
  seek(handle,pos+1);
  byte_cnt = read(handle, buf, 1);
  if (byte_cnt == 0)
    fail ("read() returned %d instead of 0", byte_cnt);
  if (buf[0] != 'b')
    fail ("tell() failed ");
}
```
#### Provide a description of the feature your test case is supposed to test.

We mainly test the following function
unsigned tell (int fd);
which gives the next position of the file to access for read() or write() call.
#### Provide an overview of how the mechanics of your test case work, as well as a qualitative description of the expected output.
This test positions the file access pointer at Position 10 using seek() function, sets pos=tell(handle) which returns the next position for reading or writing,  and writes two characters ‘a’ and ‘b’. Then it positions the file access pointer at Position pos+1 using seek() again, and reads one byte. What Function read() fetches is expected to be ‘b’. If successful and the test prints “open sample.txt” and declares “pass”. If failed, the test will show “tell() failed ”.

####  Provide the output of your own Pintos kernel when you run the test case. Please copy the full raw output file from userprog/build/tests/userprog/your-test-1.output as well as the raw results from userprog/build/tests/userprog/your-test-1.result.
output
```C
Copying tests/userprog/tell-check to scratch partition...
Copying ../../tests/userprog/sample.txt to scratch partition...
qemu -hda /tmp/EszJ0Yj3XC.dsk -m 4 -net none -nographic -monitor null
PiLo hda1
Loading...........
Kernel command line: -q -f extract run tell-check
Pintos booting with 4,088 kB RAM...
382 pages available in kernel pool.
382 pages available in user pool.
Calibrating timer...  419,020,800 loops/s.
hda: 5,040 sectors (2 MB), model "QM00001", serial "QEMU HARDDISK"
hda1: 176 sectors (88 kB), Pintos OS kernel (20)
hda2: 4,096 sectors (2 MB), Pintos file system (21)
hda3: 105 sectors (52 kB), Pintos scratch (22)
filesys: using hda2
scratch: using hda3
Formatting file system...done.
Boot complete.
Extracting ustar archive from scratch device into file system...
Putting 'tell-check' into the file system...
Putting 'sample.txt' into the file system...
Erasing ustar archive...
Executing 'tell-check':
(tell-check) begin
(tell-check) open "sample.txt"
(tell-check) end
tell-check: exit(0)
Execution of 'tell-check' complete.
Timer: 58 ticks
Thread: 30 idle ticks, 27 kernel ticks, 1 user ticks
hda2 (filesys): 94 reads, 217 writes
hda3 (scratch): 104 reads, 2 writes
Console: 969 characters output
Keyboard: 0 keys pressed
Exception: 0 page faults
Powering off...
```
result
```C
PASS
```
####  Identify two non-trivial potential kernel bugs, and explain how they would have affected your output of this test case.

1. If kernel has a bug so that tell() function gives 0 instead of next
position before write() call. Then after writing, seek() will change
position as pos+1, which is 0. Next read() call will access position
1 and get some old content from the file, and normally it would
not match the expected value ‘b’.
2. If kernel’s bug incorrectly makes tell() function to return position
x-1 while the next position should x value. Then the access pointer
after seek call will be misaligned by 1, and the read call() will just
get ‘a’ and place it into buf[0]. Again it would not match the
expected value ‘b’.

#### Tell us about your experience writing tests for Pintos. What can be improved about the Pintos testing system? (There’s a lot of room for improvement.) What did you learn from writing test cases?
I learned how to write ck files and how to run tests individually and where to find the results.
It would be nice to put everything to do with tests under one folder so I do not have to jump around folders to write tests, run tests, and find test results.
