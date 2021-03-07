#include "netscheduler.h"

void connect_nodes (node *pred,node *succ) {
	edge *myedge;

	// add incoming edge
	if (succ->in_edges == NULL) {
		succ->in_edges = (edge *)malloc(sizeof(edge));
		succ->in_edges->edge = pred;
		succ->in_edges->next = NULL;
	} else {
		myedge = succ->in_edges;
		while (myedge->next) myedge = myedge->next;
		myedge->next = (edge *)malloc(sizeof(edge));
		myedge->next->edge = pred;
		myedge->next->next = NULL;
	}

	// add outgoing edge
	myedge = pred->out_edges;
	if (myedge == NULL) {
		pred->out_edges = (edge *)malloc(sizeof(edge));
		pred->out_edges->edge = succ;
		pred->out_edges->next = NULL;
	} else {
		edge *myedge = pred->out_edges;
		while (myedge->next) myedge = myedge->next;
		myedge->next = (edge *)malloc(sizeof(edge));
		myedge->next->edge = succ;
		myedge->next->next = NULL;
	}

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
		mynode = layers[num_layers-1];
	}
	
	// add first level
	for (int i=0;i<nodes_in_start_layer;i++,mynode=mynode->next) {
		// push start node
		stack[tail]=mynode;
		tail = (tail + 1) % QUEUESIZE;
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
				fprintf(stderr,"Fatal: traversal queue size exceeded.\n");
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
	
	switch (mynode->type) {
		case INPUT: strcpy(node_shape,"cds");break;
		case OUTPUT: strcpy(node_shape,"cds");break;
		case MULT: strcpy(node_shape,"box");break;
		case ADD: strcpy(node_shape,"ellipse");break;
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
	
	traverse_dag(layers,num_layers,num_inputs,num_outputs,(void *)&myargs,node2dot,FROM_END);

	fprintf(myFile,"}\n");
	fclose(myFile);
}

void gen_c_declarations (node *mynode,void *args) {
	FILE *myFile = ((argstype *)args)->file;
	
	if (!mynode->flag && (mynode->type==ADD || mynode->type==MULT)) {
		fprintf (myFile,"node%d,",mynode->id);
		mynode->flag=1;
	}
}

void gen_c_statement (node *mynode,void *args) {
	edge *myedge = mynode->in_edges;
	FILE *myFile = ((argstype *)args)->file;
	
	// only generate statements for add and multiply
	if ((!mynode->flag) && (mynode->type == ADD || mynode->type == MULT)) {
	
		// check for special identifiers for inputs and outputs
		if (myedge->edge->type == INPUT)
			fprintf(myFile,"\t\tnode%d = input%d[i] ",mynode->id,myedge->edge->id);
		else if (mynode->out_edges->edge->type == OUTPUT)
			fprintf(myFile,"\t\toutput%d[i] = node%d ",mynode->id,myedge->edge->id);
		else
			fprintf(myFile,"\t\tnode%d = node%d ",mynode->id,myedge->edge->id);
		
		// finish statement
		myedge = myedge->next;
		switch (mynode->type) {
			 case ADD:
				fprintf (myFile,"+ node%d;\n",myedge->edge->id);
				break;
			 case MULT:
				fprintf (myFile,"* coeff%d[%d][%d];\n",mynode->layer,mynode->neuron,mynode->input_number);
				break;
		}
	
		mynode->flag = 1;
	}
}

void clear_flags (node *mynode,void *args) {
	mynode->flag=0;
}

void compute_functional_utilization(node **layers,int num_layers,int num_inputs,int num_outputs,argstype *myargs) {
	
	// assume this is a safe way to find the maximum possible latency
	int max_latency = layers[num_layers-1]->alap_cycle;
	
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

void tabulate_registers (node *layers[],int num_layers,int num_inputs,int num_outputs) {
	register_table myregistertable;
	
	int num_cycles = layers[num_layers-1]->scheduled_cycle;
	
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

void gen_c_code (node **layers,
						int num_layers,
						int num_inputs,
						int hidden_layer_size,
						int num_outputs,
						const char *filename) {
							
	FILE *myFile = fopen(filename,"w+");
	char str[1024];
	
	fprintf(myFile,"void mynetwork (");
	
	// print input arguments
	node *mynode = layers[0];
	for (int i=0;i<num_inputs;i++) {
		fprintf(myFile,"const float input%d[1024]",mynode->id);
		fprintf(myFile,",");
		mynode = mynode->next;
	}
	
	// print output arguments
	mynode = layers[num_layers-1];
	for (int i=0;i<num_outputs;i++) {
		fprintf(myFile,"float output%d[1024]",mynode->in_edges->edge->id);
		if (i!=num_outputs-1) fprintf(myFile,",");
		mynode = mynode->next;
	}
		
	fprintf(myFile,") {\n");
	
	if (!myFile) {
		snprintf (str,1024,"ERROR: opening \"%s\" for write",filename);
		perror(str);
		exit(1);
	}
	
	argstype myargs = {.file = myFile};
	
	// add the variable declarations
	fprintf(myFile,"\tfloat ");
	
	// clear to flags, so we declare each variable only once
	traverse_dag(layers,num_layers,num_inputs,num_outputs,(void *)&myargs,clear_flags,FROM_START);
	
	// generate the intermediate value declarations
	traverse_dag(layers,num_layers,num_inputs,num_outputs,(void *)&myargs,gen_c_declarations,FROM_START);
	
	// add the coefficient array
	fprintf(myFile,"coeff1[%d][%d],coeff2[%d][%d];\n",hidden_layer_size,num_inputs,num_outputs,hidden_layer_size);
	fprintf(myFile,"\n");
	
	// generate the loop
	//fprintf(myFile,"\tfor (int i=0;i<1024;i++) {\n#pragma HLS PIPELINE II=1\n");
	fprintf(myFile,"\tfor (int i=0;i<1024;i++) {\n#pragma HLS LATENCY max=1\n");
	traverse_dag(layers,num_layers,num_inputs,num_outputs,(void *)&myargs,clear_flags,FROM_START);
	traverse_dag(layers,num_layers,num_inputs,num_outputs,(void *)&myargs,gen_c_statement,FROM_START);
	fprintf(myFile,"\t}\n");
	
	fprintf(myFile,"}\n");
	
	fclose(myFile);
}

/**
 * @brief Performs the backwards pass for the MLP network.
 *
 * @param layers
 * @param num_layers
 * @param num_inputs
 * @param hidden_layer_size
 * @param num_outputs
 * @param filename
 */
void gen_backwards_pass(node **layers,
						int num_layers,
						int num_inputs,
						int hidden_layer_size,
						int num_outputs,
						const char *filename) {

	FILE *myFile = fopen(filename,"w+");
	char str[1024];
	
	fprintf(myFile,"void mynetwork_backwards (");

	// print input arguments
	node *mynode = layers[0];
	for (int i=0;i<num_inputs;i++) {
		fprintf(myFile,"const float input%d[1024]",mynode->id);
		fprintf(myFile,",");
		mynode = mynode->next;
	}
	
	// print output arguments
	mynode = layers[num_layers-1];
	for (int i=0;i<num_outputs;i++) {
		fprintf(myFile,"float output%d[1024]",mynode->in_edges->edge->id);
		if (i!=num_outputs-1) fprintf(myFile,",");
		mynode = mynode->next;
	}

	fprintf(myFile,") {\n");
	
	if (!myFile) {
		snprintf (str,1024,"ERROR: opening \"%s\" for write",filename);
		perror(str);
		exit(1);
	}

	argstype myargs = {.file = myFile};

	// clear to flags, so we declare each variable only once
	traverse_dag(layers,num_layers,num_inputs,num_outputs,(void *)&myargs,clear_flags,FROM_START);
	
	// generate the intermediate value declarations
	traverse_dag(layers,num_layers,num_inputs,num_outputs,(void *)&myargs,gen_c_declarations,FROM_START);
	
	// add the coefficient array
	fprintf(myFile,"coeff1[%d][%d],coeff2[%d][%d];\n",hidden_layer_size,num_inputs,num_outputs,hidden_layer_size);
	fprintf(myFile,"\n");

	/* generate the delta array, we need one matrix for each hidden layer,
	 * and for the output layer */
	fprintf(myFile, "delta%d[%d]; /* deltas for output layer */\n", num_layers, num_outputs);

	for (int layerno = 0 ; layerno < num_layers; layerno++) {
		fprintf(myFile, "delta%d[%d]; /* deltas for layer %d */\n", layerno, hidden_layer_size, layerno);
	}

	fprintf(myFile,"\tfor (int i=0;i<1024;i++) {\n#pragma HLS LATENCY max=1\n");

	/* Populate the final delta array for the output layer - note that
	 * because we currently only support linear MLPs, g'() is 1 for any
	 * input, leaving us with just the y_j - a_j term. */
	fprintf(myFile, "\t\t/* back-propagation for output layer */\n");
	for (int i = 0 ; i < num_outputs ; i++) {
		fprintf(myFile, "\t\tdelta%d[%d] = input[i] - node%d;\n", num_layers, i, layers[num_layers-1][i].id);
	}

	fprintf(myFile, "\t\t/* back-propagation for hidden layers */\n");
	for (int layerno = 0 ; layerno < num_layers; layerno++) {
		fprintf(myFile, "\t\t/* layer %d */\n", layerno);
		fprintf(myFile, "\t\tdelta%d[%d] = 0;\n");
		for (int i = 0; i < hidden_layer_size ; i++) {
			for (int j = 0; j < ((i == num_layers-2) ? num_outputs : hidden_layer_size) ; j++) {
				fprintf(myFile, "\t\tdelta%d[%d] += coeff%d[%d][%d] * delta%d[%d];\n",
						layerno, i, layerno, i, j, layerno+1, j);
			}
		}
	}

	fprintf(myFile,"\t}\n");
	
	fprintf(myFile,"}\n");
	
	fclose(myFile);

}