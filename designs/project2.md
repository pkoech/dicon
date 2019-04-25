Design Document for Project 2: User Programs
============================================

## Group Members
* Linus Kipkoech <kipkoechlinuss@berkeley.edu>
* Alex Yang <a.yang96@berkeley.edu>
* Haggai Kaunda <haggaikaunda@berkeley.edu>
* Matt Bayley <bayley@berkeley.edu>

Design Document for Project 2: User Programs
============================================

## Part 1: Argument Passing
Extend the implementation of process_execute (const char *file_name) with proper argument
passing.

### Data Structures and functions
The change in struct thread in thread.h (part owned by thread.c):
- Add a field struct PCB *pcbnode. If needed, each Pintos thread will manage a process
control block corresponding to a user process.
Create a new struct called PCB in process.h to manage control information of a process.
- pid_t pid; Process ID.
- bool loadSuccess; indicate if program loading is successful.
- struct list *childProcessList. If needed, each user process can have a list of child
processes. This list manages such information.
- Strict list_elem elem;//contribute this space to link a process list
- Struct semaphore loadSema; When the child process has loaded the binary program,
the parent can then return.

The functions to be changed in process.c:
- Modify tid_t process_execute (const char *file_name) so that it will wait on a semaphore
of the corresponding child process to check if the loading of program is successful.
- Modify static void start_process (void *file_name_) so that the arguments are properly
pushed onto the stack.
### Algorithms
In start_process (void *file_name_), we add the following actions
1. Split the input command
“file_name” by using strtokr() which can identify the next argument entry in the string.
2. Following Project 2 specification Section 3.1.9, we setup the user stack by pushing the
command and arguments to the stack, and then add stack entries with values as argc, argv[0],
argv[1], and so on.
3. Then notify the parent thread by sema_up(loadSema) indicating the
loading preparation is successful.

In process_execute (const char *file_name), for the child process has just created, we wait
using sema_down(loadSema)until the child thread issues sema_up(loadSema) and assess if
the loading is truly successful.

### Synchronization
We add a semaphore to let the child thread notify the parent if a loading of the binary program
is successful or not. Thus the parent should wait until the child completes the loading.

### Rationale
We strictly follow Project 2 specification Section 3.1.9 to implement the argument passing. The
original implementation does not consider a child thread may load program unsuccessfully and
thus adding a synchronization to capture this state seems to be a natural extension.

## Part 2: Process Control Syscalls
### Data Structure and Functions
The change in struct PCB in process.h (owned by process.c):
- Add a field int status; Record the exit statue value of a child process.
- Add a field struct semaphore exitSema . Indicate if this child process has exited. 0 intial
value so the parent will wait on this until the child increases its value.

The functions to be changed in process.c:
- Modify Int process_wait (tid_t child_tid) so that the parent only waits for a child process
with ID tid.
- Modify void process_exit (void) so that all child PCB resources and files opened are
released.

The functions to be changed in syscall.c:
- Modify void syscall_handler (struct intr_frame *f). We will add a checkup to see if the
input arguments are valid addresses. Then add code to implement the system calls halt,
exec, wait and practice.
- Add a new function bool check_addr(void *uaddr). It returns true if the address is valid
for this process. False if invalid.
### Algorithms

- Modify Int process_wait (tid_t child_tid) in process.c. We first find the PCB of this child.
Then wait for semaphore exitSema. Then collect the exit status to return. Before return,
we make sure the child resource is released.
- Modify void process_exit (void). We will release the space for the child process list and
also all file structures for the files opened.
- Modify void syscall_handler (struct intr_frame *f). We first add a checkup of arguments
for the following system calls:

   Void halt (void)

   pid_t exec (const char *file)

   int wait (pid_t pid)

   Int practice (int i)

   They involve either integer value or a pointer to a string. Thus we will use function check_addr().

   To implement halt(), we just call shutdown_power_off().

   To implement exec(), we call process_execute(). The return value is stored in f->;eax.

   To implement wait(), we use process_wait().

   To implement practice(), we simply increment the argument and place the return value to f->;eax.

- Add a new function bool check_addr(void *uaddr). To check if the address is valid for
this process, we use pagedir_get_page() in pagedir.c of folder userprog, is_user_vaddr()
in vaddr.h of folder threads.

### Synchronization
For system call exec(), we use process_execute() which lets the parent wait for the program
loading of a child process.

For system call wait(), we use process_wait() which implements the synchronization between a
child process and the parent.

### Rationale

