#include "reconos.h"
#include "mbox.h"

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <assert.h>
#include <getopt.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <limits.h>
#include <string.h>
#include <math.h>
#include "timing.h"

#define MAX_THREADS 32

#define TO_WORDS(x) ((x)/4)

#define THREAD_EXIT    ((uint32_t)-1)

enum resNumbers
{
    MBOX_RECV = 0,
    MBOX_SEND = 1,
    MBOX_TEST = 2,
    SEM_TEST  = 3,

    resNumbersCount
};

// software threads
pthread_t swt[MAX_THREADS];
pthread_attr_t swt_attr[MAX_THREADS];

// hardware threads
struct reconos_resource res[resNumbersCount];
struct reconos_hwt hwt[MAX_THREADS];

// mailboxes
struct mbox mb_start;
struct mbox mb_stop;
struct mbox mb_test;

// semaphores
sem_t sem_test;

typedef enum
{
    opNOP      = 0x00000000,
    opMboxPut  = 0x01000000,
    opMboxGet  = 0x02000000,
    opSemPost  = 0x03000000,
    opSemWait  = 0x04000000,
    opMemRead  = 0x05000000,
    opMemWrite = 0x06000000,

    opCodeMask = 0xff000000,
    opArgMask  = 0x00ffffff
} TargetOperation;

#define OP_WITH_ARG(op, arg) ((op) | ((arg)&opArgMask))


void *software_thread(void* data)
{
    uint32_t ret;
    struct reconos_resource *res  = (struct reconos_resource*) data;
    struct mbox *mb_start = res[MBOX_RECV].ptr;
    struct mbox *mb_stop  = res[MBOX_SEND].ptr;
    struct mbox *mb_test  = res[MBOX_TEST].ptr;
    sem_t  *sem_test      = res[SEM_TEST ].ptr;

    while ( 1 )
    {
        ret = mbox_get(mb_start);

        if (ret == THREAD_EXIT)
            pthread_exit((void*)0);

        unsigned count = (ret & opArgMask)+1;

        switch(ret & opCodeMask)
        {
            case opNOP:
                // do hard work; s/hard/\0ly/
            break;

            case opMboxPut:
                for (;count > 0; count--) {
                    mbox_put(mb_test, count);
                }
            break;

            case opMboxGet:
                for (;count > 0; count--) {
                    mbox_get(mb_test);
                }
            break;

            case opSemPost:
                for (;count > 0; count--) {
                    sem_post(sem_test);
                }
            break;

            case opSemWait:
                for (;count > 0; count--) {
                    sem_wait(sem_test);
                }
            break;
        }

        mbox_put(mb_stop, ret);
    }

    return (void*)0;
}


void print_help()
{
    printf(
        "ReconOS v3 mbox_put_get test application.\n"
        "\n"
        "Usage:\n"
        "\tmbox_put_get <-h|--help>\n"
        "\tmbox_put_get [options] <num_hw_threads> <num_sw_threads>\n"
    "Options:\n"
    "--without-reconos\tDon't use ReconOS\n"
    "--dont-flush\tDo not flush caches between iterations.\n"
    "--iterations <NUM>\tDo the calculation <NUM> times (short: -n <NUM>)\n"
    "--iterations-in-thread <NUM>\tRepeat the calculation without using any synchronization (short: -m <NUM>)\n"
    "--operation <OP> \t The operation that will be measured (nop, mbox_put, mbox_get, sem_post, sem_wait) (short: -o <OP>)\n"
    "--mbox-size <SIZE> \t The queue size of test mboxes (short: -s <SIZE>)\n");
}


TargetOperation parseOperation(const char *operation)
{
    if (strcmp(operation, "nop") == 0)
        return opNOP;
    if (strcmp(operation, "mbox_put") == 0)
        return opMboxPut;
    if (strcmp(operation, "mbox_get") == 0)
        return opMboxGet;
    if (strcmp(operation, "sem_post") == 0)
        return opSemPost;
    if (strcmp(operation, "sem_wait") == 0)
        return opSemWait;
    assert(0);
}


void debug_print_semaphore(const char *label, sem_t *sem)
{
    printf("%s: ", label);
    int sval;
    if (sem_getvalue(sem, &sval) == 0)
        printf("%d\n", sval);
    else
        printf("ERROR\n");
}


static int debug_semaphores = 0;
static int delta_iterations = 0;
static int only_print_help = 0;
static int without_reconos = 0, dont_flush = 0;
static int verbose_progress = 0;
static unsigned int iterations = 1;
static unsigned int iterations_in_thread = 1;
static unsigned int operation = opNOP;
static unsigned int mbox_size = 32;


