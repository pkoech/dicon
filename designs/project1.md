Design Document for Project 1: Threads
======================================

## Group Members
* Linus Kipkoech <kipkoechlinuss@berkeley.edu>
* Alex Yang <a.yang96@berkeley.edu>
* Haggai Kaunda <haggaikaunda@berkeley.edu>
* Matt Bayley <bayley@berkeley.edu>
# Task 1: Effecient Alarm Clock

## Data Structures and functions
The change in struct thread in thread.h (part owned by thread.c):
- Add a field as struct listelem sleepelem. It will be similar to the current list “allelem”,
except this is for maintaining the list of sleep threads waiting for specific timers.
- Add a field “int64_t deadline;” in this struct thread so that each sleep thread can be
marked with a time that should wake up.
Add global variable static struct list sleep_list in thread.c (next to ready_list). This is to
maintain a list of sleep threads.
The functions to be changed in timer.c:
- Modify void timer_sleep(int64_t ticks) to replace the busy checkup loop with a call to
Set_sleep_timer() which sets the current thread to wake up until “start + ticks” where
“start” is the current time.
The functions to be changed in thread.c:
- Modify void thread_init() to initialize the sleep list.
- Add void set_sleep_timer(int64_t deadline) to place this thread to the global thread
sleep list and mark the wakeup time which is the input argument “deadline”.
- Add void thread_wakeup(int64_t currentTime) to wake up a sleep thread which has
met the timer deadline and should wakeup.
- Modify void thread_tick(int64_t ticks) to add a call to thread_wakeup(ticks)

## Algorithms
Modify void thread_init() to initialize the sleep list with list_init (&amp;sleep_list)

set_sleep_timer(int64_t deadline) changes the state of the current thread from
THREAD_RUNNING to THREAD_BLOCKED and places it the sleep list.

The procedure is outlined as follows:
Disable interrupt
Setup the alarm time with the new deadline.
Add the current thread to the sleep list.
Call thread_block();
Enable interrupt

thread_wakeup(int64_t currentTime) will scan through the entire sleep_list to see if any of
sleep threads has a deadline the same as currentTime. Change the state of these selected
threads from THREAD_BLOCKED to THREAD_READY and move them from the sleep list to the
global thread ready list.

The procedure is outlined as follows:
Make sure interrupt is off
Scan each thread in the sleep list and for those threads whose alarm time meets the current
time tick, remove them from the sleep list and call thread_unblock();
Modify void thread_tick(int64_t ticks) to add a call to thread_wakeup(ticks) to check if waking
up some threads is necessary.

## Synchronization
Function set_sleep_timer() accesses and modifies the global shared thread sleep list, and thus
needs a synchronization to control this critical section. That is because multiple threads may
call timer_sleep() which triggers set_sleep_timer(). We will disable and enable the interrupt as
follows for the above purpose at the beginning and end of set_sleep_timer.

enum intr_level old_level = intr_disable ();
intr_set_level (old_level);

Function thread_wakup() accesses the global shared sleep list, thus we need to add a
synchronization to control this critical section. We will disable to enable the interrupt in
thread_wakeup() function. Since thread_tick() that calls thread_wakeup should have been
called during an interrupt and the interrupt is off already, we can be safe on this matter.

## Rationale
Options to implement timer_sleep():
- The current design for timer_sleep() is to go through a loop: check if the deadline is
met, and if not, yield (to be placed in the ready list with state THREAD_READY
instead of THREAD_RUNNING), and then recheck the time with the deadline again
after gaining the CPU cycle. This is a kind of busy-waiting. The weakness is that when
the deadline is not met, the system waits lots of CPU resource to checkup.
- Our design is not to place this sleep thread into the ready list. We create a new
sleep waiting list. In timer_sleep(), a sleep thread has its state changed from
THREAD_RUNNING to THREAD_BLOCKED. Later on, it is moved from the global thread sleep list to the global thread ready list only after it meets the deadline to
wake up.

