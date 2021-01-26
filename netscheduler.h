#ifndef NETSCHEDULER_H
#define NETSCHEDULER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Runtime parameters

// MLP topology
#define	NUM_LAYERS			4  // input+hidden+output+1
#define NUM_INPUTS			3
#define HIDDEN_LAYER_SIZE	3
#define NUM_OUTPUTS			1

// memory allocation for BFS
#define QUEUESIZE			1024

// resource constraints
#define NUM_MULTIPLIERS		1
#define NUM_ADDERS			1

// define overall latency constraint
// (max cycles beyond lower bound)
#define SLACK				10

// maximum iteration interval (actually the variation, so 0 means all inputs are consumed immediately)
#define MAX_II				0

// functional unit latencies
#define LATENCY_MULTIPLIER	1
#define LATENCY_ADDER		3
#define LATENCY_INPUT		1
#define LATENCY_OUTPUT		0

// Don't change anything below this line unless you intend to modify
// the code behavior
#define NODETYPE(t)			t==INPUT ? "input" : \
							t==MULT ? "mult" : \
							t==ADD ? "add" : \
							t==OUTPUT ? "output" : \
							"unknown"
							
#define LATENCY(node)		node == MULT ? LATENCY_MULTIPLIER : \
							node == ADD ? LATENCY_ADDER : \
							node == INPUT ? LATENCY_INPUT : \
							node == OUTPUT ? LATENCY_OUTPUT : \
							0

#define max(a,b) a > b ? a : b;

// type for a node type
typedef enum {INPUT,MULT,ADD,OUTPUT} node_type;

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
} node;

typedef struct {
	int *register_usage_by_cycle;
} register_table;

// type for an edge
typedef struct edge {
	node *edge;
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
} argstype;

// type for traversal order
typedef enum {
	FROM_START,FROM_END
} travordertype;

// DAG ops
void connect_nodes (node *pred,node *succ);
node *create_node(node_type type,int id);
void gen_dot (node *layers[],char *filename,int num_layers,int num_inputs,int num_outputs);
void node2dot (node *mynode,void *args);
void traverse_dag (node *layers[],int num_layers,int num_inputs,int num_outputs,void *args,void (nodefunc)(node *,void *),travordertype travorder);
void clear_flags (node *mynode,void *args);
void gen_c_code (node **layers,
						int num_layers,
						int num_inputs,
						int hidden_layer_size,
						int num_outputs,
						char *filename);
void compute_functional_utilization(node **layers,int num_layers,int num_inputs,int num_outputs,argstype *myargs);
void inc_functional_utilization (node *mynode,void *args);

// net to DAG routines
int add_layer (node **layers,int layer_num,int id,int prev_layer_size,int new_layer_size,layer_type type);
node **create_basic_network_dag (int num_layers,int num_inputs,int hidden_size);

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
void solve_schedule (node **layers,
						int num_layers,
						int num_inputs,
						int num_outputs,
						char *filename);
void tabulate_functional_unit_utilization (node *layers[],int num_layers,int num_inputs,int num_outputs);
void tabulate_registers (node *layers[],int num_layers,int num_inputs,int num_outputs);

#endif
