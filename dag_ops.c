#include "netscheduler.h"

void connect_nodes (node *pred,node *succ,int input_num) {
	edge *myedge;

	// add incoming edge
	if (succ->in_edges == NULL) {
		myedge = succ->in_edges = (edge *)malloc(sizeof(edge));
		myedge->edge = pred;
		myedge->next = NULL;
	} else {
		myedge = succ->in_edges;
		while (myedge->next) myedge = myedge->next;
		myedge = myedge->next = (edge *)malloc(sizeof(edge));
		myedge->edge = pred;
		myedge->next = NULL;
	}
	myedge->input_num = input_num;

	// add outgoing edge
	myedge = pred->out_edges;
	if (myedge == NULL) {
		myedge = pred->out_edges = (edge *)malloc(sizeof(edge));
		myedge->edge = succ;
		myedge->next = NULL;
	} else {
		myedge = pred->out_edges;
		while (myedge->next) myedge = myedge->next;
		myedge = myedge->next = (edge *)malloc(sizeof(edge));
		myedge->edge = succ;
		myedge->next = NULL;
	}
	myedge->input_num = input_num;

}

node *create_node (node_type type,int id) {
	node *mynode = (node *)malloc(sizeof(node));
	mynode->type = type;
	mynode->id = id;
	mynode->in_edges = NULL; // no inputs yet
	mynode->out_edges = NULL; // no outputs yet
	mynode->next = NULL;
	mynode->prev = NULL;
	mynode->asap_cycle = -1;
	mynode->alap_cycle = -1;
	mynode->scheduled_cycle = -1;
	mynode->final_adder = 0;
	mynode->delta_multiplier = 0;
	
	return mynode;
}

void traverse_dag (node *layers[],
				   int num_layers,
				   int num_inputs,
				   int num_outputs,
				   void *args,
				   void (nodefunc)(node *,void *),
				   travordertype travorder) {
					   
	int head=0,tail=0,nodes_in_start_layer;
	node **stack,*mynode;
	edge *myedge;
	
	stack = (node **)malloc(QUEUESIZE*sizeof(node *));
	
	// prepare to add first level
	if (travorder==FROM_START) {
		nodes_in_start_layer = num_inputs;
		mynode = layers[0];
	} else {
		nodes_in_start_layer = num_outputs;
		mynode = layers[num_layers];
	}
	
	// add first level
	while (mynode) {
		// push start node
		stack[tail]=mynode;
		tail = (tail + 1) % QUEUESIZE;
		mynode=mynode->next;
	}
	
	// breadth-first traversal from each input or output (depending on direction)
	while (head!=tail) {
		// pop node
		node *mynode = stack[head];
		head = (head + 1) % QUEUESIZE;

		// push successors/predecessors
		if (travorder==FROM_START) {
			myedge = mynode->out_edges;
		} else {
			myedge = mynode->in_edges;
		}
		
		while (myedge) {
			if ((tail+1) % QUEUESIZE == head) {
				fprintf(stderr,"Fatal: traversal queue size exceeded (currently at %d).\n",tail-head < 0 ? head-tail : tail-head);
				exit(1);
			}
			stack[tail] = myedge->edge;
			tail=(tail + 1) % QUEUESIZE;
			myedge = myedge->next;
		}
		
		// process node
		nodefunc(mynode,args);
	}

	free(stack);
}

void node2dot (node *mynode,void *args) {
	argstype *myargs = (argstype *)args;
	FILE *myFile = myargs->file;
	char node_shape[1024];
	
	if (!mynode->flag) {
		
		switch (mynode->type) {
			case INPUT: strcpy(node_shape,"cds");break;
			case OUTPUT: strcpy(node_shape,"cds");break;
			case MULT: strcpy(node_shape,"box");break;
			case ADD: strcpy(node_shape,"ellipse");break;
			case ADDBIAS: strcpy(node_shape,"square");break;
			default: strcpy(node_shape,"star");break;
		}
		
		// print node
		fprintf (myFile,"%s_%d [shape=%s fontsize=10 label=\"%s_%d(%d,%d,%d)\"]\n",NODETYPE(mynode->type),
																				 mynode->id,
																				 node_shape,
																				 NODETYPE(mynode->type),
																				 mynode->id,
																				 mynode->asap_cycle,
																				 mynode->alap_cycle,
																				 mynode->scheduled_cycle);
		
		// push successors onto stack
		char rank_stmt[1024],node_name[1024];
		snprintf(rank_stmt,1024,"{rank=same; ");

		edge *myedge = mynode->in_edges;
		while (myedge) {
			fprintf (myFile,"%s_%d -> %s_%d\n",NODETYPE(myedge->edge->type),
											   myedge->edge->id,
											   NODETYPE(mynode->type),
											   mynode->id);

			snprintf (node_name,1024,"%s_%d; ",NODETYPE(myedge->edge->type),myedge->edge->id);
			strcat(rank_stmt,node_name);

			myedge = myedge->next;
		}
		strcat (rank_stmt,"}\n");
		fprintf(myFile,"%s",rank_stmt);
		
		mynode->flag=1;
	}
}

void gen_dot (node *layers[],
			  char *filename,
			  int num_layers,
			  int num_inputs,
			  int num_outputs) {
				  
	FILE *myFile;
	char str[1024];
	
	snprintf(str,1024,"/usr/bin/dot -Tpdf -o%s",filename);
	//snprintf(str,1024,"graph.dot");

	myFile = popen(str,"w");
	//myFile = fopen(str,"w");
	if (!myFile) {
		perror("popen()");
		exit(1);
	}

	fprintf(myFile,"digraph {\nrankdir=LR;\n");

	argstype myargs = {.file = myFile};
	
	traverse_dag(layers,num_layers,num_inputs,num_outputs,(void *)&myargs,clear_flags,FROM_END);
	traverse_dag(layers,num_layers,num_inputs,num_outputs,(void *)&myargs,node2dot,FROM_END);

	fprintf(myFile,"}\n");
	fclose(myFile);
}

