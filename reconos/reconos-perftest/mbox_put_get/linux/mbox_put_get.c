#include "reconos.h"
#include "mbox.h"

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
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

// software threads
pthread_t swt[MAX_THREADS];
pthread_attr_t swt_attr[MAX_THREADS];

// hardware threads
struct reconos_resource res[2];
struct reconos_hwt hwt[MAX_THREADS];


// mailboxes
struct mbox mb_start;
struct mbox mb_stop;

void *software_thread(void* data)
{
    uint32_t ret;
    struct reconos_resource *res  = (struct reconos_resource*) data;
    struct mbox *mb_start = res[0].ptr;
    struct mbox *mb_stop  = res[1].ptr;
    while ( 1 ) {
        ret = mbox_get(mb_start);
        if (ret == THREAD_EXIT)
        {
            pthread_exit((void*)0);
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
    "--dont-flush\tDo not flush caches between iterations.\n");
}

static int only_print_help = 0;
static int without_reconos = 0, dont_flush = 0;
static int verbose_progress = 0;
static unsigned int iterations;

int main(int argc, char ** argv)
{
    int i;
    int hw_threads;
    int sw_threads;
    int running_threads;
    int simulation_steps;
    int success;
    int iteration;

    timing_t t_start, t_stop;
    ms_t t_calculate;

    // parse options
    while (1)
    {
        int c;

        static struct option long_options[] =
        {
            { "help",            no_argument, &only_print_help, 1 },
            { "without-reconos", no_argument, &without_reconos, 1 },
            { "dont-flush",      no_argument, &dont_flush,      1 },
            { "iterations",      required_argument, 0, 'n' },
            {0, 0, 0, 0}
        };

        int option_index = 0;
        c = getopt_long (argc, argv, "n:m:h?", long_options, &option_index);

        if (c == -1)
            // end of options
            break;

        switch (c) {
        case 0:
            // flags are handled by getopt - nothing else to do
            break;
        case 'n':
            iterations = atoi(optarg);
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


    running_threads = hw_threads + sw_threads;

    // We calculate one step per thread.
    simulation_steps = running_threads;

    //int gettimeofday(struct timeval *tv, struct timezone *tz);

    // init mailboxes
    mbox_init(&mb_start, simulation_steps);
    mbox_init(&mb_stop,  simulation_steps);

    // init reconos and communication resources
    if (!without_reconos)
        reconos_init();

    res[0].type = RECONOS_TYPE_MBOX;
    res[0].ptr  = &mb_start;
    res[1].type = RECONOS_TYPE_MBOX;
    res[1].ptr  = &mb_stop;

    printf("Creating %i hw-threads: ", hw_threads);
    fflush(stdout);
    for (i = 0; i < hw_threads; i++)
    {
      printf(" %i",i);fflush(stdout);
      reconos_hwt_setresources(&(hwt[i]),res,2);
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

            mbox_put(&mb_start, i);
        }
        if (verbose_progress) printf("\n");

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
