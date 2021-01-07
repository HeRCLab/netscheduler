#include "netscheduler.h"

int main () {
	node **layers;
	argstype myargs;
	
	// create DAG for basic 3-layer network
	// actually 4 layers, since our output is considered its own layer, since
	// we can use its scheduled cycle as the total network latency
	layers=create_basic_network_dag(NUM_INPUTS,HIDDEN_LAYER_SIZE);
	
	// schedule the DAG
	// actually, this only computes ASAP and ALAPs for each node
	schedule(layers,NUM_LAYERS,NUM_INPUTS,NUM_OUTPUTS);
	
	// calculate potential functional utilization
	compute_functional_utilization(layers,NUM_LAYERS,NUM_INPUTS,NUM_OUTPUTS,&myargs);
	
	// generate C file
	gen_c_code(layers,NUM_LAYERS,NUM_INPUTS,HIDDEN_LAYER_SIZE,NUM_OUTPUTS,"network.c");
	
	// generate ILP program to schedule the DAG
	generate_ilp_file (layers,NUM_LAYERS,NUM_INPUTS,NUM_OUTPUTS,"schedule.lp",&myargs);
	
	// solve the schedule
	solve_schedule(layers,NUM_LAYERS,NUM_INPUTS,NUM_OUTPUTS,"schedule.lp");
	
	// generate a DOT file
	gen_dot(layers,"my_dag.pdf",4,NUM_INPUTS,1);
	
	// compute actual functional utilization and generate report
	tabulate_functional_unit_utilization (layers,NUM_LAYERS,NUM_INPUTS,NUM_OUTPUTS);
	
	return 0;
}