int node_is_final_adder (node *mynode) {
	if (mynode->type == ADD && mynode->out_edges->edge->type == MULT) return 1;
	return 0;
}

void gen_c_declarations (node *mynode,void *args) {
	FILE *myFile = ((argstype *)args)->file;
	int shift_reg_depth = ((argstype *)args)->shift_reg_depth;
	int gen_backwards = ((argstype *)args)->gen_backwards;
	int secondforward = ((argstype *)args)->secondforward;
	
	if (!mynode->flag) {
		if	((mynode->type==ADD || mynode->type==MULT || mynode->type==ADDBIAS) || (gen_backwards && mynode->type==INPUT)) {
			// the current output of the DAG node (input, adder, etc.)
			if (gen_backwards==0 && secondforward==0)
				fprintf (myFile,"node%d,",mynode->id);
			else if (secondforward)
				fprintf (myFile,"node_sf%d,",mynode->id);
			else if (gen_backwards)
				fprintf (myFile,"node_bp%d,",mynode->id);
			
			/* // the historical outputs of each node, needed only for the outputs of neurons
			if (node_is_final_adder(mynode)) {
				for (int i=0;i<shift_reg_depth;i++) {
					fprintf (myFile,"node%d_d%d,",mynode->id,i);
				}
			} */
		}
		mynode->flag=1;
	}
}

node *get_correspondance_node (node *mynode,node **forwardprop) {
	// given a backprop node, find corresponding forward prop node
	int backlayer = mynode->layer;
	int forwardlayer = NUM_LAYERS - backlayer - 1;
	int neuron = mynode->neuron;
	
	// walk across layer
	node *forward_node = forwardprop[forwardlayer];
	for (int i=0;i<neuron;i++) {
		forward_node = forward_node->next;
	}
	return forward_node;
}

void gen_c_statement (node *mynode,void *args) {
	char suffix[1024];
	edge *myedge = mynode->in_edges;
	FILE *myFile = ((argstype *)args)->file;
	int shift_reg_depth = ((argstype *)args)->shift_reg_depth;
	int backprop = ((argstype *)args)->backprop;
	node *output_layer = ((argstype *)args)->output_layer;
	node **forwardprop = ((argstype *)args)->forwardprop;
	node **backwardprop = ((argstype *)args)->backwardprop;
	int layer = mynode->layer;
	int secondforward = ((argstype *)args)->secondforward;

	// only generate statements for add and multiply
	if (!mynode->flag) {

		// check if all predecessors have been generated
		int deps_satisfied=1;
		edge *pred = mynode->in_edges;
		while (pred) {
			if (!pred->edge->flag) deps_satisfied=0;
			pred=pred->next;
		}
	
		// make sure dependencies are satisfied before generating code for this node
		if (deps_satisfied) {

			// establish a suffix for backprop nodes
			if (secondforward)
				strcpy(suffix,"_sf");
			else if (backprop)
				strcpy(suffix,"_bp");
			else
				strcpy(suffix,"");

			// CASE 1:  forward prop output node
			if (mynode->type == OUTPUT && !backprop && !secondforward) {
				if (secondforward) return;
				fprintf(myFile,"\toutput%s%d = node%s%d;\n",suffix,mynode->id,suffix,myedge->edge->id);
			
			// CASE 2:  backward prop delta multiplier node
			} else if (mynode->delta_multiplier) {
				node *fpn = get_correspondance_node(mynode,forwardprop);
				//node *fpn = forwardprop[NUM_LAYERS-layer];
				
				if (fpn->type==INPUT)
					fprintf(myFile,"\tnode_bp%d = node_bp%d * node%d_d%d;\n",
								mynode->id,
								mynode->in_edges->edge->id,
								fpn->id,
								shift_reg_depth);
				else
					fprintf(myFile,"\tnode_bp%d = node_bp%d * node_sf%d;\n",
								mynode->id,
								mynode->in_edges->edge->id,
								fpn->id);
			
			// CASE 3:  backprop input node (which represents an output node in the forward prop)
			} else if (mynode->type == INPUT && backprop) {
				// SHOULD BE OUTPUT OF SECOND FORWARD PASS!
				fprintf (myFile,"\tnode_bp%d = (node_sf%d - input%d);\n",
							mynode->id,
							get_correspondance_node(mynode,forwardprop)->id,
							HISTORY_LENGTH-1);
				
				// update biases
				fprintf (myFile,"\tbias_fp%d[%d] -= LEARN_RATE * node_bp%d;\n",NUM_LAYERS-1,mynode->neuron,mynode->id);
				fprintf (myFile,"\tbias_bp%d[%d] -= LEARN_RATE * node_bp%d;\n",NUM_LAYERS-1,mynode->neuron,mynode->id);
				
				// update weights of output node coefficients
				node *mynode_forward = get_correspondance_node(mynode,forwardprop);
				
			// CASE 4:  all other nodes, assuming incoming edges
			} else if (myedge && !(mynode->type==OUTPUT && secondforward)) {
				// ** begin the statement
				if (myedge->edge->type == INPUT) {
					if (secondforward)
						fprintf(myFile,"\tnode%s%d = node%d_d%d ",
							suffix,mynode->id,myedge->edge->id,FORECAST_LENGTH);
					else if (backprop)
						fprintf (myFile,"\tnode%s%d = node_bp%d ",suffix,mynode->id,myedge->edge->id);
					else
						fprintf(myFile,"\tnode%s%d = input%d ",suffix,mynode->id,myedge->edge->id);
				} else {
					if (!backprop || mynode->type != OUTPUT) {
						fprintf(myFile,"\tnode%s%d = node%s%d ",suffix,mynode->id,suffix,myedge->edge->id);
					}
				}
				// ** finish the statement
				myedge = myedge->next;
				switch (mynode->type) {
					case ADD:
						fprintf (myFile,"+ node%s%d;\n",suffix,myedge->edge->id);
						
						// update bias in forward and backprop
						if (backprop && mynode->final_adder) {
							fprintf (myFile,"\tbias_bp%d[%d] -= LEARN_RATE * node_bp%d;\n",NUM_LAYERS-mynode->layer,mynode->neuron,mynode->id);
							fprintf (myFile,"\tbias_fp%d[%d] -= LEARN_RATE * node_bp%d;\n",NUM_LAYERS-mynode->layer,mynode->neuron,mynode->id);
						}
						
						break;
					case MULT:
						if (backprop)
							fprintf (myFile,"* coeff_bp%d[%d][%d];\n",NUM_LAYERS - mynode->layer,mynode->input_number,mynode->neuron);
						else
							fprintf (myFile,"* coeff_fp%d[%d][%d];\n",mynode->layer,mynode->neuron,mynode->input_number);
						
						break;
					case ADDBIAS:
						if (backprop)
							fprintf (myFile,"+ bias_bp%d[%d];\n",mynode->layer,mynode->neuron);
						else
							fprintf (myFile,"+ bias_fp%d[%d];\n",mynode->layer,mynode->neuron);
						break;
				}
			}

			// for backprop, add weight update code
			if (backprop && mynode->final_adder && !secondforward) {
			// find predecessor of the corresponding node in forward pass
				
				// for each outgoing edge (incoming edge in forward prop)
				
				int layer_in_forward_pass = NUM_LAYERS-(mynode->layer);
				
				// update biases
				if (NUM_LAYERS-mynode->layer-1)
					fprintf (myFile,"\tbias_fp%d[%d] -= LEARN_RATE * node_bp%d;\n"
									"\tbias_bp%d[%d] -= LEARN_RATE * node_bp%d;\n",
										NUM_LAYERS-mynode->layer-1,
										mynode->neuron,
										mynode->id,
										NUM_LAYERS-mynode->layer-1,
										mynode->neuron,
										mynode->id);
				
				for (node *n=forwardprop[layer_in_forward_pass];n;n=n->next) {
				
					if (get_correspondance_node(mynode,forwardprop)->type==INPUT)
						fprintf (myFile,
							"\tcoeff_fp%d[%d][%d] -= LEARN_RATE * node_bp%d * node%d_d%d;\n"
							"\tcoeff_bp%d[%d][%d] -= LEARN_RATE * node_bp%d * node%d_d%d;\n",
							NUM_LAYERS-mynode->layer, // original layer
							n->neuron, // edge number
							mynode->neuron, // output number
							get_correspondance_node(n,backwardprop)->id,
							get_correspondance_node(mynode,forwardprop)->id,
							shift_reg_depth,
							NUM_LAYERS-mynode->layer, // original layer
							n->neuron, // edge number
							mynode->neuron, // output number
							get_correspondance_node(n,backwardprop)->id,
							get_correspondance_node(mynode,forwardprop)->id,
							shift_reg_depth);
					else
						fprintf (myFile,
							"\tcoeff_fp%d[%d][%d] -= LEARN_RATE * node_bp%d * node_sf%d;\n"
							"\tcoeff_bp%d[%d][%d] -= LEARN_RATE * node_bp%d * node_sf%d;\n",
							NUM_LAYERS-mynode->layer, // original layer
							n->neuron, // edge number
							mynode->neuron, // output number
							get_correspondance_node(n,backwardprop)->id,
							get_correspondance_node(mynode,forwardprop)->id,
							NUM_LAYERS-mynode->layer, // original layer
							n->neuron, // edge number
							mynode->neuron, // output number
							get_correspondance_node(n,backwardprop)->id,
							get_correspondance_node(mynode,forwardprop)->id);
						
					//e++;
				}
			}

			mynode->flag = 1;
		}
	}
	
}

