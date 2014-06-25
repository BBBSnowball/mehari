#!/bin/sh

set -e


tr -d '\r\262' <"$1" \
	| iconv -f windows-1252 -t ascii//IGNORE \
	| sed -e 's/^static RealT Vq/RealT Vq/' \
		-e 's/\bSTDCALL\s//' -e 's/^_API_SIMMODEL\s*//' -e 's/_SIMMODEL_VERSION/"1.0"/' \
		-e '/^#include "dscfunc\.h"$/ a\\n\n#define RealT double\n#define IntT int\n\/\/#define mod(a, b) (fmod(a, b))\n#define mod(a, b) (a)'

cat <<'EOF'


#ifndef STEPS
#define STEPS 100
#endif
void run(const RealT input_values[STEPS*sizeof(Vu)/sizeof(*Vu)],
		RealT output_values[STEPS*sizeof(Vy)/sizeof(*Vy)]) {
#pragma HLS INTERFACE ap_fifo port=input_values
#pragma HLS INTERFACE ap_fifo port=output_values
	unsigned int input_index = 0, output_index = 0;

	RealT x [sizeof(Vx)/sizeof(*Vx)];
	RealT dx[sizeof(Vx)/sizeof(*Vx)];
	RealT u [sizeof(Vu)/sizeof(*Vu)];
	RealT y [sizeof(Vy)/sizeof(*Vy)];
	RealT h = 0.1;
	RealT t = 0.0;

	for (unsigned int i=0;i<STEPS;i++) {
		unsigned int j;
		int status = 0;
		evalND(x, t, h, &status);
		for (j=0;j<sizeof(u)/sizeof(*u);j++)
			u[j] = input_values[input_index++];
		evalD(x, u, t, h, &status);
		evalS(dx, x, u, t, h, &status);
		outputsND(y, x, t, h, &status);
		outputsD(y, x, u, t, h, &status);
		for (j=0;j<sizeof(u)/sizeof(*u);j++)
			output_values[output_index++] = y[j];

		for (j=0;j<sizeof(x)/sizeof(*x);j++)
			x[j] = x[j] * h*dx[j];

		t += h;
	}
}
EOF
