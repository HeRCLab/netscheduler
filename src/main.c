#include "netscheduler.h"
#include "computegraph.h"

#include <herc/argparse.h>

#define STR_IMPL_(x) #x      
#define STR(x) STR_IMPL_(x)


int generate_main(int argc, const char** argv) {
	const char* const usages[] = {
		"schednet generate [options] OUTPUT_FILE",
		"schednet generate [options]",
		NULL,
	};

	int num_layers = NUM_LAYERS;
	int hidden_size = HIDDEN_LAYER_SIZE;
	int num_inputs = NUM_INPUTS;
	int num_outputs = NUM_OUTPUTS;

	struct argparse_option options[] = {
		OPT_HELP(),
		OPT_INTEGER('l', "layers", &num_layers, "Number of hidden layers in the MLP (default: "STR(NUM_LAYERS)")", NULL, 0, 0),
		OPT_INTEGER('H', "hidden_size", &hidden_size, "Number of nodes in the hidden layer(s) (default: "STR(HIDDEN_LAYER_SIZE)")", NULL, 0, 0),
		OPT_INTEGER('I', "inputs", &num_inputs, "Number of inputs to the MLP (default:"STR(NUM_INPUTS)")", NULL, 0, 0),
		OPT_INTEGER('O', "outputs", &num_outputs, "Number of outputs to the MLP (default:"STR(NUM_OUTPUTS)")", NULL, 0, 0),
		OPT_END(),
	};

	struct argparse argparse;
	argparse_init(&argparse, options, usages, 0);

	argparse_describe(&argparse, "\nGenerates C code implementing various MLP neural-network related functions based on provided parameters.", "");
	argc = argparse_parse(&argparse, argc, argv);

	node **layers;
	layers = create_basic_network_dag(num_layers, num_inputs, hidden_size);
	gen_c_code(layers,
			num_layers,
			num_inputs,
			hidden_size,
			num_outputs,
			(argc > 0) ? argv[0] : "/dev/stdout"
	);

	gen_backwards_pass(layers,
			num_layers,
			num_inputs,
			hidden_size,
			num_outputs,
			(argc > 0) ? argv[0] : "/dev/stdout"
	);


	return 0;
}

void usage(void) {
	fprintf(stderr, "schednet COMMAND [command arguments... ]\n");
	fprintf(stderr, "\nAvailable commands:\n");
	fprintf(stderr, "\tgenerate - code generation utilities\n");
	fprintf(stderr, "\tgen      - alias for generate\n");
	fprintf(stderr, "\nUse schednet COMMAND --help for more information on a specific command.\n");
}

int main (int argc, const char** argv) {

	/* temporary hack to mess with cg stuff */
	cg* g = cg_init_mlp(1, 1, 2, 5, 3);
	/* cg_node n; */
	/* cg_edge e; */
	/* dgraph_debug_dump(stderr, cg, g->graph, n, printf("%s\n", cg_node_type_to_string(n.type));, e, printf("\n");); */
	cg_make_concrete(g);
	/* cg_generate_dot(g, stdout); */
	cg_generate_forward_pass(g, stdout, "forward_pass", "float");
	cg_destroy(g);
	exit(0);

	if (argc < 2) { usage(); exit(1); }

	/* Don't bother doing real argument parsing at this stage, since we
	 * will either call into a subcommand or else show help and then exit.
	 */
	if ((strncmp(argv[1], "help", 4) == 0) || 
	    (strncmp(argv[1], "--help", 6) == 0) ||
	    (strncmp(argv[1], "-help", 5) == 0) ||
	    (strncmp(argv[1], "-h", 2) == 0)) {
		usage();
		exit(0);
	}

	/* Expand aliases */
	if (strncmp(argv[1], "gen", 3) == 0) { argv[1] = "generate"; }

	if (strncmp(argv[1], "generate", 8) == 0) {
		return generate_main(argc - 1, &(argv[1]));
	}

	exit(0);

	/* dead code - needs to be cleaned up and put inside of subcommands */

	node **layers;
	argstype myargs;
	
	// create DAG for basic 3-layer network
	// actually 4 layers, since our output is considered its own layer, since
	// we can use its scheduled cycle as the total network latency
	layers=create_basic_network_dag(NUM_LAYERS,NUM_INPUTS,HIDDEN_LAYER_SIZE);
	
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
	
	// compute register usage by cycle
	tabulate_registers (layers,NUM_LAYERS,NUM_INPUTS,NUM_OUTPUTS);
	
	// print out vector program
	tabulate_schedule_by_cycle (layers,NUM_LAYERS,NUM_INPUTS,NUM_OUTPUTS);
		
	return 0;
}