void clear_flags (node *mynode,void *args) {
	mynode->flag=0;
}

void compute_functional_utilization(node **layers,int num_layers,int num_inputs,int num_outputs,argstype *myargs) {
	
	// assume this is a safe way to find the maximum possible latency
	int max_latency = layers[num_layers]->alap_cycle;
	
	// allocate and initialize function unit usage counters
	myargs->add_use = (int *)malloc(sizeof(int) * max_latency);
	for (int i=0;i<max_latency;i++) myargs->add_use[i]=0;
	myargs->mult_use = (int *)malloc(sizeof(int) * max_latency);
	for (int i=0;i<max_latency;i++) myargs->mult_use[i]=0;
	
	traverse_dag(layers,num_layers,num_inputs,num_outputs,(void *)myargs,clear_flags,FROM_START);
	traverse_dag(layers,num_layers,num_inputs,num_outputs,(void *)myargs,inc_functional_utilization,FROM_START);
}

void inc_functional_utilization (node *mynode,void *args) {
	argstype *myargs = (argstype *)args;
	
	if (!mynode->flag) {
	
		for (int i=mynode->asap_cycle;i<=mynode->alap_cycle;i++) {
			if (mynode->type == ADD) myargs->add_use[i]++; else
			if (mynode->type == MULT) myargs->mult_use[i]++;
		}
	
		mynode->flag=1;
	}
}

