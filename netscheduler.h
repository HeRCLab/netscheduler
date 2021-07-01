#ifndef NETSCHEDULER_H
#define NETSCHEDULER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "trainer.h"

// Runtime parameters

// relates to generation of DAGs
#define BINARY_ADDER

// solver
#define	USE_GUROBI
#define GUROBI_PATH		"LD_LIBRARY_PATH=\"/home/csce611/gurobi911/linux64/lib\" /home/csce611/gurobi911/linux64/bin"
//#define GUROBI_PATH	"/usr/sbin"

//#define	USE_SIGNAL_FILE	"Ivol_Acc_Load_data1_w3_NSTD.txt"
#define USE_SIGNAL_FILE 	"Ivol_Acc_Load_data3_w3_w2_50per_STD.txt"
#define WRAPPER_FILENAME	"wrapper.cpp"
#define SIGNAL_TIME_END	9.75f

// type of schedule sought
//#define VECTORIZE

// debugging statements to be generated in network.cpp and in trainer_layers
//#define	GEN_NETWORK_DEBUG

// enables (or disables) ALL debugging messages
#define DEBUG_FLAG			1

// file for all debugging messages
#define	DEBUG_TARGET		stderr

// which steps to perform
//#define PERFORM_SCHEDULING
#define GEN_HLS_CODE
#define GENERATE_TESTBENCH
#define PERFORM_OFFLINE_TRAINING
#define	EPOCHS				1	// for offline training
#define ONLINE_TRAINING

// MLP forecasting objective
#define FORECAST_LENGTH		20
#define HISTORY_LENGTH		50

// MLP topology (for DAG construction)
#define	NUM_LAYERS		3  // input+hidden+output
#define MLP_TOPOLOGY	{HISTORY_LENGTH,10,1}

// syntheized signal
#define	SYNTHESIZED_SIGNAL_TIME	20.f
#define SAMPLE_RATE				5000.f

// applies to synthesized and read signals
#define SUBSAMPLED_RATE			1250.f

// for both offline and online training
#define LEARNING_RATE			0.1f
#define	INITIAL_WEIGHT_SCALER	1.f

// memory allocation for BFS
#define QUEUESIZE			(1024*1024)

// debugging PDFs
//#define	GENPDFS

// physical resource constraints (for scheduling)
#define NUM_ADDERS			200
#define NUM_MULTIPLIERS		200

// data type for HLS
// uncomment the first two lines for fixed point
#define DATATYPE_BASE	"ap_fixed<5,0,AP_TRN,AP_WRAP>"
#define DATATYPE		"fxp_t"
//#define DATATYPE	"float"

// define overall latency constraint
// (max cycles beyond lower bound)
#define SLACK				50

// maximum iteration interval (actually the variation, so 0 means all inputs are consumed immediately)
#define MAX_II				0

// functional unit latencies
#define LATENCY_MULTIPLIER	1
#define LATENCY_ADDER		3
#define LATENCY_INPUT		1
#define LATENCY_OUTPUT		0

// Don't change anything below this line unless you intend to modify
// the code behavior

#define logmsg(msg,...)		if (DEBUG_FLAG) {\
								char __str[1024];\
								snprintf(__str,1024,msg,##__VA_ARGS__);\
								fprintf(DEBUG_TARGET,"[DEBUG] %s\n",__str);\
								fflush(DEBUG_TARGET);\
							}

#define NODETYPE(t)			t==INPUT ? "input" : \
							t==MULT ? "mult" : \
							t==ADD ? "add" : \
							t==OUTPUT ? "output" : \
							t==ADDBIAS ? "addbias" : \
							"unknown"
							
#define NODETYPE_CODE(t)	t==INPUT ? "input" : \
							t==MULT ? "node" : \
							t==ADD ? "node" : \
							t==OUTPUT ? "output" : \
							t==ADDBIAS ? "node" : \
							"unknown"
							
