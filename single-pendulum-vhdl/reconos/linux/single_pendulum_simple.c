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
#include "data.h"

#define MAX_THREADS 32

#define TO_WORDS(x) ((x)/4)

#define THREAD_EXIT    ((void*)-1)
#define WITHOUT_MEMORY ((void*)-2)
#define SET_ITERATIONS ((uint32_t)-3)

// software threads
pthread_t swt[MAX_THREADS];
pthread_attr_t swt_attr[MAX_THREADS];

// hardware threads
struct reconos_resource res[2];
struct reconos_hwt hwt[MAX_THREADS];


// mailboxes
struct mbox mb_start;
struct mbox mb_stop;

void print_data(real_t* data, size_t count)
{
    size_t i;
    for (i=0; i<count; i++)
    {
        if (sizeof(real_t) == 8)
            printf("(%04zu) %16Lx     ", i, *(unsigned long long int*)(void*)&data[i]);
        else
            printf("(%04zu) %16x     ", i, *(unsigned int*)(void*)&data[i]);

        if ((i+1)%4 == 0) printf("\n\t\t");
    }
    printf("\n");
}

void print_reals(real_t* data, size_t count)
{
    size_t i;
    for (i=0; i<count; i++)
    {
        printf("(%04zu) %6.3f     ", i, data[i]);
        if ((i+1)%4 == 0) printf("\n\t\t");
    }
    printf("\n");
}

void mbox_put_data(struct mbox *mb, const void* p, size_t size)
{
    int i;
    const uint32_t* parts = (const uint32_t*)p;

    assert((size%sizeof(uint32_t)) == 0);

    for (i=0; i<size/sizeof(uint32_t); i++)
    {
        mbox_put(mb, parts[i]);
    }
}

void mbox_get_data(struct mbox *mb, void* p, size_t size)
{
    int i;
    uint32_t* parts = (uint32_t*)p;

    assert((size%sizeof(uint32_t)) == 0);

    for (i=0; i<size/sizeof(uint32_t); i++)
    {
        parts[i] = mbox_get(mb);
    }
}

void mbox_put_pointer(struct mbox *mb, void* p)
{
    mbox_put_data(mb, &p, sizeof(void*));
}

void* mbox_get_pointer(struct mbox *mb)
{
    void *p;
    mbox_get_data(mb, &p, sizeof(void*));
    return p;
}

void single_pendulum_simple(single_pendulum_simple_state_t* state) {
    //Vp[3] = Vp[0];
    state->dx0 = state->x1;
    state->Vdx[1] = (-(state->Vp[2])*state->Vp[1]*sin(state->Vx[0])-state->Vx[1]*state->Vp[3]+state->Vu[0])/state->Vp[4];
}

volatile static int iterations_in_thread = 1;

// sort thread shall behave the same as hw thread:
// - get pointer to data buffer
// - if valid address: sort data and post answer
// - if exit command: issue thread exit os call
void *software_thread(void* data)
{
    void* ret;
    struct reconos_resource *res  = (struct reconos_resource*) data;
    struct mbox *mb_start = res[0].ptr;
    struct mbox *mb_stop  = res[1].ptr;
    //pthread_t self = pthread_self();
    //printf("SW Thread %lu: Started with mailbox addresses %p and %p ...\n", self,  mb_start, mb_stop);
    while ( 1 ) {
        ret = mbox_get_pointer(mb_start);
        //printf("SW Thread %lu: Got address %p from mailbox %p.\n", self, (void*)ret, mb_start);
        if (ret == THREAD_EXIT)
        {
            //  printf("SW Thread %lu: Got exit command from mailbox %p.\n", self, mb_start);
            pthread_exit((void*)0);
        }
        else
        {
            int iterations = iterations_in_thread;
            int i;
            for (i=0; i<iterations; ++i)
                single_pendulum_simple( (single_pendulum_simple_state_t*) ret );
        }
        
        mbox_put_pointer(mb_stop, ret);
    }

    return (void*)0;
}

void print_mmu_stats()
{
    int hits,misses,pgfaults;

    reconos_mmu_stats(&hits,&misses,&pgfaults);

    printf("MMU stats: TLB hits: %d    TLB misses: %d    page faults: %d\n",hits,misses,pgfaults);
}


void print_help()
{
    printf(
        "ReconOS v3 single_pendulum_simple test application.\n"
        "\n"
        "Usage:\n"
        "\tsingle_pendulum_simple <-h|--help>\n"
        "\tsingle_pendulum_simple [options] <num_hw_threads> <num_sw_threads>\n"
    "Options:\n"
    "--without-reconos\tDon't use ReconOS\n"
    "--without-memory\tThe hardware thread doesn't access the memory. It will calculate with dummy values.\n"
    "--iterations <NUM>\tDo the calculation <NUM> times (short: -n <NUM>)\n"
    "--iterations-in-thread <NUM>\tRepeat the calculation without using any synchronization (short: -m <NUM>)\n"
    "--dont-flush\tDo not flush caches between iterations.\n");
}