void generate_hls_wrapper_code(const char *filename,node **layers) {
	char tmp[1024];
	FILE *myFile = fopen(filename,"w+");
	
	if (!myFile) {
		snprintf(tmp,1024,"Error opening \"%s\" for write",filename);
		perror(tmp);
		exit(1);
	}
	
	fprintf(myFile,"#include \"hls_stream.h\"\n"
				   "#include \"hls_math.h\"\n"
				   "#include \"shift_reg.h\"\n"
				   "#include \"ap_fixed.h\"\n"
				   "#include <cassert>\n\n");
	
	#ifdef DATATYPE_BASE
	fprintf(myFile,"typedef %s %s;\n\n",DATATYPE_BASE,DATATYPE);
	#endif
	
	fprintf (myFile,"#define\tHISTORY_LENGTH\t%d\n\n"
					"void mynetwork (",HISTORY_LENGTH);
					
	for (int i=0;i<HISTORY_LENGTH;i++) {
		if (i) fprintf(myFile,",");
		fprintf(myFile,"const %s input%d",DATATYPE,i);
	}
	
	fprintf(myFile,", %s &output%d);\n",DATATYPE,layers[NUM_LAYERS-1]->id);
					
	fprintf (myFile,"template<\n"
					"\tunsigned int shift_reg_depth\n"
					">\n"
					"void network_wrapper(\n"
					"\thls::stream<%s>& input0,\n"
					"\thls::stream<%s>& output0\n"
					") {\n\n"
					"\tstatic ShiftRegister<%s, shift_reg_depth> sreg;\n\n"
					"\tif (!input0.empty()) {\n"
					"\t\tsreg.shift_in(input0);\n\n",DATATYPE,DATATYPE,DATATYPE);
					
	
	fprintf (myFile,"\t\tfloat ");
	
	for (int i=0;i<HISTORY_LENGTH;i++) {
		if (i) fprintf(myFile,",");
		fprintf(myFile,"in%d",i);
	}
	
	fprintf (myFile,";\n\t\%s out0;\n\n",DATATYPE);
	
	for (int i=0;i<HISTORY_LENGTH;i++) {
		fprintf(myFile,"\t\tin%d = sreg[%d];\n",i,i);
	}
	
	fprintf (myFile,"\n\t\tmynetwork(");
	
	for (int i=0;i<HISTORY_LENGTH;i++) {
		if (i) fprintf(myFile,",");
		fprintf(myFile,"in%d",i);
	}
	
	fprintf (myFile,",out0);\n\n");
	
	fprintf (myFile,"\t\toutput0.write(out0);\n\n");
	
	fprintf (myFile,"\t}\n}\n\n"
					"void streaming_toplevel(\n"
					"\thls::stream<%s>& input0,"
					"\thls::stream<%s>& output0\n"
					") {\n"
					"#pragma HLS LATENCY max=1\n"
					"\tnetwork_wrapper<HISTORY_LENGTH>(input0, output0);\n"
					"}\n",DATATYPE,DATATYPE);
}

void count_registers (node *mynode,void *args) {
	if (!mynode->flag) {
		
		int start_cycle = mynode->scheduled_cycle;
		int latency = LATENCY(mynode->type);
		start_cycle += latency;
		int end_cycle = start_cycle;
		
		// check all outgoing edges
		edge *myedge = mynode->out_edges;
		while (myedge) {
			// find all cycles where output value is held
			
			if (myedge->edge->scheduled_cycle > end_cycle) end_cycle = myedge->edge->scheduled_cycle;
			
			//printf("%d -> %d cycles %d to %d\n",mynode->id,myedge->edge->id,start_cycle,end_cycle);
			
			myedge = myedge->next;
		}
		
		for (int i=start_cycle;i<end_cycle;i++)	((register_table *)args)->register_usage_by_cycle[i]++;
		
		mynode->flag=1;
	}
}

void gen_shift_registers (node *mynode,void *args) {
	int depth = ((argstype *)args)->shift_reg_depth;
	FILE *myFile = ((argstype *)args)->file;
	
	if (!mynode->flag) {
		// check if this is a final adder
		if (mynode->final_adder || mynode->type==INPUT) {
			// print declarations
			fprintf(myFile,"\tstatic %s ",DATATYPE);
			for (int i=0;i<=depth;i++) {
				if (i) fprintf(myFile,",");
				fprintf(myFile,"node%d_d%d",mynode->id,i);
			}
			fprintf(myFile,";\n");
			
			// print shift register
			for (int i=depth-1;i>=-1;i--) {
				if (i>=0)
					fprintf(myFile,"\tnode%d_d%d = node%d_d%d;\n",mynode->id,i+1,
																  mynode->id,i);
				else if (mynode->type == INPUT)
					fprintf(myFile,"\tnode%d_d%d = input%d;\n",mynode->id,i+1,
															  mynode->id);
				else	
					fprintf(myFile,"\tnode%d_d%d = node%d;\n",mynode->id,i+1,
																  mynode->id);
			}
		}
		
		mynode->flag=1;
	}
}

void tabulate_registers (node *layers[],int num_layers,int num_inputs,int num_outputs) {
	register_table myregistertable;
	
	int num_cycles = layers[num_layers]->scheduled_cycle;
	
	// allocate table
	myregistertable.register_usage_by_cycle = (int *)malloc(sizeof(int)*num_cycles);
	for (int i=0;i<num_cycles;i++) myregistertable.register_usage_by_cycle[i]=0;
		
	// clear flags
	traverse_dag (layers,
			  num_layers,
			  num_inputs,
			  num_outputs,
			  NULL,
			  clear_flags,
			  FROM_START);
	
	// tally registers
	traverse_dag (layers,
				  num_layers,
				  num_inputs,
				  num_outputs,
				  (void *)&myregistertable,
				  count_registers,
				  FROM_START);
				  
	printf ("register usage by cycle\n");
	printf ("%10s%10s\n","cycle","usage");
	for (int i=0;i<num_cycles;i++) {
		printf ("%10d%10d\n",i,myregistertable.register_usage_by_cycle[i]);
	}
}

void gen_debugging_statements (node *layers[],FILE *outFile) {
	int mlp_topology[] = MLP_TOPOLOGY;
	node *mynode;
	
	for (int i=0;i<NUM_LAYERS;i++) {
		mynode = layers[i];
		fprintf(outFile,"\tprintf(\"LAYER %d:\\n\");\n",i);
		for (int j=0;j<mlp_topology[i];j++) {
			fprintf(outFile,"\tprintf(\"node %d (node%d) = %%0.4e\\n\",%s%d);\n",j,mynode->id,NODETYPE_CODE(mynode->type),mynode->id);
			mynode = mynode->next;
		}
	}
}

