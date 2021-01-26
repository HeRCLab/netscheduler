#include "netscheduler.h"

void set_asaps (node *mynode,void *args) {
	if (mynode->type == INPUT) mynode->asap_cycle = 0;

	edge *myedge = mynode->out_edges;
	while (myedge) {
		int latency = LATENCY(mynode->type);
		myedge->edge->asap_cycle = max(myedge->edge->asap_cycle,mynode->asap_cycle + latency);
		myedge = myedge->next;
	}
}

void set_alaps (node *mynode,void *args) {
	//if (mynode->type==OUTPUT) mynode->alap_cycle = mynode->asap_cycle;
	
	edge *myedge = mynode->in_edges;
	while (myedge) {
		int latency_of_current_node = LATENCY(myedge->edge->type);
		myedge->edge->alap_cycle = mynode->alap_cycle - latency_of_current_node;
		myedge = myedge->next;
	}
}

void count_registers (node *mynode,void *args) {
	if (!mynode->flag) {
		
		// check all outgoing edges
		edge *myedge = mynode->out_edges;
		while (myedge) {
			// find all cycles where output value is held
			int start_cycle = mynode->scheduled_cycle+LATENCY(mynode->type);
			int end_cycle = myedge->edge->scheduled_cycle;
			
			for (int i=start_cycle;i<end_cycle;i++)	((register_table *)args)->register_usage_by_cycle[i]++;
			
			myedge = myedge->next;
		}
		
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

void schedule (node *layers[],int num_layers,int num_inputs,int num_outputs) {
	argstype myargs;
	
	// set asaps
	traverse_dag (layers,num_layers,num_inputs,num_outputs,(void *)&myargs,set_asaps,FROM_START);
	
	// set latency slack
	layers[num_layers-1]->alap_cycle = layers[num_layers-1]->asap_cycle + SLACK;
	
	// set alaps
	traverse_dag (layers,num_layers,num_inputs,num_outputs,(void *)&myargs,set_alaps,FROM_END);
	
	// set alaps for inputs, which will hopefully allow for II > 1 (experimental)
	node *mynode = layers[0];
	for (int i=0;i<num_inputs;i++) {
		mynode->alap_cycle = MAX_II;
		mynode = mynode->next;
	}
}

void emit_resource_constraints (node *mynode,void *args) {
	argstype *myargs = (argstype *)args;
	FILE *myFile = myargs->file;
	int cycle = myargs->cycle;
	node_type type = myargs->type;
	int *first = &myargs->first;
	
	// first, check if we already processed this node to avoid
	// multiple instances of the same node in one constraint
	if (!mynode->flag) {
		
		// next, check if there is any potential for using all of the resources
		// in this cycle
		if ((mynode->type == ADD && myargs->add_use[cycle] > NUM_ADDERS) ||
			(mynode->type == MULT && myargs->mult_use[cycle] > NUM_MULTIPLIERS)) {
		
			// finally, check if the node can potentially be used in this cycle
			if (mynode->type == type &&
				mynode->asap_cycle <= cycle &&
				mynode->alap_cycle >= cycle) {
				
				// join variables with addition
				if (!*first) {
					fprintf(myFile," + ");
				} else {
					*first=0;
				}
				
				fprintf(myFile,"n_%d_c_%d",mynode->id,cycle);
			}
		}
		mynode->flag=1;
	}
}

void emit_start_and_dependency_constraints (node *mynode,void *args) {
	FILE *myFile = ((argstype *)args)->file;
	
	// add unique start time constraint
	fprintf (myFile,"\\ start time constraint\n");
	for (int i=mynode->asap_cycle;i<=mynode->alap_cycle;i++) {
		if (i!=mynode->asap_cycle) fprintf (myFile," + ");
		fprintf(myFile,"n_%d_c_%d",mynode->id,i);
	}
	fprintf (myFile," = 1\n");
	
	// add data dependency constraints
	fprintf (myFile,"\\ data dependency constraint\n");
	edge *myedge = mynode->in_edges;
	while (myedge) {
		node *pred = myedge->edge;
		for (int i=mynode->asap_cycle;i<=mynode->alap_cycle;i++) {
			if (i!=mynode->asap_cycle) fprintf (myFile," + ");
			fprintf(myFile,"%d n_%d_c_%d",i,mynode->id,i);
		}
		
		for (int i=pred->asap_cycle;i<=pred->alap_cycle;i++) {
			//if (i!=pred->asap_cycle) fprintf (myFile," + ");
			fprintf(myFile," - %d n_%d_c_%d",i,pred->id,i);
		}
		
		fprintf(myFile," >= %d\n",LATENCY(pred->type));
		myedge = myedge->next;
	}
}

void generate_declarations (node *mynode,void *args) {
	argstype *myargs = (argstype *)args;
	FILE *myFile = myargs->file;
	
	if (!mynode->flag) {
		for (int i=mynode->asap_cycle;i<=mynode->alap_cycle;i++)
			fprintf(myFile,"n_%d_c_%d\n",mynode->id,i);
		mynode->flag=1;
	}
}

void generate_ilp_file (node **layers,
						int num_layers,
						int num_inputs,
						int num_outputs,
						char *filename,
						argstype *myargs) {
							
	int last_cycle = layers[num_layers-1]->alap_cycle;
	FILE *myFile;
	char str[1024];
	
	myFile=fopen(filename,"w+");
	if (!myFile) {
		snprintf(str,1024,"ERROR: opening \"%s\" for write",filename);
		perror(str);
		exit(1);
	}
	
	// add latency objective function
	fprintf (myFile,"minimize\n\n");
	
	int earliest_completion = layers[num_layers-1]->asap_cycle;
	int latest_completion = layers[num_layers-1]->alap_cycle;
	for (int i = earliest_completion;i<=latest_completion;i++) {
		if (i!=earliest_completion) fprintf(myFile," + ");
		fprintf (myFile,"%d n_%d_c_%d",i,layers[num_layers-1]->id,i);
	}
	fprintf(myFile,"\n");
	
	fprintf (myFile,"\nsubject to\n\n");
	
	// define start time and dependency constraints
	myargs->file = myFile;
	
	traverse_dag (layers,
				  num_layers,
				  num_inputs,
				  num_outputs,
				  (void *)myargs,
				  emit_start_and_dependency_constraints,
				  FROM_START);
	
	
	fprintf (myFile,"\\ resource constraints\n");
	
	// define resource constraint for each cycle
	for (myargs->cycle = 0;myargs->cycle <= last_cycle;myargs->cycle++) {
		
		// clear flags
		traverse_dag (layers,
				  num_layers,
				  num_inputs,
				  num_outputs,
				  (void *)myargs,
				  clear_flags,
				  FROM_START);
		
		// set constraints for multipliers
		myargs->type=MULT;
		myargs->first=1;
		traverse_dag (layers,
					  num_layers,
					  num_inputs,
					  num_outputs,
					  (void *)myargs,
					  emit_resource_constraints,
					  FROM_START);
					  
		if (!myargs->first) {
			fprintf(myFile," <= %d\n",NUM_MULTIPLIERS);
		}
		
		// clear flags
		traverse_dag (layers,
				  num_layers,
				  num_inputs,
				  num_outputs,
				  (void *)myargs,
				  clear_flags,
				  FROM_START);
		
		// set constraints for adders
		myargs->type=ADD;
		myargs->first=1;
		traverse_dag (layers,
					  num_layers,
					  num_inputs,
					  num_outputs,
					  (void *)myargs,
					  emit_resource_constraints,
					  FROM_START);
		
		if (!myargs->first) {
			fprintf(myFile," <= %d\n",NUM_ADDERS);
		}
	}
	
	// add declarations
	fprintf (myFile,"\n\\ declarations\n");
	fprintf (myFile,"\ninteger\n\n");
	// clear flags
	traverse_dag (layers,
			  num_layers,
			  num_inputs,
			  num_outputs,
			  (void *)myargs,
			  clear_flags,
			  FROM_START);
			  
	traverse_dag (layers,
				  num_layers,
				  num_inputs,
				  num_outputs,
				  (void *)myargs,
				  generate_declarations,
				  FROM_START);
	
	fprintf (myFile,"\nend\n");
	
	fclose(myFile);
}

void apply_schedule (node *mynode,void *args) {
	argstype *myargs = (argstype *)args;
	
	if (mynode->id == myargs->id) mynode->scheduled_cycle = myargs->cycle;
}

void solve_schedule (node **layers,
						int num_layers,
						int num_inputs,
						int num_outputs,
						char *filename) {

	FILE *myFile;
	char str[1024],shell_command[1024],output_filename[1024],filename_prefix[1024];
	int ret,id,cycle;
	
	// make sure we can open the LP file
	myFile = fopen(filename,"r+");
	if (!myFile) {
		snprintf(str,1024,"Error opening \"%s\" for reading",filename);
		perror(str);
		exit(1);
	}
	
	fclose(myFile);
	
	// generate output filename
	sscanf(filename,"%[^.]",filename_prefix);
	snprintf(output_filename,1024,"%s.out",filename_prefix);
	
	// run the solver
	snprintf(shell_command,1024,"glpsol --binarize --tmlim 600 --lp %s -o %s",filename,output_filename);
	ret = system(shell_command);
	if (ret==-1) {
		snprintf(str,1024,"Error running \"%s\"",shell_command);
		perror(str);
		exit(1);
	}
	
	// read the output (assuming for now that it is solvable
	// TODO: check for "unsolvable" output
	snprintf(shell_command,1024,"awk '$2 ~ /n_[0-9]+_c_[0-9]+/ {if ($4==1) print $2}' %s",output_filename);
	myFile = popen(shell_command,"r");
	if (!myFile) {
		snprintf(str,1024,"Error running \"%s\"",shell_command);
		perror(str);
		exit(1);
	}
	
	// apply the solution
	while (!feof(myFile)) {
		argstype myargs;
		
		fscanf(myFile,"%s",str);
		//printf("read: \"%s\"\n",str);
		sscanf(str,"n_%d_c_%d",&myargs.id,&myargs.cycle);
		
		traverse_dag (layers,
			  num_layers,
			  num_inputs,
			  num_outputs,
			  (void *)&myargs,
			  apply_schedule,
			  FROM_START);
	}
	
	fclose(myFile);
}

void incr_utilization (node *mynode,void *args) {
	argstype *myargs = (argstype *)args;
	
	if (!mynode->flag) {
		if (mynode->type == ADD)
			myargs->add_scheduled_utilization[mynode->scheduled_cycle]++;
		else if (mynode->type == MULT)
			myargs->mult_scheduled_utilization[mynode->scheduled_cycle]++;
		
		mynode->flag=1;
	}
}

void tabulate_functional_unit_utilization (node *layers[],int num_layers,int num_inputs,int num_outputs) {
	// find scheduled latency
	int max_cycle = layers[num_layers-1]->scheduled_cycle+1;
	
	// check if the scheduled succeeded
	if (max_cycle==0) return;
	
	// allocate and initialize tables
	int *adder_utilization = (int *)malloc(sizeof(int)*max_cycle);
	int *multiplier_utilization = (int *)malloc(sizeof(int)*max_cycle);
	
	for (int i=0;i<max_cycle;i++) {
		adder_utilization[i]=0;
		multiplier_utilization[i]=0;
	}
	
	argstype myargs = {.add_scheduled_utilization = adder_utilization,
					   .mult_scheduled_utilization = multiplier_utilization};
					   
	// clear flags
	traverse_dag (layers,
			  num_layers,
			  num_inputs,
			  num_outputs,
			  (void *)&myargs,
			  clear_flags,
			  FROM_START);
			  
	// count
	traverse_dag (layers,
			  num_layers,
			  num_inputs,
			  num_outputs,
			  (void *)&myargs,
			  incr_utilization,
			  FROM_START);
			  
	printf ("Functional unit utilization\n"
	        "---------------------------\n");
	
	printf ("%10s%12s%12s\n","cycle","multiplier","adder");
	
	for (int i=0;i<max_cycle;i++)
		printf ("%10d%12d%12d\n",i,multiplier_utilization[i],adder_utilization[i]);
	
	free(adder_utilization);
	free(multiplier_utilization);
}