static int only_print_help = 0;
static int without_reconos = 0, without_memory = 0, iterations = 1, dont_flush = 0;
static int verbose_progress = 0;

int main(int argc, char ** argv)
{
    int i;
    int ret;
    int hw_threads;
    int sw_threads;
    int running_threads;
    int simulation_steps;
    single_pendulum_simple_state_t *data, *expected;
    int success;
    int iteration;

    timing_t t_start, t_stop;
    ms_t t_generate;
    ms_t t_calculate;
    ms_t t_check;

    // parse options
    while (1)
    {
        int c;

        static struct option long_options[] =
        {
            { "help",            no_argument, &only_print_help, 1 },
            { "without-reconos", no_argument, &without_reconos, 1 },
            { "without-memory",  no_argument, &without_memory,  1 },
            { "dont-flush",      no_argument, &dont_flush,      1 },
            { "iterations",      required_argument, 0, 'n' },
            { "iterations-in-thread", required_argument, 0, 'm' },
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
        case 'm':
            iterations_in_thread = atoi(optarg);
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

    if (without_memory && sw_threads > 0)
    {
        fprintf(stderr, "'--without-memory' only works with hardware threads!\n");
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


    //print_mmu_stats();

    // create pages and generate data
    if (!without_memory) {
        t_start = gettime();

        printf("malloc of %zu bytes...\n", BLOCK_SIZE * simulation_steps);
        data     = malloc(BLOCK_SIZE * simulation_steps);
        expected = malloc(BLOCK_SIZE * simulation_steps);
        printf("generate data ...\n");
        generate_data(data, expected, simulation_steps);

        t_stop = gettime();
        t_generate = calc_timediff_ms(t_start, t_stop);

        printf("Printing of generated data skipped. \n");
        //print_data(data, simulation_steps * sizeof(single_pendulum_simple_state_t) / sizeof(real_t));
    }


    // Start sort threads
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

            if (iterations_in_thread > 1 && hw_threads > 0) {
                // set iterations_in_thread for hw thread
                mbox_put(&mb_start, SET_ITERATIONS);
                mbox_put(&mb_start, iterations_in_thread);
            }

            mbox_put_pointer(&mb_start, (without_memory ? WITHOUT_MEMORY : &data[i]));
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
            (void)mbox_get_pointer(&mb_stop);
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

    // check data
    //data[0] = 6666; // manual fault
    if (!without_memory) {
        t_start = gettime();

        printf("Checking data: ... ");
        fflush(stdout);
        ret = check_data(data, expected, simulation_steps);
        if (ret >= 0)
        {
            int job;
            printf("failure at word index %i\n", ret);
            printf("expected %5.3f    found %5.3f\n", ((real_t*)expected)[ret], ((real_t*)data)[ret]);
            job = ret / BLOCK_SIZE;
            printf("dumping job %d:\n", job);
            printf("  expected:\t"); print_data (expected[job].all, REALS_PER_BLOCK);
            printf("  actual:  \t"); print_data (data    [job].all, REALS_PER_BLOCK);
            printf("  expected:\t"); print_reals(expected[job].all, REALS_PER_BLOCK);
            printf("  actual:  \t"); print_reals(data    [job].all, REALS_PER_BLOCK);

            success = 0;
        }
        else
        {
            printf("success\n");
            //print_data(data, TO_WORDS(buffer_size));

            success = 1;
        }

        t_stop = gettime();
        t_check = calc_timediff_ms(t_start,t_stop);
    }

    // terminate all threads
    printf("Sending terminate message to %i threads:", running_threads);
    fflush(stdout);
    for (i=0; i<running_threads; i++)
    {
      printf(" %i",i);fflush(stdout);
      mbox_put_pointer(&mb_start, THREAD_EXIT);
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
    if (!without_reconos)
        print_mmu_stats();
    if (!without_memory) {
        printf( "Running times (size: %d jobs, %d hw-threads, %d sw-threads):\n"
            "\tGenerate data: %lu ms\n"
            "\tCalculation  : %lu ms\n"
            "\tCheck data   : %lu ms\n",
                simulation_steps, hw_threads, sw_threads,
                t_generate, t_calculate, t_check);
    } else {
        printf( "Running times (size: %d jobs, %d hw-threads, %d sw-threads):\n"
            "\tCalculation  : %lu ms\n",
                simulation_steps, hw_threads, sw_threads,
                t_calculate);
    }

    free(data);
    free(expected);

    if (!without_reconos)
        reconos_cleanup();

    if (success)
        return 0;
    else
        return 1;
}