int main(int argc, char ** argv)
{
    int i;
    int hw_threads;
    int sw_threads;
    int running_threads;
    int simulation_steps;
    int iteration;

    timing_t t_start, t_stop;
    ms_t t_calculate;

    static struct option long_options[] =
    {
        { "help",                   no_argument, &only_print_help, 1 },
        { "without-reconos",        no_argument, &without_reconos, 1 },
        { "dont-flush",             no_argument, &dont_flush,      1 },
        { "iterations",             required_argument, 0, 'n' },
        { "iterations-in-thread",   required_argument, 0, 'm' },
        { "operation",              required_argument, 0, 'o' },
        { "mbox-size",              required_argument, 0, 's' },
        { "delta-iterations",       required_argument, 0, 'D' },
        { "debug-semaphores",       no_argument, &debug_semaphores, 1 },
        {0, 0, 0, 0}
    };

    // parse options
    while (1)
    {
        int c;
        int option_index = 0;
        c = getopt_long (argc, argv, "n:m:o:s:D:dh?", long_options, &option_index);

        if (c == -1)
            // end of options
            break;

        switch (c)
        {
        case 0:
            // flags are handled by getopt - nothing else to do
            break;
        case 'n':
            iterations = atoi(optarg);
            break;
        case 'm':
            iterations_in_thread = atoi(optarg);
            break;
        case 'o':
            operation = parseOperation(optarg);
            break;
        case 's':
            mbox_size = atoi(optarg);
            break;
        case 'd':
            debug_semaphores = 1;
            break;
        case 'D':
            delta_iterations = atoi(optarg);
            break;
        case 'h':
        case '?':
            only_print_help = 1;
            break;
        }
    }

#   ifdef WITHOUT_RECONOS
    without_reconos = 1;
#   endif

    verbose_progress = (iterations < 10);

    if (only_print_help || argc - optind > 2)
    {
        print_help();
        exit(-1);
    }

    hw_threads = optind < argc ? atoi(argv[optind++]) : 1;
    sw_threads = optind < argc ? atoi(argv[optind++]) : 0;

    if (iterations < 1)
    {
        fprintf(stderr, "The number of iterations must be at least 1.\n");
        exit(-1);
    }

    if (without_reconos && hw_threads > 0)
    {
        fprintf(stderr, "We cannot use hardware threads without reconOS!\n");
        exit(-1);
    }

    if (sizeof(void*) > sizeof(uint32_t) && sw_threads + hw_threads > 1)
    {
        fprintf(stderr, "mboxes work with 4-byte values, so we have some trouble passing a "
            "pointer through them. We have to pass it in parts, but with more than one thread, "
            "the threads might get parts from different pointers. Therefore, you cannot use "
            "more than one thread on this platform.\n");
        exit(-1);
    }

    if (iterations_in_thread > 1) {
        if (hw_threads > 0 && hw_threads + sw_threads > 1)
        {
            fprintf(stderr, "'--iterations-in-thread' can only be used with software threads "
                "or one hardware thread.\n");
            exit(-1);
        }
    }

     if (iterations_in_thread >= 1<<24) {
        fprintf(stderr, "'--iterations-in-thread' can not be larger then 2^24.\n");
        exit(-1);
     }

    running_threads = hw_threads + sw_threads;

    // We calculate one step per thread.
    simulation_steps = running_threads;

    //int gettimeofday(struct timeval *tv, struct timezone *tz);

    void* buffer = NULL;
    if (operation == opMemRead || operation == opMemWrite)
        buffer = malloc(4*mbox_size);

    // init mailboxes
    mbox_init(&mb_start, simulation_steps);
    mbox_init(&mb_stop,  simulation_steps);
    if (operation == opMboxPut || operation == opMboxGet)
        mbox_init(&mb_test,  mbox_size);

    // init semaphore
    sem_init(&sem_test, 0, 0);

    // init reconos and communication resources
    if (!without_reconos)
        reconos_init();

    res[0].type = RECONOS_TYPE_MBOX;
    res[0].ptr  = &mb_start;
    res[1].type = RECONOS_TYPE_MBOX;
    res[1].ptr  = &mb_stop;
    res[2].type = RECONOS_TYPE_MBOX;
    res[2].ptr  = &mb_test;
    res[3].type = RECONOS_TYPE_SEM;
    res[3].ptr  = &sem_test;

    printf("Creating %i hw-threads: ", hw_threads);
    fflush(stdout);
    for (i = 0; i < hw_threads; i++)
    {
      printf(" %i",i);fflush(stdout);
      reconos_hwt_setresources(&(hwt[i]), res, resNumbersCount);
      reconos_hwt_create(&(hwt[i]),i,NULL);
    }
    printf("\n");

    // init software threads
    printf("Creating %i sw-threads: ",sw_threads);
    fflush(stdout);
    for (i = 0; i < sw_threads; i++)
    {
      printf(" %i",i);fflush(stdout);
      pthread_attr_init(&swt_attr[i]);
      pthread_create(&swt[i], &swt_attr[i], software_thread, (void*)res);
    }
    printf("\n");

    // Start threads
    t_start = gettime();

    if (!verbose_progress) {
        printf("Calculating... ");
        fflush(stdout);
    }

    for (iteration=0; iteration<iterations; iteration++) {
        if (verbose_progress) {
            printf("Putting %i blocks into job queue: ", simulation_steps);
            fflush(stdout);
        }

        if (!without_reconos && !dont_flush)
            reconos_cache_flush();

        for (i=0; i<simulation_steps; i++)
        {
            if (verbose_progress) { printf(" %i",i); fflush(stdout); }

            mbox_put(&mb_start, OP_WITH_ARG(operation, iterations_in_thread-1+delta_iterations));
        }
        if (verbose_progress) printf("\n");


        if (debug_semaphores) 
        {
            debug_print_semaphore("before: mb_test.sem_read", &mb_test.sem_read);
            debug_print_semaphore("before: mb_test.sem_write", &mb_test.sem_write);
            debug_print_semaphore("before: sem_test", &sem_test);
        }


        if (verbose_progress) { printf("Perform operation: "); fflush(stdout); }
        for (i=0; i<simulation_steps; i++)
        {
            if (verbose_progress) { printf(" %i",i); fflush(stdout); }

            unsigned count = iterations_in_thread;

            switch(operation)
            {
                case opNOP:
                    // relax...
                break;

                case opMboxPut:
                    for (; count > 0; count--) {
                        mbox_get(&mb_test);
                    }
                break;

                case opMboxGet:
                    for (; count > 0; count--) {
                        mbox_put(&mb_test, count);
                    }
                break;

                case opSemPost:
                    for (; count > 0; count--) {
                        sem_wait(&sem_test);
                    }
                break;

                case opSemWait:
                    for (; count > 0; count--) {
                        sem_post(&sem_test);
                    }
                break;

                case opMemRead:
                case opMemWrite:
                    mbox_put(&mb_start, buffer);
                    mbox_put(&mb_start, mbox_size);
                break;
            }
        }
        if (verbose_progress) printf("\n");

        if (debug_semaphores) 
        {
            debug_print_semaphore("after: mb_test.sem_read", &mb_test.sem_read);
            debug_print_semaphore("after: mb_test.sem_write", &mb_test.sem_write);
            debug_print_semaphore("after: sem_test", &sem_test);
        }

        // Wait for results
        if (verbose_progress) {
            printf("Waiting for %i acknowledgements: ", simulation_steps);
            fflush(stdout);
        }
        for (i=0; i<simulation_steps; i++)
        {
            if (verbose_progress) { printf(" %i",i); fflush(stdout); }
            (void)mbox_get(&mb_stop);
        }  
        if (verbose_progress) printf("\n");
    }

    if (!verbose_progress) {
        printf("done\n");
        fflush(stdout);
    }

    t_stop = gettime();
    t_calculate = calc_timediff_ms(t_start,t_stop);

    if (!without_reconos)
        reconos_cache_flush();

    // terminate all threads
    printf("Sending terminate message to %i threads:", running_threads);
    fflush(stdout);
    for (i=0; i<running_threads; i++)
    {
      printf(" %i",i);fflush(stdout);
      mbox_put(&mb_start, THREAD_EXIT);
    }
    printf("\n");

    printf("Waiting for termination...\n");
    for (i=0; i<hw_threads; i++)
    {
      pthread_join(hwt[i].delegate,NULL);
    }
    for (i=0; i<sw_threads; i++)
    {
      pthread_join(swt[i],NULL);
    }

    printf("\n");

    printf( "Running times (size: %d jobs, %d hw-threads, %d sw-threads):\n"
        "\tCalculation  : %lu ms\n",
            simulation_steps, hw_threads, sw_threads,
            t_calculate);

    if (!without_reconos)
        reconos_cleanup();

    return 0;
}