To keep track of the child processes spawned during system call exec(), we use a list of PCBs
maintained by the parent. That simplifies the information sharing and control management.
## Part 3: File Operation Syscalls

#### Data Structures and Functions

Create a new struct called FCB in process.h to manage control information of a file opened.
int fdesc; -- File descriptor
char * fname; -- File name
struct file * fp – pointer to file structure based on Pintos file system.
struct list_elem FCBelem; // use this to join a file open list or hashtable


Changes in struct PCB  in process.h  (owned by process.c):
Add field struct hashtable * openFiles.  Each kernel thread with a user process maintains a hashtable of FCBs. The File descriptor is the key of the hashtable. We need syscalls to be able to look up file structs belonging to the user process by the file descriptor.

Changes in syscall.c:

Modify void syscall_handler (struct intr_frame * f). We will add a check to see if the input arguments are valid addresses. Then add code to pass arguments to syscall_create, syscall_remove, syscall_open, syscall_filesize, syscall_read, syscall_write, syscall_seek, syscall_tell, and syscall_close.

Add  new function  int add_FCB(struct file * fp) to add a newly opened file structure to the file control block hashtable.  Return a local file descriptor. The return value -1 means failed.
Add  new function  struct file * access_FCB(int fd) to  identify the  opened file structure  from the FCB list based on an integer file descriptor. Return the local file descriptor. The return value NULL  means failed.

<pre>
bool syscall_create (const char * file, unsigned initial_size) {
  // calls filesys_create();
}

bool syscall_remove (const char * file) {
  // calls filesys_remove();
}

int syscall_open (const char * file) {
  // calls filesys_open() and puts the file * in the process's FCB struct;
}

int syscall_filesize (int fd) {
  // calls file_length() returning the offset;
}

int syscall_read (int fd, void * buffer, unsigned length) {
  // call file_read() using the file struct associated with the file descriptor;
}

int syscall_write (int fd, const void * buffer, unsigned length) {
  // call file_write() using the file struct associated with the file descriptor;
}

void syscall_seek (int fd, unsigned position) {
  // call file_seek() with the file struct associated with the file descriptor;
}

unsigned syscall_tell (int fd) {
  // call file_tell() using the file struct associated with the file descriptor;
}

void syscall_close (int fd) {
  // call file_close() and remove the file struct pointer from the process's FCB hash table;
}

</pre>
#### Algorithms

All file io syscalls will begin by acquiring a filesystem level lock and end by releasing that lock.

For file descriptor based system calls, we use  access_FCB() to locate the file control block in the hashtable based on the input file descriptor, then use file_length() to implement system call filesize(), use file_read() to implement system call read(), use file_write() to implement system call write(), use file_seek() to implement system call seek(), use file_seek () to implement system call seek(), and use file_tell() to implement system call tell().


#### Synchronization

Following the suggestion from Section 1.3 of the Project 2 specification, we can use a global lock on the file system operations to ensure thread safety. Namely  acquire a lock in the beginning of file system call handling and release it at the end.

We also plan to use of file_deny_write() to ensure nobdy can modify the executable on disk when a user process is running. We use file_allow_write() to enable that after execution.

To avoid collisions between different user processes we give each process its own file struct for every open file, even if two processes are reading the same file. This way they aren't both moving the same offset variable.

#### Rationale

We feel that maintaining a hashtable of files opened internally provides an efficient look up the file struct associated with a given file descriptor and access to any state info needed for io operations.

Every file io syscall is essentially a lookup on this hash table for the relevant file struct and a call to an existing function in filesys.c or file.c.

Keeping the open file information for each user process separate also ensures that a user program can't make up their own integer file descriptions and interfere with files in use by other processes.

One concern we have is about how to use deny_write when multiple file structs exist for the same underlying physical file or inode. If we must correct this we could add a list struct to inode and a list_elem to file structs and maintain a list of file structs referencing each inode. A cleaner solution may be to move the deny_write property to the inode struct itself. It's hard to tell how far we are supposed to go in this direction when the next project is about more sophisticated synchronization in Pintos. We will ask about this in the design review.


## Additional Questions.
1. Test program Sc-bad- sp.c sets a bad address to stack pointer (%esp) in Line 18 and then
makes a system call. The process should terminate with -1 exit code.

2. Test program Sc-bad- arg.c sticks a system call number (SYS_EXIT) at the very top of the
stack, then invokes a system call with the stack pointer (%esp) set to its address in Line 14. The argument to the system call would be above the top of the user address space, and this process fails with -1 exit code.

