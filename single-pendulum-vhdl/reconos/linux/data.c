#include <string.h>
#include <math.h>
#include "data.h"

static real_t test_data_0[] = {
	0.2,9.81,0.5,0.2,0.2,0.0,1.998999,-0.222032,0.0,-0.222032,-22.08869242579079
};

void generate_data(single_pendulum_simple_state_t* data, single_pendulum_simple_state_t* expected, int simulation_steps)
{
	int i;

	for (i=0; i<simulation_steps; i++) {
		memcpy(data[i].all,     test_data_0, sizeof(*data) - sizeof(data->Vdx));
		memset(data[i].Vdx, 0, sizeof(data[i].Vdx));
		memcpy(expected[i].all, test_data_0, sizeof(*data));
	}
}

int check_data(const single_pendulum_simple_state_t* data, const single_pendulum_simple_state_t* expected, int simulation_steps)
{
	const real_t *a = data->all, *b = expected->all;
	int i;

	for (i=0; i<simulation_steps*REALS_PER_BLOCK; i++) {
		if (fabs(a-b) > 1e-9) {
			return i;
		}
	}

	// no difference found
	return -1;
}