In terms of the sleep list management. There are two options.
- One is to use the same list structure implementation as used for the ready list
management (from “lib/kernel/list.c”.)
- Another is to use a sorted list to reduce the time complexity in identifying threads
whose time deadlines are met. The sorted list seems to be provided by
“lib/kernel/list.c” and thus we will investigate this option during the project
development.


# Task 2: Priority Scheduler

## Data Structures and Functions
<pre>
struct thread {
    ...
    // added to hold the effective priority of the thread;
    int efpriority;
    ...
}

static void init_thread (struct thread * t, const char * namd, int priority) {
    ...
    // added to initialize efpriority as same as priority;
    t->efpriority = priority;
    ...
}

int thread_get_priority (void) {
    return thread_current()->efpriority;
}

// this will compare the efpriority of two threads and return ( a < b ) ;
bool list_less_func efpriority_less (list_elem a, list_elem b) {
}

void sema_up (struct semaphore * sema) {
    ...
    // instead of list_pop_front we will use list_max() with efpriority_less() to iterate and take the waiter with highest efpriority;
}

void donate_priority (struct thread * t, int donatedpriority) {
    if (donatedpriority > t->efpriority) {
        t->efpriority = donatedpriority;
        // we also move t to the back of whatever queue its on so any list iteration in progress will see its shiny new efpriority;
        }
}

void lock_release (struct lock * lock) {
    ...
    sema_up (sema);
    // here we donate priority from the highest priority waiter to the new lock holder;
}

// will yield CPU if this results in the priority lowering due to lock release or voluntary lowering;
void thread_set_priority (int p) {
    struct thread * t = thread_current();
    int original_priority = t->efpriority;
    t->priority = p;
    t->efpriority = p;
    if (p < original_priority) {
        thread_yield();
    }
}

void cond_signal (struct condition * cond, struct lock * lock UNUSED) {
    // instead of list_pop_front we will use list_max() with efpriority_less() to iterate and take the waiter with highest efpriority;
}

static struct thread * next_thread_to_run (void) {
    // instead of list_pop_front we will use list_max() with efpriority_less() to iterate and take the ready thread with highest efpriority;
}

</pre>
## Algorithms
To choose the next thread to run the scheduler we iterate through the linked list of ready threads (`ready_list`) to grab the thread with the highest `efpriority`. We can't keep this linked list ordered because any insertion or sort could be interrupted. Instead we will always iterate front to back and add any new members at the back so they will still be examined if an iterating thread is interrupted.

To acquire a lock, threads will call `lock_try_acquire`. If the lock is free the lock's holder will become the current thread. If not, the thread will be set to blocked and added to the linked list of threads waiting on the semaphore behind the lock (`semaphore.waiters`). If the thread's `efpriority` is higher than the `efpriority` of the thread holding the lock then it will donate.

To release a lock, we first lower the thread's `efpriority` back to `priority`. We iterate to find the highest `efpriority` `waiter` from the lock's semaphore. It won't need any donations because it has the highest `efpriority` among waiters. Only new waiters need to donate.

To implement priority scheduling for semaphores we need only modify `sema_up()` to iterate over all waiters from the front of the list to the back and take the one with highest `efpriority`.

To implement priority scheduling for condition variables we need only modify `cond_signal()` to follow the same behavior outlined above. We iterate over all waiters, taking the one with the maximum `efpriority`.

Changing a thread's priority or effective priority will result in the thread being moved to the back of the ready list or the wait list. This protects the thread-safeness of the list iterations used to select the thread with highest `efpriority`.

## Synchronization
Getting the maximum priority thread by iterating front to back and holding onto the thread with the maximum `efpriority` encountered should work. Any additions to the list or modifications to `efpriority` in the middle of the iteration will send the thread to the back of the list where it will be encountered later.

## Rationale
Our biggest question was whether to attempt to maintain sorted lists or not. Because lists aren't thread safe, we can't guarantee that insertions are atomic so the list may not always be sorted. Iterating through every thread in the list seems inefficient but in practice these lists shouldn't be too large and it's the only thread-safe option we could come up with. We still need to think more about exactly how to update priority and move threads to the back of these lists in a thread safe way but it seems quite doable.