void gen_header_file (int num_layers,
					  int *layer_sizes,
					  struct layer *trainer_layers) {
						  
	FILE *myFile = fopen("network.h","w+");
	
	if (!myFile) {
		perror("network.h");
		exit(1);
	}
	
	// learning rate
	fprintf(myFile,"#define	LEARN_RATE		(%s)%f\n",DATATYPE,LEARNING_RATE);
	fprintf(myFile,"#define LEARN_RATE_DUT	(float)%f\n\n",LEARNING_RATE);
	
	// data type
#ifdef DATATYPE_BASE
	fprintf(myFile,"typedef %s %s;\n\n",DATATYPE_BASE,DATATYPE);
#endif

	// prototypes
	fprintf(myFile,"#ifdef __cplusplus\n"
				   "extern \"C\" {\n"
				   "void mynetwork_dut (hls::stream<%s>& input_strm,hls::stream<%s>& output0_strm);\n"
				   "}\n"
				   "#endif\n\n","float","float");

	fprintf(myFile,"void mynetwork (hls::stream<%s>& input_strm,hls::stream<%s>& output0_strm);\n\n",
			DATATYPE,DATATYPE);

	// initial weights
	for (int i=1;i<NUM_LAYERS;i++) {
	
		fprintf(myFile,"#define LAYER%d_WEIGHTS	{",i);
		
		for (int j=0;j<layer_sizes[i];j++) {
			if (layer_sizes[i] > 1) fprintf(myFile,"{");
			for (int k=0;k<layer_sizes[i-1];k++) {
				if (k!=0) fprintf(myFile,",");
				fprintf(myFile,"%0.10e",trainer_layers[i].weights[j*HISTORY_LENGTH+k]);
			}
			if (j==layer_sizes[i]-1) fprintf(myFile,"}\\\n"); else fprintf(myFile,"},\\\n");
		}
		
		if (layer_sizes[i] > 1) fprintf(myFile,"}\n");
	}

	//fprintf(myFile,"\n");
	fclose(myFile);
}

