
#ifndef __DATA_H__
#define __DATA_H__

typedef double real_t;

#pragma pack(push, 1)
typedef union {
    struct {
        real_t p0, p1, p2, p3, p4, u0, x0, x1, t;
        real_t dx0, dx1;
    };

    struct {
        real_t Vp[5];
        real_t Vu[1];
        real_t Vx[2];
        real_t Vt;
        real_t Vdx[2];
    };

    real_t all[11];
} single_pendulum_simple_state_t;
#pragma pack(pop)

#define BLOCK_SIZE (sizeof(single_pendulum_simple_state_t))
#define TO_BLOCKS(x) ((x)/(BLOCK_SIZE))
#define REALS_PER_BLOCK (BLOCK_SIZE / sizeof(real_t))

void generate_data(single_pendulum_simple_state_t* data, single_pendulum_simple_state_t* expected, int simulation_steps);
int check_data(const single_pendulum_simple_state_t* data, const single_pendulum_simple_state_t* expected, int simulation_steps);

#endif // not defined __DATA_H__
