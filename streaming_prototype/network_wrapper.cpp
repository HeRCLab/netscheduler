// NN Wrapper - Provides a streaming input/output system for the neural network.

// These includes should live in an include-once header file to avoid build errors.
#include "hls_stream.h"
#include "hls_math.h"

#include "shift_reg.h"
#include <cassert>

// Important #defines. (Most could eventually be encoded in other ways.)
#define NUM_INPUTS         3
#define NUM_OUTPUTS        1
#define HIDDEN_LAYER_SIZE  3


// Prototype has to be declared here because it's not generated by the
// scheduler toolchain yet.
/*void mynetwork (
	const float input0[1024],
	const float input1[1024],
	const float input2[1024],
	float output0[1024]
);*/
// Updated signature, using plain values / refs (see implementation below for more info):
void mynetwork (
	const float input0,
	const float input1,
	const float input2,
	float& out0
);


// Wrapper function with desired 1x input, 1x output interface.
// Shift register depth can be parameterized at compile time.
template<
unsigned int shift_reg_depth
>
void network_wrapper(
	hls::stream<float>& input0,
	hls::stream<float>& output0
) {
	#pragma HLS inline
	#pragma HLS pipeline II=1

	// Asserts are "free", since they are optimized away at compile time.
	// When used to assert loop bounds, they allow the compiler to give more precise
	//   latency estimates for the loop.
	assert(shift_reg_depth >= NUM_INPUTS);

	ShiftRegister<float, shift_reg_depth> sreg;

	// We do a possibly redundant FIFO occupancy check to avoid outputting
	//   duplicate values on each cycle.
	// Vivado/Vitis should optimize away the other check in shift_in().
	if (!input0.empty()) {
		sreg.shift_in(input0); // Read in value from external port if available.

		float in0;
		float in1;
		float in2;
		float out0;

		// Pull values out of shift register, and assign to temp variables.
		// Vivado/Vitis will optimize these temporaries away at compile time.
		in0 = sreg[0]; // t         (most recent sample)
		in1 = sreg[1]; // t - dt    (1 timestep back)
		in2 = sreg[2]; // t - 2*dt  (2 timesteps back)

		// Forward pass:
		mynetwork(in0, in1, in2, out0);

		// Backward pass would go here when available.

		// Propagating weights between stages might require a shared weights/coefficients
		//   array at this level, which would be written by the backward pass, and read
		//   by the forward pass (1 reader, 1 writer rule).

		// Return prediction(s).
		/*for (int i = 0; i < NUM_OUTPUTS; i++) {
			// Not sure if this can be safely unrolled.
			output0.write(out0);
		}*/

		// Output can be done as a loop, or as a series of .write() calls.
		output0.write(out0);
	}
}

// Updated `mynetwork` function.
// Notable changes include:
// - Inputs are scalars now.
// - Output is a by-reference scalar.
// - Order of operations is rearranged to allow Vivado/Vitis to see the data dependencies.
// - Coefficient/weights arrays are initialized. This saves them from being optimized away.
void mynetwork (
	const float input0,
	const float input1,
	const float input2,
	float& out0
) {
	#pragma HLS pipeline II=1
	float node3,node8,node13,node4,node9,node14,node5,node10,node15,node7,node12,node17,node6,node11,node16,node18,node19,node20,node22,node21,coeff1[3][3],coeff2[1][3];

	// Initialize coefficients:
	coeff1[0][0] = 0.1;
	coeff1[0][1] = 0.2;
	coeff1[0][2] = 0.3;
	coeff1[1][0] = 0.4;
	coeff1[1][1] = 0.5;
	coeff1[1][2] = 0.6;
	coeff1[2][0] = 0.7;
	coeff1[2][1] = 0.7;
	coeff1[2][2] = 0.9;

	coeff2[0][0] = 0.1;
	coeff2[0][1] = 0.2;
	coeff2[0][2] = 0.3;

	// Actual DAG of operations.
	node3 = input0 * coeff1[0][0];
	node8 = input0 * coeff1[1][0];
	node13 = input0 * coeff1[2][0];
	node4 = input1 * coeff1[0][1];
	node9 = input1 * coeff1[1][1];
	node14 = input1 * coeff1[2][1];
	node5 = input2 * coeff1[0][2];
	node10 = input2 * coeff1[1][2];
	node15 = input2 * coeff1[2][2];
	node11 = node9 + node10;
	node6 = node4 + node5;
	node7 = node6 + node3;
	node12 = node11 + node8;
	node16 = node14 + node15;
	node17 = node16 + node13;
	node18 = node7 * coeff2[0][0];
	node19 = node12 * coeff2[0][1];
	node20 = node17 * coeff2[0][2];
	//out0.write(node21 + node18);
	//node21 = node19 + node20; // Not sure if this could cause issues, since this state won't be retained between invocations.

	// Re-ordered these last 2 ops, since the forward pass is now "stateless".
	node21 = node19 + node20;
	out0 = node21 + node18; // Return final output value.
}


// Top-level function to plug into Vivado/Vitis.
// It has no template parameters, so it can be directly instantiated in testbenches/designs.
void streaming_toplevel(
	hls::stream<float>& input0,
	hls::stream<float>& output0
) {
	#pragma HLS pipeline II=1
	network_wrapper<10>(input0, output0); // Shift register will be 10 elements long.
}