void gen_c_code_loop_version (node **layers,
						node **back_layers,
						int num_layers,
						int *layer_sizes,
						FILE *myFile,
						int gen_backprop,
						int forecast_length,
						struct layer *trainer_layers) {
							
							
	char learn_rate_constant[1024];
	
	// headers
#ifdef GEN_NETWORK_DEBUG
	fprintf(myFile,"#include <stdio.h>\n");
#endif

	fprintf(myFile,"#include \"hls_stream.h\"\n"
				   "#include \"ap_fixed.h\"\n"
				   "#include \"network.h\"\n\n");
	
	for (int func=1;func>=0;func--) {
	
		// LEARN_RATE should be different for the hardware and software versions of the function
		if (func==0)
			strcpy(learn_rate_constant,"LEARN_RATE");
		else
			strcpy(learn_rate_constant,"LEARN_RATE_DUT");
	
		char data_type[1024],suffix[1024];
		if (func==0) {
			sprintf(data_type,"%s",DATATYPE);
			strcpy(suffix,"");
		} else {
			strcpy(data_type,"float");
			strcpy(suffix,"_dut");
		}
	
		fprintf(myFile,"void mynetwork%s (hls::stream<%s>& input_strm,hls::stream<%s>& output0_strm) {\n\n",suffix,data_type,data_type);
		
		fprintf(myFile,"// limit the number of functional units to avoid oversubscription\n"
					   "#pragma HLS ALLOCATION instances=mul limit=%d operation\n"
					   "#pragma HLS ALLOCATION instances=add limit=%d operation\n"
					   "#pragma HLS ALLOCATION instances=sub limit=%d operation\n\n",NUM_MULTIPLIERS,NUM_ADDERS,NUM_ADDERS);
		
		// GENERATE COEFFICIENTS WITH INITIALIZATION
		// forward pass 1 coefficients

		// we need three copies of the weights for this algorithm, so generate three identical
		// structures with identical initializations.  the third copy needs only the output layer
		// weights

		for (int memory_copy=0;memory_copy<3;memory_copy++) {
			for (int i=1;i<num_layers;i++) {
				char suffix[20];
				
				// don't generate backups for layer 1
				if (memory_copy==2 && i==1) continue;
				
				// suffixes for the three copies of the parameter memories
				if (memory_copy==0)
					strcpy(suffix,"fp");
				else if (memory_copy==1)
					strcpy(suffix,"bp");
				else
					strcpy(suffix,"backup");
				
				// use 1D array for layers with one neuron
				if (layer_sizes[i] > 1)
					fprintf(myFile,"\tstatic %s coeff_%s%d[%d][%d]=LAYER%d_WEIGHTS;\n",data_type,suffix,i,layer_sizes[i],layer_sizes[i-1],i);
				else
					fprintf(myFile,"\tstatic %s coeff_%s%d[%d]=LAYER%d_WEIGHTS;\n",data_type,suffix,i,layer_sizes[i-1],i);
				/*
				for (int j=0;j<layer_sizes[i];j++) {
					if (layer_sizes[i] > 1) fprintf(myFile,"{");
					for (int k=0;k<layer_sizes[i-1];k++) {
						if (k!=0) fprintf(myFile,",");
						fprintf(myFile,"%0.10e",trainer_layers[i].weights[j*HISTORY_LENGTH+k]);
					}
					if (j==layer_sizes[i]-1) fprintf(myFile,"}\n"); else fprintf(myFile,"},\n");
				}
				if (layer_sizes[i] > 1)
					fprintf(myFile,"};\n");
				else
					fprintf(myFile,";\n");
				*/
				// pragmas
				// NOTE: this is hacky--need a better way!
				if (i==1) {
					fprintf(myFile,"#pragma HLS ARRAY_PARTITION variable=coeff_%s%d cyclic factor=%d dim=2\n"
								   "#pragma HLS RESOURCE variable=coeff_fp%d core=RAM_T2P_BRAM\n\n",
								   suffix,i,layer_sizes[i-1] > LAYER1_MEMORY_BANKS ? LAYER1_MEMORY_BANKS : layer_sizes[i-1],i);
				} else {
					fprintf(myFile,"#pragma HLS ARRAY_PARTITION variable=coeff_%s%d complete dim=1\n",
								   suffix,i);
				}
				
				fprintf(myFile,"\tstatic %s bias_%s%d[%d]={",data_type,suffix,i,layer_sizes[i]);
				for (int j=0;j<layer_sizes[i];j++) {
					if (j) fprintf(myFile,",");
					fprintf(myFile,"0.0");
				}
				fprintf(myFile,"};\n");
				fprintf(myFile,"#pragma HLS ARRAY_PARTITION variable=bias_%s%d complete dim=1\n\n",suffix,i);
			}
		}
		
		// temporary variables
		fprintf(myFile,"// temporary value\n"
					   "\t%s neuron_out;\n\n"
					   "// the shift register for remembering historical inputs, for both forward passes\n"
					   "\tstatic %s inputs[%d];\n"
					   "#pragma HLS ARRAY_PARTITION variable=inputs cyclic factor=%d dim=1\n\n",
					   data_type,data_type,HISTORY_LENGTH+FORECAST_LENGTH,(HISTORY_LENGTH+FORECAST_LENGTH) > LAYER1_MEMORY_BANKS ? LAYER1_MEMORY_BANKS : (HISTORY_LENGTH+FORECAST_LENGTH));
					   
		// shift registers
		fprintf(myFile,"// shift in the new input value\n"
					   "\tshift_reg_loop: for (int i=%d;i>=1;i--) {\n"
					   "#pragma HLS UNROLL\n"
					   "\t\tinputs[i]=inputs[i-1];\n"
					   "\t}\n"
					   "\tinputs[0] = input_strm.read();\n\n",HISTORY_LENGTH+FORECAST_LENGTH-1);
		
		int ports = 2*(layer_sizes[0] > LAYER1_MEMORY_BANKS ? layer_sizes[0] : LAYER1_MEMORY_BANKS);
		int expected_II = (layer_sizes[0] + ports - 1) / ports;
		
		// forward pass 1
		fprintf(myFile,"// ********************************\n"
					   "// forward pass 1\n"
					   "// ********************************\n"
					   "\t%s output_current = 0;\n"
					   "\tfp1_loop: for (int i=0;i<%d;i++) {\n"
					   "#pragma HLS PIPELINE II=%d\n"
					   "\t\t%s sum = 0;\n"
					   "\t\tinner_loop_fp1: for (int j=0;j<%d;j++) {\n"
					   "\t\t\tsum += inputs[j] * coeff_fp1[i][j];\n"
					   "\t\t}\n"
					   "\t\tneuron_out = sum + bias_fp1[i];\n"
					   "\t\toutput_current += coeff_fp2[i] * neuron_out; // output layer\n"
					   "\t}\n"
					   "\toutput_current += bias_fp2[0];\n"
					   "\toutput0_strm.write(output_current);\n\n",
					   data_type,layer_sizes[1],expected_II,data_type,layer_sizes[0]);
					   
		// forward pass 2
		fprintf(myFile,"// ********************************\n"
					   "// forward pass 2\n"
					   "// ********************************\n"
					   "\t%s bp_hidden[%d];\n"
					   "#pragma HLS ARRAY_PARTITION variable=bp_hidden complete dim=1\n\n",
					   data_type,layer_sizes[1]);
					   
		fprintf(myFile,"\t%s output_past = 0;\n"
					   "\tfp2_loop: for (int i=0;i<%d;i++) {\n"
					   "#pragma HLS PIPELINE II=%d\n"
					   "\t\t%s sum = 0;\n"
					   "\t\tinner_loop_fp2: for (int j=0;j<%d;j++) {\n"
					   "\t\t\tsum += inputs[j+%d] * coeff_bp1[i][j];\n"
					   "\t\t}\n"
					   "\t\tneuron_out = bp_hidden[i] = sum + bias_fp1[i];\n"
					   
					   "\t\tcoeff_backup2[i] = coeff_bp2[i] * neuron_out;\n"
					   "\t\toutput_past += coeff_backup2[i];\n"
					   
					   //"\t\toutput_past += coeff_bp2[i] * neuron_out;\n"
					   "\t}\n\n"
					   "\toutput_past += bias_bp2[0];\n\n",
					   data_type,layer_sizes[1],expected_II,data_type,HISTORY_LENGTH,FORECAST_LENGTH);
		
		// backpropagation
		fprintf(myFile,"\t// ********************************\n"
					   "\t// backpropagation code\n"
					   "\t// ********************************\n\n");

		fprintf(myFile,"\t// delta for output neuron\n"
					   "\t%s node_bp0 = (output_past - inputs[0]);\n\n",data_type);
					   
		fprintf(myFile,"\t%s deltas[%d];\n"
					   "#pragma HLS ARRAY_PARTITION variable=deltas complete dim=1\n\n",
					   data_type,layer_sizes[1]);
					   
		fprintf(myFile,"// ********************************\n"
					   "// delta loop hidden layer\n"
					   "// ********************************\n");
					   
		fprintf(myFile,"\tdelta_loop: for (int i=0;i<%d;i++) {\n"
					   "#pragma HLS UNROLL\n"
					   //"\t\tdeltas[i] = node_bp0 * coeff_backup2[i] * bp_hidden[i];\n"
					   "\t\tdeltas[i] = node_bp0 * coeff_backup2[i];\n"
					   "\t}\n\n",
					   layer_sizes[1]);
		
		fprintf(myFile,"\t// ********************************\n"
					   "\t// weight update loop output layer\n"
					   "\t// ********************************\n");
					   
		fprintf(myFile,"\tupdate_output_layer_loop: for (int i=0;i<%d;i++) {\n"
					   "#pragma HLS UNROLL\n"
					   "\t\tcoeff_fp2[i] -= %s * node_bp0 * bp_hidden[i];\n"
					   "\t\tcoeff_bp2[i] -= %s * node_bp0 * bp_hidden[i];\n"
					   //"\t\tcoeff_backup2[i] -= %s * node_bp0 * bp_hidden[i];\n"
					   "\t}\n"
					   "\tbias_fp2[0] -= %s * node_bp0;\n"
					   "\tbias_bp2[0] -= %s * node_bp0;\n\n",
					   //layer_sizes[1],learn_rate_constant,learn_rate_constant,learn_rate_constant,learn_rate_constant,learn_rate_constant);
					   layer_sizes[1],learn_rate_constant,learn_rate_constant,learn_rate_constant,learn_rate_constant);
					   
		fprintf(myFile,"// ********************************\n"
					   "// weight update loop hidden layer\n"
					   "// ********************************\n");
					   
		fprintf(myFile,"\tupdate_hidden_layer_outer_loop: for (int i=0;i<%d;i++) {\n"
					   "#pragma HLS PIPELINE II=1\n"
					   "\t\tupdate_hidden_layer_inner_loop: for (int j=0;j<%d;j++) {\n"
					   "#pragma HLS UNROLL\n"
					   "\t\t\tcoeff_fp1[i][j] -= %s * deltas[i] * inputs[j+%d];\n"
					   "\t\t\tcoeff_bp1[i][j] -= %s * deltas[i] * inputs[j+%d];\n"
					   "\t\t}\n"
					   "\t\tbias_fp1[i] -= %s * deltas[i];\n"
					   "\t\tbias_bp1[i] -= %s * deltas[i];\n"
					   "\t}\n"
					   "}\n\n",
						layer_sizes[1],HISTORY_LENGTH,learn_rate_constant,FORECAST_LENGTH,learn_rate_constant,FORECAST_LENGTH,learn_rate_constant,learn_rate_constant);
	}
}