#define LATENCY(node)		node == MULT ? LATENCY_MULTIPLIER : \
							node == ADD ? LATENCY_ADDER : \
							node == INPUT ? LATENCY_INPUT : \
							node == OUTPUT ? LATENCY_OUTPUT : \
							node == ADDBIAS ? LATENCY_ADDER : \
							0

#define max(a,b) a > b ? a : b;

// type for a node type
typedef enum {INPUT,MULT,ADD,OUTPUT,ADDBIAS} node_type;

// type for a layer type
typedef enum {INPUT_LAYER,NEURON_LAYER,NEURON_BINARY_ADD_LAYER,OUTPUT_LAYER} layer_type;

// type for a DFG node
typedef struct node {
	int id;
	int asap_cycle;
	int alap_cycle;
	node_type type;
	struct edge *out_edges;
	struct edge *in_edges;
	struct node *next;
	struct node *prev;
	int flag;
	int scheduled_cycle;
	int layer;
	int input_number;
	int neuron;
	int final_adder;
	int delta_multiplier;
} node;

typedef struct {
	int *register_usage_by_cycle;
} register_table;

// type for an edge
typedef struct edge {
	node *edge;
	int input_num;
	struct edge *next;
} edge;

// type for traversal function
typedef struct {
	node *node;
	FILE *file;
	int cycle;
	node_type type;
	int first;
	int id;
	int *add_use;
	int *mult_use;
	int *add_scheduled_utilization;
	int *mult_scheduled_utilization;
	int flag;
	int history_length;
	int gen_backwards;
	node *output_layer;
	int num_layers;
	int forecast_length;
	int backprop;
	int secondforward;
	int shift_reg_depth;
	node **forwardprop;
	node **backwardprop;
} argstype;

typedef struct {
	node_type type;
	int cycle;
	int found;
} func_cycle;

// type for traversal order
typedef enum {
	FROM_START,FROM_END
} travordertype;

// DAG ops
void connect_nodes (node *pred,node *succ,int input_num);
node *create_node(node_type type,int id);
void gen_dot (node *layers[],char *filename,int num_layers,int num_inputs,int num_outputs);
void node2dot (node *mynode,void *args);
void traverse_dag (node *layers[],int num_layers,int num_inputs,int num_outputs,void *args,void (nodefunc)(node *,void *),travordertype travorder);
void clear_flags (node *mynode,void *args);
void gen_c_code (node **layers,
						node **back_layers,
						int num_layers,
						int *layer_sizes,
						FILE *myFile,
						int gen_backprop,
						int forecast_length,
						struct layer *trainer_layers);
						
void compute_functional_utilization(node **layers,int num_layers,int num_inputs,int num_outputs,argstype *myargs);
void inc_functional_utilization (node *mynode,void *args);
void generate_hls_wrapper_code(char *filename,node **layers);

// net to DAG routines
int add_layer (node **layers,int layer_num,int id,int prev_layer_size,int new_layer_size,layer_type type,int inc_bias,int inc_delta_multiplier);
node **create_basic_network_dag (int num_layers,int *layer_sizes,int inc_bias,int inc_delta_multiplier);

// scheduling
void set_asaps (node *mynode,void *args);
void set_alaps (node *mynode,void *args);
void schedule (node *layers[],int num_layers,int num_inputs,int num_outputs);
void generate_ilp_file (node **layers,
						int num_layers,
						int num_inputs,
						int num_outputs,
						char *filename,
						argstype *myargs);
int solve_schedule (node **layers,
						int num_layers,
						int num_inputs,
						int num_outputs,
						char *filename);
void tabulate_functional_unit_utilization (node *layers[],int num_layers,int num_inputs,int num_outputs);
void tabulate_registers (node *layers[],int num_layers,int num_inputs,int num_outputs);
void tabulate_schedule_by_cycle (node *layers[],int num_layers,int num_inputs,int num_outputs);

#endif