# Task 3: Multi-level Feedback Queue Scheduler (MLFQS)

## Data Structures and Functions
<pre>

// Modifications to thread struct in thread.h
struct thread
  {
    /* Owned by thread.c. */
    ...
    fixed_point_t nice                  /* The nice value. */
    fixed_point_t recent_cpu            /* The recent_cpu estimate. */
    ...
}

// Modifications to thread.c
1) Global variables added to thread.c
___
#define NUM_MLFQ 64                               /* The number of Queues for the MLFQ scheduler.
static struct list mlfq_readylist       /* A list of ready threads mlfq_readylist, where each thread has priority between 0-63
//static int highest_queueno                        /* Instead of this use list_max to calculate highest thread
static fixed_point_t load_avg                     /* Average load estimation.
static int total_ready_threads;                   /*maintain the total number of ready threads in the system.
___

// Modifications to functions in thread.c
#begin#

void thread_init() {
...
  // set nice and recent_cpu values of the main thread; initialize load_avg to 0.
  initial_thread->nice = 0;
  initial_thread->recent_cpu = 0;
  load_avg = 0;

  // initialize the mlfq_readylist.
  for (int i = 0; i < NUM_MLFQ; i++) {
     list_init(&mlfq_readylist[i]);
  }
...
}

void init_thread((struct thread *t, const char *name, int priority) {
...
  // nice and recent_cpu of new thread are inherited from parent.
  t->nice = thread_get_nice();
  t->recent_cpu = thread_get_recent_cpu();
...
}

void thread_tick(void) {
  // update priority field of running thread and other load parameters. See Algorithm for details.
}

void thread_set_nice(int nice) {
  thread_current()->nice = nice;
}

int thread_get_nice (void) {
  return thread_current()->nice;
}

int thread_get_recent_cpu (void) {
  return thread_current()->recent_cpu;
}

int thread_get_load_avg (void) {
  return load_avg;
}

static struct thread * next_thread_to_run (void) {
  // use mlfq_ready list when mlfq flag is on.
}

void thread_unblock (struct thread *t) {
  // Call thread_add_readyqueue() to add a thread to a ready queue or MLFQ. Update total_ready_threads
}

void thread_yield (void) {
  // Call thread_add_readyqueue() to add a thread to a ready queue or MLFQ. Update total_ready_threads
}
void thread_exit (void) {
  // Update total_ready_threads
}
#end


// Functions to Add to thread.c
...
void thread_add_readyqueue (struct thread *t) {
  // add t to a mlfq_readylist if mlfq flag is on. Other wise add to regular ready list.
}
...
 </pre>

## Algorithms
- **Choosing the next thread to run (to be implemented in function _next_thread_to_run_)**
  - When thread_mlfqs flag is true, do:
    - use list_max on the thread attribute "priority" to get the thread with highest priority.
    - Using this, select the highest priority thread, and move to the tail of the list. This will ensure Round Robin
  - If thread_mlfqs flag is false, do:
    - use code default handling of ready queue. Since Scheduler will either use Advanced Scheduling with MLFQS Algorythm
      or Priority Scheduling, we will default to Priority Scheduling when thread_mlfqs flag is not set.


- **Adding a thread to a Ready Queue**
  We will add a new function _void thread_add_readyqueue(struct thread *t) to handle adding a thread to a ready queue depending on whether
  thread_mlfqs flag is set or not.
    - When thread_mlfqs flag is set, do:
      - calculate priority for thread struct
      - Add thread to the list at mlfqs_readylist
    - Otherwise revert to default way to add thread to a readyqueue.

- **Computing values needed for Advanced Scheduler**
  This will be implemented in function _thread_tick_ which we will modify to function as follows:
  - On each timer tick, Increment the current running thread's recent_cpu by 1.
  - On every fourth tick, recompute the priority of every thread in mlfqs_readylist, Adjusting priorities that either go
    negative or greater than PRI_MAX. And setting them to 0 or PRI_MAX respectively.
  - Once per second, recompute _recent_cpu_ and update priority for each thread based on new recent_cpu value. Additionally
    recompute the _load_avg_.

  These values are computed using the formulas provided in the spec which we list below for completeness.
    **priority formula(every fourth tick):** _priority = PRI_MAX − (recent_cpu/4) − (nice × 2)_
    **load_avg formula(Every second):** _load_avg = (59/60) × load_avg + (1/60) × ready_threads_
    **recent_cpu formula(Every second)** _recent_cpu = (2 × load_avg)/(2 × load_avg + 1) × recent_cpu + nice_

## Synchronization
Since we add the global variable highest_queueno and mlfq_readylist to the existing code where the
original ready list is being accessed, the existing code already handles race condition prevention using interrupt disabling surrounding this code segment. Thus, we do not need to add extra synchronization functionality.

For the modification to thread_tick(), it is called by an interrupt handling, and thus we should
be safe to assume no other interrupt can enter and we can access mlfq queues without
worrying about new synchronization issues.

## Rationale
To find the highest priority queue that is not empty, one option is to let the system scan the top portion of the multi-level queues which is expensive.
Another option is to maintain a global variable which keeps track of the highest queue that has a thread.
Another option is to use the max function to get the thread with highest priority from the list and move the thread to the tail of the list to ensure round robin.
We use the third option because it's cleaner and easier to implement.

Another thing we have to do is calculate the number of ready threads for the load_avg formula.
One option is to scan the entire ready list, which is expensive. We take the second option to maintain a global variable called total_ready_threads which keeps a counter of the total number of ready threads for us.

In terms of list management for MLFQ ready lists, three possible implementations are 1) a
general list; 2) a sorted list with `list_insert_ordered(); 3) a maximum list with `list_max()`.
The basic data structures are implemented in “lib/kernel/list.c”.
Since we need to find the thread with the highest priority, we plan to use the max list so
that the underlying data structure does not need to be sorted. That saves the insert cost.

# Additional Questions

## 1. Base Priority vs Effective Priority Bug
<pre>
We call thread_create() 3 times
    pthread_id = A, Base Priority = 30, Effective Priority = 30
    pthread_id = B: Base Priority = 20, Effective Priority = 20
    pthread_id = C: Base Priority = 10, Effective Priority = 10
A donates priority to C
    pthread_id = C: Base Priority = 10, Effective Priority = 30
B and C sleep 1 second
A enters critical section and sleeps 2 seconds.
B and C wait to enter critical section
    pthread_id = B: Base Priority = 20, Effective Priority = 20
    pthread_id = C: Base Priority = 10, Effective Priority = 30
A exits critical section

---------------------------------------------------------
  EXPECTED CONTROL FLOW   |   ACTUAL CONTROL FLOW
C enters, prints, waits   | B enters, prints, waits
B enters, prints, waits   | C enters, prints, waits
                          |
      EXPECTED OUTPUT     |       ACTUAL OUTPUT
C                         | B
B                         | C
---------------------------------------------------------
</pre>
## 2.
Threads A, B, and C have nice values 0, 1,2. We update thread priority as PRI_MAX −
(recent_cpu/4) − (nice × 2). When a thread’s priority drops, it needs to yield to the one in the
ready queue with the larger or same priority in the round-robin order.
The per second formula for recent_cpu update will not be triggered because TIMER_FREQ is 100.
![](http://i1191.photobucket.com/albums/z461/marinebomber275/Screen%20Shot%202018-02-14%20at%2010.23.26%20PM.png)

## 3.
The spec does not say clearly how to choose threads in the same priority in a &quot;round-
robin&quot; order if there are multiple threads in a queue. To simplify the implementation, we put
the current thread to the tail of a ready queue and select the next thread from the queue in a
FIFO mode.
