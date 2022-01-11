#include "netscheduler.h"

int main () {
	// this is the internal representation of the MLP for the codegen
	// it's really just "bookmarks" into the DAG that represents the MLP
	node **layers,**back_layers;
	
	// signals for training
	SIGNAL input_signal,output_signal_expected;
	
	// this is the internal representation of the MLP used by the trainer
	// we create this here because it is the mechanism for the trainer to
	// convey the weights and biases back to us
	// NOTE: the trainer assumes MLP structure as defined in the #define macros
	struct layer trainer_layers[NUM_LAYERS];
	
	// create another MLP so we can save the initial weights and biases for
	// the FPGA implementation of the online trained MLP
	struct layer initial_trainer_layers[NUM_LAYERS];
	
	// these is the argument container needed for DAG traversal
	argstype myargs;
	
	// create DAG for basic 3-layer network
	logmsg("Converting MLP to DAG...");
	int layer_sizes[] = MLP_TOPOLOGY;
	layers=create_basic_network_dag(NUM_LAYERS,layer_sizes,1,0);
	
	// forecast length--an important parameter
	int forecast_length=FORECAST_LENGTH;
	
	srand(42);
	
#ifdef PERFORM_SCHEDULING
	// schedule the DAG
	// actually, this only computes ASAP and ALAPs for each node
	schedule(layers,NUM_LAYERS,layer_sizes[0],layer_sizes[NUM_LAYERS-1]);
	
	// calculate potential functional utilization
	compute_functional_utilization(layers,NUM_LAYERS,layer_sizes[0],layer_sizes[NUM_LAYERS-1],&myargs);
		
	// generate ILP program to schedule the DAG
	generate_ilp_file (layers,NUM_LAYERS,layer_sizes[0],layer_sizes[NUM_LAYERS-1],"schedule.lp",&myargs);
	
	// solve the schedule
	int latency = solve_schedule(layers,NUM_LAYERS,layer_sizes[0],layer_sizes[NUM_LAYERS-1],"schedule.lp");
	logmsg("Schedule minimum latency for %d adders and %d multipliers = %d cycles",NUM_ADDERS,NUM_MULTIPLIERS,latency);
	
	// compute actual functional utilization and generate report
	//tabulate_functional_unit_utilization (layers,NUM_LAYERS,layer_sizes[0],layer_sizes[NUM_LAYERS-1]);
	
	// compute register usage by cycle
	//tabulate_registers (layers,NUM_LAYERS,layer_sizes[0],layer_sizes[NUM_LAYERS-1]);
	
	// print out vector program
	//tabulate_schedule_by_cycle (layers,NUM_LAYERS,layer_sizes[0],layer_sizes[NUM_LAYERS-1]);
#endif

#ifdef GENPDFS
	logmsg("Generating PDF for forward propagation DAG...");
	gen_dot(layers,"forward_pass.pdf",NUM_LAYERS,layer_sizes[0],layer_sizes[NUM_LAYERS-1]);
#endif

#ifdef ONLINE_TRAINING
	// create a reversed version of the layer size array for calculating gradients
	// note that this might not work for non-linear activation functions
	int layer_sizes_in_reverse[NUM_LAYERS];
	for (int i=0;i<NUM_LAYERS;i++) {
		layer_sizes_in_reverse[i]=layer_sizes[NUM_LAYERS-i-1];
	}
	
	// create another DAG for the backpropagation
	logmsg("Creating backpropagation DAG...");
	back_layers=create_basic_network_dag(NUM_LAYERS,layer_sizes_in_reverse,0,1);
	
	// this flag is used for HLS C-code generation
	int gen_backprop=1;

	#ifdef GENPDFS
	// debugging (sorry for the nested ifdefs)
		logmsg("Generating PDF for backpropagation DAG...");
		gen_dot(back_layers,"backward_pass.pdf",NUM_LAYERS,layer_sizes_in_reverse[0],layer_sizes_in_reverse[NUM_LAYERS-1]);
	#endif
#else
	// this flag is used for HLS C-code generation
	int gen_backprop=0;
#endif

#ifdef GEN_HLS_CODE
	logmsg("Creating HLS file...");
	FILE *myFile=fopen ("network.cpp","w+");
	if (!myFile) {
		char str[1024];
		snprintf(str,1024,"ERROR opening \"%s\" for write:","network.c");
		perror(str);
		return 1;
	}
	// generate C file
	logmsg("Generating HLS code...");
	
#ifdef ONLINE_TRAINING

	// this allocates and initializes the weights and biases
	// normally this is done in train_network(), but we want to avoid calling that since
	// we only need randomized weights and biases (as opposed to trained weights and biases)
	initialize_mlp(initial_trainer_layers,NUM_LAYERS,layer_sizes);
	
	gen_header_file(NUM_LAYERS,layer_sizes,initial_trainer_layers);
	gen_c_code_loop_version(layers,back_layers,NUM_LAYERS,layer_sizes,
			   myFile,gen_backprop,forecast_length,initial_trainer_layers);

#else
	train_network (trainer_layers,
			initial_trainer_layers,
			NUM_LAYERS,
			layer_sizes,
			EPOCHS,
			&input_signal,
			&output_signal_expected);

	// sanity check
	check_predicted_signal(input_signal,output_signal_expected);
	
	gen_header_file(NUM_LAYERS,layer_sizes,trainer_layers);
	gen_c_code_loop_version(layers,back_layers,NUM_LAYERS,layer_sizes,
			   myFile,gen_backprop,forecast_length,trainer_layers);
			   
	validate_test_bench ("network.cpp",input_signal,output_signal_expected) {
#endif



	fclose(myFile);
	
	/*
	logmsg("Generating HLS wrapper...");
	generate_hls_wrapper_code(WRAPPER_FILENAME,layers);
	*/
	
	
#endif

/* #ifdef GENERATE_TESTBENCH
	logmsg("Generating HLS testbench...");
	myFile=fopen("testbench.cpp","w+");
	if (!myFile) {
		perror("Error opening \"testbench.cpp\" for write");
		exit(1);
	}
	gen_testbench (myFile,input_signal,output_signal_expected);
	fclose(myFile);
#endif */

	return 0;
}
