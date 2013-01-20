#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <getopt.h>
#include "spinlock.h"

#if NUM_THREADS < 256
#define DEFAULT_TEST_THREADS (255)
#else
#define DEFAULT_TEST_THREADS (300)
#endif

#define DEFAULT_TEST_ITERATIONS (2)
#define TEST_ITERATIONS (test_args.t_iterations)
#define TEST_THREADS    (test_args.t_threads)

static struct spinlock g_lock = __SPIN_LOCK_INITIALIZER;
static struct spinlock g_sync_lock = __SPIN_LOCK_INITIALIZER;
static volatile int num_threads;
struct test_args
{
    int t_iterations;
    int t_threads;
};

static struct test_args test_args = { .t_iterations = DEFAULT_TEST_ITERATIONS,
                                      .t_threads = DEFAULT_TEST_THREADS
};

static void sync_wait(void)
{
    spin_lock(&g_sync_lock);
    ++num_threads;
    while(num_threads != TEST_THREADS)
    {
        //release before re-test
        spin_unlock(&g_sync_lock);
        sleep(1);
        spin_lock(&g_sync_lock);
    }
    spin_unlock(&g_sync_lock);
}

static void *test_thread(void *unused)
{
    sync_wait();

#ifdef TEST_TRY_LOCK
    while(spin_trylock(&g_lock) == 0);
#else
    spin_lock(&g_lock);
#endif

#ifdef DEBUG
    printf("Lock acquired by thread [%lld], [head:%d, tail:%d]\n", 
           (unsigned long long)pthread_self(), g_lock.tickets.head, g_lock.tickets.tail);
#endif

    spin_unlock(&g_lock);

#ifdef DEBUG
    printf("Unlock by thread [%lld], [head:%d, tail:%d]\n", 
           (unsigned long long)pthread_self(), g_lock.tickets.head, g_lock.tickets.tail);
#endif

    return NULL;
}

static char *prog;
static void usage(void)
{
    printf("%s options\n"
           "\t-t | number of test lock threads \n"
           "\t-i | number of test iterations \n"
           "\t-h | this help\n", prog);
}

int main(int argc, char **argv)
{
    pthread_t *tids = NULL;
    int err, i, c = 0;

    if( (prog = strrchr(argv[0], '/') ) )
        ++prog;
    else prog = argv[0];

    opterr = 0;
    while ( (c = getopt(argc, argv, "t:i:h" ) ) != EOF )
    {
        switch(c)
        {
        case 't':
            TEST_THREADS = atoi(optarg);
            break;
        case 'i':
            TEST_ITERATIONS = atoi(optarg);
            break;
        case 'h':
        case '?':
        default:
            usage();
            exit(EXIT_FAILURE);
        }
    }
    if(optind != argc) usage();

    if(TEST_THREADS <= 0 || TEST_THREADS > MAX_THREADS)
        TEST_THREADS = DEFAULT_TEST_THREADS;

    if(TEST_ITERATIONS <= 0)
        TEST_ITERATIONS = DEFAULT_TEST_ITERATIONS;

    tids = calloc(TEST_THREADS, sizeof(*tids));
    assert(tids != NULL);
    printf("Running spinlock test for [%d] iterations with [%d] threads\n",
           TEST_ITERATIONS, TEST_THREADS);
    for(c = 0; c < TEST_ITERATIONS; ++c)
    {
        num_threads = 0;
        printf("Test iteration [%d]\n", c);
        for(i = 0; i < TEST_THREADS; ++i)
        {
            err = pthread_create(&tids[i], NULL, test_thread, NULL);
            assert(err == 0);
        }
        for(i = 0; i < TEST_THREADS; ++i)
            pthread_join(tids[i], NULL);
    }
    free(tids);
    return 0;
}    
