#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#define NUM_THREADS 8
#define NUM_LOOPS   1000000

static long long counter = 0;
static pthread_mutex_t counter_mutex = PTHREAD_MUTEX_INITIALIZER;

/* Thread function */
static void *worker(void *arg)
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

int main(void)
{
    pthread_t threads[NUM_THREADS];

    /* Create multiple threads */
    for (int i = 0; i < NUM_THREADS; i++) {
        int ret = pthread_create(&threads[i], NULL, worker, NULL);

        if (ret != 0) {
            fprintf(stderr, "pthread_create failed: %d\n", ret);
            return EXIT_FAILURE;
        }
    }

    /* Wait for all threads to finish */
    for (int i = 0; i < NUM_THREADS; i++) {
        int ret = pthread_join(threads[i], NULL);

        if (ret != 0) {
            fprintf(stderr, "pthread_join failed: %d\n", ret);
            return EXIT_FAILURE;
        }
    }

    /* Calculate the expected result */
    long long expected =
        (long long)NUM_THREADS * NUM_LOOPS;

    printf("Expected counter value: %lld\n", expected);
    printf("Actual counter value:   %lld\n", counter);

    /* Verify correctness */
    if (counter == expected) {
        printf("PASS: counter value is correct.\n");
    } else {
        printf("FAIL: race condition detected.\n");
    }

    pthread_mutex_destroy(&counter_mutex);

    return EXIT_SUCCESS;
}
