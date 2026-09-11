# Shared Counter with Multiple Threads

A minimal POSIX threads (pthreads) example. Eight threads each increment one
shared counter one million times. The program then joins all threads and compares
the final value against the expected total.

The program is written with the increment protected by a mutex, so it passes. To
observe the race condition the protection has to be removed — see
[Observing the race](#observing-the-race) below.

A second part then tests whether a self-written helper function is reentrant, and
shows the fix — see [Part 2](#part-2-reentrancy-test).

## How it works

### Shared data

```c
static long long counter = 0;
static pthread_mutex_t counter_mutex = PTHREAD_MUTEX_INITIALIZER;
```

All threads of a process share one address space, so a file-scope variable is a
single memory location that every thread reads and writes. `counter` is that
shared data; `counter_mutex` is the lock that guards it.

`PTHREAD_MUTEX_INITIALIZER` statically initializes a mutex with default
attributes. It can only be used for mutexes with static storage duration; a
dynamically allocated mutex needs `pthread_mutex_init()` instead.

### Thread function

```c
static void *worker(void *arg)
```

A pthread entry point must have exactly this signature: it takes `void *` and
returns `void *`. Here no argument is passed, so `(void)arg;` marks it
deliberately unused and silences the `-Wunused-parameter` warning.

### Creating and joining

`pthread_create(&threads[i], NULL, worker, NULL)` starts a new thread running
`worker` and stores its ID in `threads[i]`. The second argument (`NULL`) requests
default attributes; the fourth is the argument handed to `worker`.

`pthread_join(threads[i], NULL)` blocks the calling thread until thread `i`
terminates, and releases that thread's resources. Without the join loop, `main`
could return while workers are still running, terminating the whole process and
printing a meaningless number.

Note that the pthreads API does **not** set `errno`. Each function returns the
error code directly, which is why the code checks `ret != 0` rather than calling
`perror()`.

### Critical section

```c
pthread_mutex_lock(&counter_mutex);
counter++;
pthread_mutex_unlock(&counter_mutex);
```

`counter++` compiles to a three-step read-modify-write sequence:

```
load   R <- counter
add    R <- R + 1
store  counter <- R
```

That sequence is not atomic — the scheduler may suspend a thread between any two
steps. The mutex makes the region between lock and unlock a **critical section**:
at most one thread can be inside at a time, so no other thread can read `counter`
between one thread's load and its store.

`pthread_mutex_destroy()` at the end releases the mutex's resources.

## Build

```bash
gcc -Wall -Wextra -O0 -o counter counter.c -pthread
```

`-pthread` is needed at both compile and link time. It defines `_REENTRANT` and
links the threading runtime; plain `-lpthread` is not equivalent.

Use `-O0`. At `-O2` the optimizer can collapse the whole loop into a single
addition, which removes the per-iteration read-modify-write and hides the race in
the unprotected variant.

## Run

```bash
./counter
```

No command-line arguments. Thread count and iteration count are compile-time
constants:

```c
#define NUM_THREADS 8
#define NUM_LOOPS   1000000
```

Change them and rebuild to experiment.

## Results

### As written (mutex protected)

Three consecutive runs:

```
Expected counter value: 8000000
Actual counter value:   8000000
PASS: counter value is correct.
```

```
Expected counter value: 8000000
Actual counter value:   8000000
PASS: counter value is correct.
```

```
Expected counter value: 8000000
Actual counter value:   8000000
PASS: counter value is correct.
```

The result is correct on every run, and will stay correct regardless of thread
count, iteration count, or machine load.

### Observing the race

Comment out the two mutex calls in `worker`, leaving the bare increment:

```c
for (long i = 0; i < NUM_LOOPS; i++) {
    /* pthread_mutex_lock(&counter_mutex); */
    counter++;
    /* pthread_mutex_unlock(&counter_mutex); */
}
```

Rebuild and run three times:

```
Expected counter value: 8000000
Actual counter value:   6179155
FAIL: race condition detected.
```

```
Expected counter value: 8000000
Actual counter value:   6603756
FAIL: race condition detected.
```

```
Expected counter value: 8000000
Actual counter value:   6364094
FAIL: race condition detected.
```

Three properties of this output are worth noting:

1. The value is always **lower** than expected, never higher. A lost update can
   only discard an increment, never invent one.
2. The value **differs on every run** — here roughly 23%, 17%, and 20% of the
   increments were lost. This nondeterminism is the defining symptom of a race
   condition, and the reason such bugs are hard to reproduce under a debugger.
3. Every thread still executed all one million iterations. Nothing was skipped;
   the increments were lost in memory because two threads loaded the same value
   before either stored its result.
