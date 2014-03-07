#include "reconos.h"
#include "mbox.h"

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <assert.h>

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
            printf("(%04d) %16Lx     ", i, *(unsigned long long int*)(void*)&data[i]);
        else
            printf("(%04d) %16x     ", i, *(unsigned int*)(void*)&data[i]);

        if ((i+1)%4 == 0) printf("\n\t\t");
    }
    printf("\n");
}

void print_reals(real_t* data, size_t count)
{
    size_t i;
    for (i=0; i<count; i++)
    {
        printf("(%04d) %6.3f     ", i, data[i]);
        if ((i+1)%4 == 0) printf("\n\t\t");
    }
    printf("\n");
}

void single_pendulum_simple(single_pendulum_simple_state_t* state) {
    //Vp[3] = Vp[0];
    state->dx0 = state->x1;
    state->Vdx[1] = (-(state->Vp[2])*state->Vp[1]*sin(state->Vx[0])-state->Vx[1]*state->Vp[3]+state->Vu[0])/state->Vp[4];
}

// sort thread shall behave the same as hw thread:
// - get pointer to data buffer
// - if valid address: sort data and post answer
// - if exit command: issue thread exit os call
void *software_thread(void* data)
{
    unsigned int ret;
    unsigned int dummy = 23;
    struct reconos_resource *res  = (struct reconos_resource*) data;
    struct mbox *mb_start = res[0].ptr;
    struct mbox *mb_stop  = res[1].ptr;
    //pthread_t self = pthread_self();
    //printf("SW Thread %lu: Started with mailbox addresses %p and %p ...\n", self,  mb_start, mb_stop);
    while ( 1 ) {
        ret = mbox_get(mb_start);
        //printf("SW Thread %lu: Got address %p from mailbox %p.\n", self, (void*)ret, mb_start);
        if (ret == UINT_MAX)
        {
          //  printf("SW Thread %lu: Got exit command from mailbox %p.\n", self, mb_start);
          pthread_exit((void*)0);
        }
        else
        {
          single_pendulum_simple( (single_pendulum_simple_state_t*) ret );
        }
        
        mbox_put(mb_stop, dummy);
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
    "\tsingle_pendulum_simple <num_hw_threads> <num_sw_threads> [--without-reconos]\n");
}

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
    int without_reconos;

    timing_t t_start, t_stop;
    ms_t t_generate;
    ms_t t_sort;
    ms_t t_check;

    if (argc > 4 || (argc > 1 && argv[1][0] == '-'))
    {
      print_help();
      exit(1);
    }
    // we have exactly 2 arguments now...
    hw_threads = argc > 1 ? atoi(argv[1]) : 1;
    sw_threads = argc > 2 ? atoi(argv[2]) : 0;

    without_reconos = argc > 3 && strcmp(argv[3], "--without-reconos") == 0;

    if (without_reconos && hw_threads > 0)
    {
        fprintf(stderr, "We cannot use hardware threads without reconOS!\n");
        exit(1);
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
    t_start = gettime();

    printf("malloc of %d bytes...\n", BLOCK_SIZE * simulation_steps);
    data     = malloc(BLOCK_SIZE * simulation_steps);
    expected = malloc(BLOCK_SIZE * simulation_steps);
    printf("generate data ...\n");
    generate_data(data, expected, simulation_steps);

    t_stop = gettime();
    t_generate = calc_timediff_ms(t_start, t_stop);

    printf("Printing of generated data skipped. \n");
    //print_data(data, simulation_steps * sizeof(single_pendulum_simple_state_t) / sizeof(real_t));


    // Start sort threads
    t_start = gettime();

    printf("Putting %i blocks into job queue: ", simulation_steps);
    fflush(stdout);

    if (!without_reconos)
        reconos_cache_flush();

    for (i=0; i<simulation_steps; i++)
    {
      printf(" %i",i); fflush(stdout);
      mbox_put(&mb_start, (unsigned int)&data[i]);
    }
    printf("\n");

    // Wait for results
    printf("Waiting for %i acknowledgements: ", simulation_steps);
    fflush(stdout);
    for (i=0; i<simulation_steps; i++)
    {
      printf(" %i",i); fflush(stdout);
      ret = mbox_get(&mb_stop);
    }  
    printf("\n");

    t_stop = gettime();
    t_sort = calc_timediff_ms(t_start,t_stop);

    if (!without_reconos)
        reconos_cache_flush();

    // check data
    //data[0] = 6666; // manual fault
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

    // terminate all threads
    printf("Sending terminate message to %i threads:", running_threads);
    fflush(stdout);
    for (i=0; i<running_threads; i++)
    {
      printf(" %i",i);fflush(stdout);
      mbox_put(&mb_start,UINT_MAX);
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
    printf( "Running times (size: %d jobs, %d hw-threads, %d sw-threads):\n"
        "\tGenerate data: %lu ms\n"
        "\tSort data    : %lu ms\n"
        "\tCheck data   : %lu ms\n",
            simulation_steps, hw_threads, sw_threads,
            t_generate, t_sort, t_check);

    free(data);
    free(expected);

    if (!without_reconos)
        reconos_cleanup();

    if (success)
        return 0;
    else
        return 1;
}

