#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sched.h>
#include <pthread.h>

#define NUM_THREADS   8
#define NUM_LOOPS     1000000
#define REENT_LOOPS   100000
#define BUF_SIZE      32

/* ==========================================================
 * PART 1 - Shared counter
 * ========================================================== */

static long long counter = 0;
static pthread_mutex_t counter_mutex = PTHREAD_MUTEX_INITIALIZER;

static void *counter_worker(void *arg)
{
    (void)arg;

    for (long i = 0; i < NUM_LOOPS; i++) {
        /* Protect the shared counter with a mutex */
        pthread_mutex_lock(&counter_mutex);
        counter++;
        pthread_mutex_unlock(&counter_mutex);
    }

    return NULL;
}

/* ==========================================================
 * PART 2 - Reentrancy test
 * ========================================================== */

static char *tag_of(int id)
{
    static char buf[BUF_SIZE];          /* shared by all threads */

    snprintf(buf, sizeof(buf), "#%d#", id);
    return buf;
}

static char *tag_of_r(int id, char *buf, size_t size)
{
    snprintf(buf, size, "#%d#", id);
    return buf;
}

/* Per-thread bookkeeping for the reentrancy test */
struct reent_arg {
    int       id;                       /* value this thread always passes in */
    long long calls;                    /* number of calls made */
    long long mismatches;               /* calls that returned the wrong text */
};

/* Each thread asks for its own tag and checks that it got its own tag back. */
static void *reent_worker_unsafe(void *arg)
{
    struct reent_arg *a = arg;
    char expected[BUF_SIZE];

    snprintf(expected, sizeof(expected), "#%d#", a->id);

    for (long i = 0; i < REENT_LOOPS; i++) {
        char *s = tag_of(a->id);

        sched_yield();                  /* widen the window for another thread */

        if (strcmp(s, expected) != 0)
            a->mismatches++;
        a->calls++;
    }

    return NULL;
}

static void *reent_worker_safe(void *arg)
{
    struct reent_arg *a = arg;
    char expected[BUF_SIZE];
    char buf[BUF_SIZE];                 /* on this thread's own stack */

    snprintf(expected, sizeof(expected), "#%d#", a->id);

    for (long i = 0; i < REENT_LOOPS; i++) {
        char *s = tag_of_r(a->id, buf, sizeof(buf));

        sched_yield();

        if (strcmp(s, expected) != 0)
            a->mismatches++;
        a->calls++;
    }

    return NULL;
}

/* Run one variant of the test and report the totals. */
static int run_reent_test(const char *label, void *(*fn)(void *))
{
    pthread_t threads[NUM_THREADS];
    struct reent_arg args[NUM_THREADS];
    long long total_calls = 0, total_mismatches = 0;

    for (int i = 0; i < NUM_THREADS; i++) {
        args[i].id = i;
        args[i].calls = 0;
        args[i].mismatches = 0;

        int ret = pthread_create(&threads[i], NULL, fn, &args[i]);
        if (ret != 0) {
            fprintf(stderr, "pthread_create failed: %d\n", ret);
            return -1;
        }
    }

    for (int i = 0; i < NUM_THREADS; i++) {
        int ret = pthread_join(threads[i], NULL);
        if (ret != 0) {
            fprintf(stderr, "pthread_join failed: %d\n", ret);
            return -1;
        }
        total_calls += args[i].calls;
        total_mismatches += args[i].mismatches;
    }

    printf("%s\n", label);
    printf("  Calls made:          %lld\n", total_calls);
    printf("  Wrong results:       %lld\n", total_mismatches);
    printf("  Corruption rate:     %.2f%%\n",
           100.0 * (double)total_mismatches / (double)total_calls);

    if (total_mismatches == 0)
        printf("  PASS: function is reentrant.\n\n");
    else
        printf("  FAIL: function is NOT reentrant.\n\n");

    return total_mismatches == 0 ? 0 : 1;
}

/* ==========================================================
 * main
 * ========================================================== */

int main(void)
{
    pthread_t threads[NUM_THREADS];

    /* ---------- Part 1: shared counter ---------- */
    printf("=== Part 1: shared counter ===\n");

    for (int i = 0; i < NUM_THREADS; i++) {
        int ret = pthread_create(&threads[i], NULL, counter_worker, NULL);
        if (ret != 0) {
            fprintf(stderr, "pthread_create failed: %d\n", ret);
            return EXIT_FAILURE;
        }
    }

    for (int i = 0; i < NUM_THREADS; i++) {
        int ret = pthread_join(threads[i], NULL);
        if (ret != 0) {
            fprintf(stderr, "pthread_join failed: %d\n", ret);
            return EXIT_FAILURE;
        }
    }

    long long expected = (long long)NUM_THREADS * NUM_LOOPS;

    printf("Expected counter value: %lld\n", expected);
    printf("Actual counter value:   %lld\n", counter);

    if (counter == expected)
        printf("PASS: counter value is correct.\n\n");
    else
        printf("FAIL: race condition detected.\n\n");

    /* ---------- Part 2: reentrancy ---------- */
    printf("=== Part 2: reentrancy of tag_of() ===\n");

    run_reent_test("tag_of()   - static buffer:", reent_worker_unsafe);
    run_reent_test("tag_of_r() - caller buffer:", reent_worker_safe);

    pthread_mutex_destroy(&counter_mutex);

    return EXIT_SUCCESS;
}