3. The test directory does not contain test programs for the following system calls: remove(),
filesize(), seek(), tell(). We can check the file existence after remove() call. For filesize(), we can
simply check if the size 0 when a file is just created, and check if size is 4 after writing 4 bytes of
data. For seek(), we can write some bytes, and then relocate the seek position at beginning,
and check if the system can overwrite some bytes. For tell(), we can measure if a distance to
the beginning is still correct after using seek().

### GDB Questions:

#### 1
Qn: Set a breakpoint at process_execute and continue to that point. What is the name and address of the thread running this function? What other threads are present in pintos at this time? Copy their struct threads. (Hint: for the last part dumplist &all_list thread allelem may be useful.)

Ans:
Address of thread: 0xc000e000
Name: main
Other threads: idle thread. Struct thread given below.
0xc0104000 {tid = 2, status = THREAD_BLOCKED, name = "idle", '\000' <repeats 11 times>, stack = 0xc0104f34 "", priority = 0, allelem = {prev = 0xc000e020, next = 0xc0034b58 <all_list+8>}, elem = {prev = 0xc0034b60 <ready_list>, next = 0xc0034b68 <ready_list+8>}, pagedir = 0x0, magic = 3446325067}

#### 2.
Qn: What is the backtrace for the current thread? Copy the backtrace from gdb as your answer and also copy down the line of c code corresponding to each function call.

Ans:
#0  process_execute (file_name=file_name@entry=0xc0007d50 "args-none") at ../../userprog/process.c:32
#1  0xc002025e in run_task (argv=0xc0034a0c <argv+12>) at ../../threads/init.c:288
#2  0xc00208e4 in run_actions (argv=0xc0034a0c <argv+12>) at ../../threads/init.c:340
#3  main () at ../../threads/init.c:133


#### 3.
Qn: Set a breakpoint at start_process and continue to that point. What is the name and address of the thread running this function? What other threads are present in pintos at this time? Copy their struct threads.

Ans:
Current thread : "args-none\000\000\000\000\000\000"
Address: 0xc010a000

Other threads include: main and idle.

Struct thread of main: 0xc000e000 {tid = 1, status = THREAD_BLOCKED, name = "main", '\000' <repeats 11 times>, stack = 0xc000eebc "\001", priority = 31, allelem = {prev = 0xc0034b50 <all_list>, next = 0xc0104020}, elem = {prev = 0xc0036554 <temporary+4>, next = 0xc003655c <temporary+12>}, pagedir = 0x0, magic = 3446325067}

Struct thread of idle:  0xc0104000 {tid = 2, status = THREAD_BLOCKED, name = "idle", '\000' <repeats 11 times>, stack = 0xc0104f34 "", priority = 0, allelem = {prev = 0xc000e020, next = 0xc010a020}, elem = {prev = 0xc0034b60 <ready_list>, next = 0xc0034b68 <ready_list+8>}, pagedir = 0x0, magic = 3446325067}

#### 4.
Qn: Where is the thread running start_process created? Copy down this line of code.

Ans:
Thread created in function process_execute by code at line: 45│   tid = thread_create (file_name, PRI_DEFAULT, start_process, fn_copy);

#### 5.
Qn: Continue one more time. The user program should cause a page fault and thus cause the page fault handler to be executed. It’ll look something like [Thread ] #1 stopped. pintos-debug: a page fault exception occurred in user mode pintos-debug: hit ’c’ to continue, or ’s’ to step to intr_handler 0xc0021ab7 in intr0e_stub () Please find out what line of our user program caused the page fault. Don’t worry if it’s just an hex address. (Hint: btpagefault may be useful)

Ans: 0x0804870c

#### 6.
Qn: The reason why btpagefault returns an hex address is because pintos-gdb build/kernel.o only loads in the symbols from the kernel. The instruction that caused the page fault is in our user program so we have to load these symbols into gdb. To do this use load user symbols build/tests/userprog/args-none . Now do btpagefault again and copy down the results.

Ans:
#0  _start (argc=<error reading variable: can't compute CFA for this frame>, argv=<error reading variable: can't compute CFA for this frame>) at ../../lib/user/entry.c:9

#### 7.
Qn:  Why did our user program page fault on this line?
Ans: The page fault was caused because on attempting to read it’s arguments, function _start in lib/user/entry.c:9 encountered an error. It tried to access an invalid part of memory. This is because argument passing for user programs has not been implemented yet.
