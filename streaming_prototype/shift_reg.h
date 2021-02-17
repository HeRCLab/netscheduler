// Generic implementation of a shift-register for HLS work.
// Copyright (c) Philip Conrad, 2021. All rights reserved.
#include <cassert>
#include "hls_stream.h"

template<
typename T,
unsigned int length
>
class ShiftRegister {
private:
	T reg[length];

public:
	// Default constructor.
	ShiftRegister() {
		#pragma HLS reset variable=reg
		#pragma HLS array_partition variable=reg complete
		#pragma HLS inline
		for (unsigned int i = 0; i < length; i++) {
			#pragma HLS unroll
			reg[i] = T(0);
		}
	}

	// "Feed" operators, that implement the "shift" part of the shift-register.
	inline void operator () (T& output_port, const T& input_port) {
		#pragma HLS inline
		#pragma HLS latency min=1 max=1
		#pragma HLS pipeline II=1

		output_port = reg[length-1];
		for (unsigned int i = length - 1; i != 0; i--) {
			#pragma HLS unroll
			reg[i] = reg[i-1];
		}
		reg[0] = input_port;
	}

	// "Feed" with enable.
	inline void operator () (T& output_port, const T& input_port, bool enable) {
		#pragma HLS inline
		#pragma HLS latency min=1 max=1
		#pragma HLS pipeline II=1

		output_port = reg[length-1];
		if (enable) {
			for (unsigned int i = length - 1; i != 0; i--) {
				#pragma HLS unroll
				reg[i] = reg[i-1];
			}
		}
		reg[0] = input_port;
	}

	// Indexed read operator.
	// Note: If this winds up being too slow, switch to
	//   destination-reference-passing style like the feed functions use.
	// Note: If inefficient memory type chosen, consider adding an
	//   assertion of the form:   assert(index <= length);
	inline T operator [] (unsigned int index) {
		#pragma HLS inline
		#pragma HLS latency min=1 max=1
		assert(index <= length);
		return reg[index];
	}

	inline void shift_in(hls::stream<T>& in) {
		#pragma HLS inline
		if (!in.empty()) {
			for (unsigned int i = length - 1; i != 0; i--) {
				#pragma HLS unroll
				reg[i] = reg[i-1];
			}
			reg[0] = in.read();
		}
	}

	inline void shift_in(hls::stream<T>& in, bool enable) {
			#pragma HLS inline
			if (!in.empty() && enable) {
				for (unsigned int i = length - 1; i != 0; i--) {
					#pragma HLS unroll
					reg[i] = reg[i-1];
				}
				reg[0] = in.read();
			}
		}

	// Could add std::ostream for output niceness.
};