void gen_c_code (node **layers,
						node **back_layers,
						int num_layers,
						int *layer_sizes,
						FILE *myFile,
						int gen_backprop,
						int forecast_length,
						struct layer *trainer_layers) {
							
	// headers
#ifdef GEN_NETWORK_DEBUG
	fprintf(myFile,"#include <stdio.h>\n#include \"ap_fixed.h\"\n\n");
#endif

	fprintf(myFile,"#include \"ap_fixed.h\"\n\n");
	
	fprintf(myFile,"#define	LEARN_RATE	(%s)%f\n\n",DATATYPE,LEARNING_RATE);
	
#ifdef DATATYPE_BASE
	fprintf(myFile,"typedef %s %s;\n\n",DATATYPE_BASE,DATATYPE);
#endif
	
	// STEP 1:  GENERATE FUNCTION PROTOTYPE
	fprintf(myFile,"void mynetwork (");
	
	// STEP 2:  GENERATE INPUT ARGUMENTS
	int num_inputs = layer_sizes[0];
	node *mynode = layers[0];
	for (int i=0;i<num_inputs;i++) {
		fprintf(myFile,"const %s input%d,",DATATYPE,mynode->id);
		mynode = mynode->next;
	}
	
	int num_outputs = layer_sizes[NUM_LAYERS-1];

	// STEP 3:  GENERATE OUTPUT ARGUMENTS
	mynode = layers[num_layers];
	for (int i=0;i<num_outputs;i++) {
		fprintf(myFile,"%s &output%d",DATATYPE,mynode->id);
		if (i!=num_outputs-1) fprintf(myFile,",");
		mynode = mynode->next;
	}
		
	fprintf(myFile,") {\n");
	
	// STEP 4:  GENERATE COEFFICIENTS WITH INITIALIZATION
	for (int i=1;i<num_layers;i++) {
		fprintf(myFile,"\tstatic %s coeff_fp%d[%d][%d]={",DATATYPE,i,layer_sizes[i],layer_sizes[i-1]);
		
		for (int j=0;j<layer_sizes[i];j++) {
			fprintf(myFile,"{");
			for (int k=0;k<layer_sizes[i-1];k++) {
				if (k!=0) fprintf(myFile,",");
				fprintf(myFile,"%0.10e",trainer_layers[i].weights[j*num_inputs+k]);
			}
			if (j==layer_sizes[i]-1) fprintf(myFile,"}\n"); else fprintf(myFile,"},\n");
		}
		fprintf(myFile,"};\n");
	}
	for (int i=1;i<num_layers;i++) {
		fprintf(myFile,"\tstatic %s coeff_bp%d[%d][%d]={",DATATYPE,i,layer_sizes[i],layer_sizes[i-1]);
		
		for (int j=0;j<layer_sizes[i];j++) {
			fprintf(myFile,"{");
			for (int k=0;k<layer_sizes[i-1];k++) {
				if (k!=0) fprintf(myFile,",");
				fprintf(myFile,"%0.10e",trainer_layers[i].weights[j*num_inputs+k]);
			}
			if (j==layer_sizes[i]-1) fprintf(myFile,"}\n"); else fprintf(myFile,"},\n");
		}
		fprintf(myFile,"};\n");
	}
	
	// STEP 5:  GENERATE BIASES WITH INITIALIZATION
	for (int i=1;i<num_layers;i++) {
		fprintf(myFile,"\tstatic %s bias_fp%d[%d]={",DATATYPE,i,layer_sizes[i]);
		for (int j=0;j<layer_sizes[i];j++) {
			if (j) fprintf(myFile,",");
			fprintf(myFile,"%0.0f",trainer_layers[i].biases[j]);
		}
		fprintf(myFile,"};\n");
	}
	for (int i=1;i<num_layers;i++) {
		fprintf(myFile,"\tstatic %s bias_bp%d[%d]={",DATATYPE,i,layer_sizes[i]);
		for (int j=0;j<layer_sizes[i];j++) {
			if (j) fprintf(myFile,",");
			fprintf(myFile,"%0.0f",0.f/*trainer_layers[i].biases[j]*/);
		}
		fprintf(myFile,"};\n");
	}
	
	// STEP 6:  GENERATE FORWARD PROP VARIABLES
	fprintf(myFile,"\%s ",DATATYPE);
	// set up the traversal arguments
	argstype myargs = {.file = myFile,
					   .gen_backwards=0,
					   .output_layer=layers[num_layers],
					   .num_layers = num_layers,
					   .secondforward=0,
					   .shift_reg_depth = forecast_length,
					   .forwardprop = layers,
					   .backwardprop = back_layers};
					   
	// slight hack to avoid unnecessary delay lines when not training online
#ifndef ONLINE_TRAINING
	myargs.shift_reg_depth=0;
#endif
	// clear flags, so we declare each variable only once
	traverse_dag(layers,num_layers,num_inputs,num_outputs,(void *)&myargs,clear_flags,FROM_START);
	traverse_dag(layers,num_layers,num_inputs,num_outputs,(void *)&myargs,gen_c_declarations,FROM_START);
	fseek(myFile,-1,SEEK_CUR);
	fprintf(myFile,";\n");
	
	if (gen_backprop) {
		// STEP 6A:  GENERATE SECONDARY FORWARD PROP VARIABLES
		fprintf(myFile,"\t%s ",DATATYPE);
		myargs.secondforward=1;
		traverse_dag(layers,num_layers,num_inputs,num_outputs,(void *)&myargs,clear_flags,FROM_START);
		traverse_dag(layers,num_layers,num_inputs,num_outputs,(void *)&myargs,gen_c_declarations,FROM_START);
		fseek(myFile,-1,SEEK_CUR);
		fprintf(myFile,";\n");
	}
	
	// delete previous comma
	fseek(myFile,-1,SEEK_CUR);
	fprintf(myFile,";\n");
	
#ifdef ONLINE_TRAINING
	// STEP 6B:  GENERATE BACKPROP VARIABLES
	if (gen_backprop) {
		
		fprintf(myFile,"\t%s ",DATATYPE);
		
		myargs.secondforward=0;
		myargs.gen_backwards=1;
		// generate backprop and weight update code
		traverse_dag(back_layers,num_layers,num_inputs,num_outputs,(void *)&myargs,clear_flags,FROM_START);
		traverse_dag(back_layers,num_layers,num_outputs,num_inputs,(void *)&myargs,gen_c_declarations,FROM_START);
		
		// delete previous comma
		fseek(myFile,-1,SEEK_CUR);
		fprintf (myFile,";\n");
		
		/*
		// generate the shift registers
		traverse_dag(layers,num_layers,num_inputs,num_outputs,(void *)&myargs,clear_flags,FROM_START);
		traverse_dag(layers,num_layers,num_inputs,num_outputs,(void *)&myargs,gen_shift_registers,FROM_START);
		*/
		
		// GENERATE SHIFT REGISTERS FOR INPUTS
		traverse_dag(layers,num_layers,num_inputs,num_outputs,(void *)&myargs,clear_flags,FROM_START);
		fprintf(myFile,"\n\t// generate shift registers for inputs\n");
		mynode = layers[0];
		for (int i=0;i<num_inputs;i++) {
/* 			fprintf(myFile,"\tstatic %s ",DATATYPE);
			for (int j=0;j<FORECAST_LENGTH;j++) {
				fprintf(myFile,"input%d_d%d,",mynode->id,j+1);
			}
			// delete previous comma
			fseek(myFile,-1,SEEK_CUR);
			fprintf (myFile,";\n"); */
			// generate the shift code
			gen_shift_registers(mynode,(void *)&myargs);
			mynode = mynode->next;
		}
	}
#endif
	
	// STEP 7:  GENERATE CODE FOR FORWARD PASS
	
	fprintf(myFile,"\n\t// forward pass code\n");
	myargs.backprop=0;
	myargs.secondforward=0;
	traverse_dag(layers,num_layers,num_inputs,num_outputs,(void *)&myargs,clear_flags,FROM_START);
	traverse_dag(layers,num_layers,num_inputs,num_outputs,(void *)&myargs,gen_c_statement,FROM_START);
	
	if (gen_backprop) {
		// STEP 8:  GENERATE CODE FOR SECONDARY FORWARD PASS
		fprintf(myFile,"\n\t// secondary forward pass code\n");
		myargs.backprop=0;
		myargs.secondforward=1;
		traverse_dag(layers,num_layers,num_inputs,num_outputs,(void *)&myargs,clear_flags,FROM_START);
		traverse_dag(layers,num_layers,num_inputs,num_outputs,(void *)&myargs,gen_c_statement,FROM_START);
		
		// STEP 9:  GENERATE BACKPROP PASS
		fprintf(myFile,"\n\t// backpropagation code\n");
		myargs.backprop=1;
		myargs.secondforward=0;
		traverse_dag(back_layers,num_layers,num_outputs,num_inputs,(void *)&myargs,clear_flags,FROM_START);
		traverse_dag(back_layers,num_layers,num_outputs,num_inputs,(void *)&myargs,gen_c_statement,FROM_START);
	}
	
	//fprintf(myFile,"\t}\n");
	
#ifdef GEN_NETWORK_DEBUG
	gen_debugging_statements(layers,myFile);
#endif
	
	fprintf(myFile,"}\n");
}
