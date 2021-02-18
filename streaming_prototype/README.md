Streaming Network Wrapper Example
---------------------------------

This project is intended to demonstrate a simplistic framework for how a Vivado/Vitis HLS forward/backward pass could be embedded into a streaming 1x input, 1x output wrapper.

## How to build

 1. Create a new project in Vitis.
 1. Select the `xczu7ev-ffvc1156-2-e` board, and use a `'Vivado IP Flow'` target.
 1. Add `network_wrapper.cpp` and `shift_reg.h` to the sources.
 1. Synthesize.

## How to run the test bench

TODO

## Architecture

The core is divided into 3 code sections (not in order of appearance in the code):

 - Top-level function
 - Primary template function
 - `mynetwork` Forward-pass function

The top-level exists to provide a fully-instantiated starting point for the HLS tools.
A common pattern in HLS projects is to embed method calls or template function instantiations inside one of these top-level functions, using inlining in the function so that there is no call overhead.

The primary template function is the "wrapper" that this project focuses on.
It provides a wrapper around the forward pass, and embeds a shift register for input values whose length is controllable at compile time.

The `mynetwork` function is a modified version of what (at time of writing) the `schednet` tool generates.
It implements a forward pass, currently with dummy values for the weights, and parameters for accepting values from the inputs shift register.
