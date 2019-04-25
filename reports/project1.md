Final Report for Project 1: Threads
===================================
**_Alex Yang, Haggai Kaunda, Linus Kipkoech, Matt Bayley_**
## Task 1 Changes
We made no changes and followed the original design exactly
## Task 2 Changes
Our previous design gave a high level view of changes, we made a number of the concrete
changes to refine the previous design. We summarize the details of changes as follows.
#### Data Structure Changes
- In struct thread in thread.h, we have added a field base_priority to memorize the old priority
before donation while using the old field priority to represent the effective priority
We have added a field: struct lock *lock_waited to memorize the lock this thread waits for.
Add a field struct list *lock_held to memorize a list of locks this thread holds
- In struct lock of synch.h (part owned by thread.c), we have added a field struct list_elem
lockelem to contribute as an element of lock list in a thread. We add field int max_priority to
representthe maximum value of priority among threads waiting for this lock and this information
is updated when waiting thread information is updated.
#### Function Modifications
1. **We have changed the following functions in thread.c and describe design reasoning as follows:**
    - **Modify _next_thread_to_run()_** in thread.c to find next thread with the highest priority
when multi-feedback queue flag *thread_mlfqs* is not on. We use *list_max()* function to
select the thread with t the highest priority. Notice this considers the priorities of some
threads have been updated recently. Initially we have considered used a sorted list.
Namely keeping the ready list in an order of thread priority. We abandoned this design
because it is too expensive to reorganize the structure when a thread changes its
priority. In the other functions discussed below for *cond_signal()*, *sema_up()*, and
*lock_release()*, similarly we will all use list_max() to select a waiting thread with the
highest priority.
    - **Modify _void thread_set_priority (int new_priority)_**. Compared to our earlier design, we
have added more cases to handle. When the new base priority is lowered, but the
previous base priority dominates the effective priority, we have to consider the donated
priority from the held locks may influence the final effective priority. Thus we have
scanned the held locks to update the effective priority. We also check if this thread
priority affects others and if it should yield to a higher priority thread in the ready list.
For synchronization, we disable the interrupt so that only one thread can get into this
critical section to modify the ready list.
    - **Modify _void init_thread (struct thread *t, char *name, int priority)_**. We add the initiation
of thread structure regarding to priorities and the locks_held list/lock_waited.
    - **Modify _tid_t thread_create (const char *name, int priority, thread_func
*function, void *aux)_** to add *thread_yiedl()* at the end as the new thread addition may
change the priority order of ready tasks.

2. **We have the changed the following functions in synch.c:**
    - **Modify _void sema_up (struct semaphore *sema)_**
    Fetch a thread with the highest priority to wake up.
    - **Modify _void lock_acquire (struct lock *lock)_**
    We have followed the previous design to use *sema_try_acquire()* first. If this thread indeed
needs to wait, we have added priority donation by calling *lock_donate(lock, thisThread)*.
    - **Add _void lock_donate(struct lock *l, struct thread *t)_** to set the effective priority of the
lock holder as this thread waits. This is conducted recursively as the priority change of
the lock owner has a chain reaction to another lock it waits.
    - **Modify _void lock_release (struct lock *lock)_**
    To restore the priority of this thread as it releases a lock. Then another waiting thread with the
highest priority should be selected and the lock priority donation value is also changed. Finally
we will call *thread_yield()* to reexamine if a higher priority task should be scheduled.
    - **Modify _void cond_signal (struct condition *cond, struct lock *lock)_**. We follow the
previous design and use _list_max()_ to find a waiting thread with the highest priority.

## Task 3 Changes
Our previous design for Task 3 assumed an implementation of the mlfqs scheduler containing 64 literal queues and
the algorythm would manage these queues to run threads in the the ready queue. The major change we made to this
algorythm was flattening the 64 queues into one queue. This worked because at each scheduling decision, we only
cared about the thread with highest priority. So we could just get this thread by using list_max on the flattened
queue. Because the ready_list queue was a central part of implementing the mlfqs scheduler, changing it also meant
we needed to change all the function that assumed 64 priority ready lists. We outline the major changes below.

#### Data Structure Changes
- In thread.c we got ride of the list mlfqs_ready_list and used the original list provided in the skeleton code for
our ready list queue implementation. NOTE: the motivation for doing this is described above.

#### Function Modifications
- On choosing the next thread to run in _next_thread_to_run_ we used the list_max function to grab the thread with
with highest priority and poped this thread from _ready_queue_ before returning it. The code to handle this when
_thread_mlfqs_ flag was on remained unchanged.

- On adding a thread to the _ready_queue_ in _thread_add_ready_queue_ we simply pushed this thread to the end of the
_ready_queue_.

- To enforce round robin when we had a tie on priority, we modified _thread_tick to always yeild the current thread
when the current thread had exceeded it's allocated _TIME_SLICE_. This approach worked because the _thread_yeild_ function
pushed the running thread to the end of the _ready_queue_ and the _list_max_ function returns the first max value it
 encounters.

These were the main changes to task3 and notably, they made our design and implementation much easier to deal with. Thanks
to Eric for pointing us in the right direction during the Design review.




## Group Work Breakdown

Initially Linus did the design for task 1, Matt did the design for task 2, and Alex and Haggai did the design for task 3. We ended up making substantial changes to tasks 2 and 3. Matt ran into problems with his implementation of the modified task 2 and was unable to solve persistent segfault issues. Alex and Haggai came to the rescue and were able to bring their own version of task 2 up to speed at the last minute. For the next project we will be more aware of the difficulty of debugging in Pintos and will build and test smaller components of the project further ahead of time to avoid to issues like this.
