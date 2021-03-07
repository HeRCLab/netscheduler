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

void emit_op_constraints (node *mynode,void *args) {
	argstype *myargs = (argstype *)args;

	if (myargs->cycle >= mynode->asap_cycle && myargs->cycle <= mynode->alap_cycle) {
		myargs->flag=1;

		if (mynode->type==MULT) fprintf (myargs->file,"- %d n_%d_c_%d ",
												myargs->cycle,
												mynode->id,
												myargs->cycle);

		if (mynode->type==ADD) fprintf (myargs->file,"+ %d n_%d_c_%d ",
												myargs->cycle,
												mynode->id,
												myargs->cycle);
	}
}

void emit_vector_constraints (node *mynode,void *args) {

	if (!mynode->flag) {
		FILE *myFile = ((argstype *)args)->file;

		fprintf (myFile,"\\ vector constraints\n");

		int min_asap = 1000;
		int max_alap = 0;
		int num_inputs = 0;

		// NOTE: this only works for 0 to 2 predecessor nodes
		edge *myedge = mynode->in_edges;
		while (myedge) {
			node *in_node = myedge->edge;

			if (in_node->asap_cycle < min_asap) {
				min_asap = in_node->asap_cycle;
			}
			if (in_node->alap_cycle > max_alap) {
				max_alap = in_node->alap_cycle;
			}
			num_inputs++;
			myedge = myedge->next;
		}

		if (num_inputs==2) {
			// don't allow the predecessors to complete at the same time, since this will put the
			// results into the same vector element

			for (int i=min_asap;i<=max_alap;i++) {
				int first_pred_id = mynode->in_edges->edge->id;
				int second_pred_id = mynode->in_edges->next->edge->id;

				if (i!=min_asap) fprintf(myFile,"+ ");
				fprintf(myFile,"%d n_%d_c_%d - %d n_%d_c_%d ",i,first_pred_id,i,i,second_pred_id,i);
			}
			fprintf(myFile,"< 0\n");
		}

		mynode->flag=1;
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
	
#ifdef VECTORIZE
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
					  emit_vector_constraints,
					  FROM_START);

		fprintf(myFile,"\\ operation constraints for vector unit\n");
		for (int i=0;i<=last_cycle;i++) {
			myargs->cycle = i;
			myargs->flag = 0;

			traverse_dag (layers,
					  num_layers,
					  num_inputs,
					  num_outputs,
					  (void *)myargs,
					  emit_op_constraints,
					  FROM_START);

			if (myargs->flag) fprintf(myFile," > 0\n");
			
		}
#endif

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
	snprintf(output_filename,1024,"%s.sol",filename_prefix);
	
	// run the solver
#ifdef USE_GUROBI
	snprintf(shell_command,1024,"GUROBI_PATH=\"%s\" LD_LIBRARY_PATH=\"%s/lib\" "
								"%s/gurobi_cl ResultFile=%s %s",
								GUROBI_PATH,GUROBI_PATH,GUROBI_PATH,
								output_filename,filename);
#else
	snprintf(shell_command,1024,"glpsol --binarize --tmlim 36000 --lp %s -o %s",filename,output_filename);
#endif
	ret = system(shell_command);
	if (ret==-1) {
		snprintf(str,1024,"Error running \"%s\"",shell_command);
		perror(str);
		exit(1);
	}
	
	// read the output (assuming for now that it is solvable
	// TODO: check for "unsolvable" output
#ifdef USE_GUROBI
	snprintf(shell_command,1024,"awk '$1 ~ /n_[0-9]+_c_[0-9]+/ {if ($2==1) print $1}' %s",output_filename);
#else
	snprintf(shell_command,1024,"awk '$2 ~ /n_[0-9]+_c_[0-9]+/ {if ($4==1) print $2}' %s",output_filename);
#endif
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
	int max_cycle = layers[num_layers-1]->scheduled_cycle;
	
	// check if the scheduled succeeded
	if (max_cycle<=0) return;
	
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

	int total_adds = 0;
	int total_mults = 0;
	for (int i=0;i<max_cycle;i++) {
		total_adds += adder_utilization[i];
		total_mults += multiplier_utilization[i];
	}
	
	int add_slots = max_cycle*NUM_ADDERS;
	printf ("total adds = %d, total slots %d, utilization = %0.0f%%\n",total_adds,add_slots,(float)total_adds/(float)add_slots*100.f);
	int mult_slots = max_cycle*NUM_MULTIPLIERS;
	printf ("total mults = %d, total slots %d, utilization = %0.0f%%\n",total_mults,mult_slots,(float)total_mults/(float)mult_slots*100.f);

	free(adder_utilization);
	free(multiplier_utilization);
}

void printinst (node *mynode,void *myargs) {
	func_cycle *myarg = (func_cycle *)myargs;
	
	if (!mynode->flag) {
		
		if ((mynode->scheduled_cycle==myarg->cycle) && (mynode->type==myarg->type)) {
			char str[1024];
			if (mynode->type==ADD) {
				snprintf(str,1024,"%s %d %d %d",NODETYPE(mynode->type),
												mynode->id,
												mynode->in_edges->edge->id,
												mynode->in_edges->next->edge->id);
												
			} else if (mynode->type==MULT) {
				snprintf(str,1024,"%s %d %d coeff",NODETYPE(mynode->type),
												mynode->id,
												mynode->in_edges->edge->id);
												
			} else if (mynode->type==INPUT) {
				snprintf(str,1024,"load %d",mynode->id);
			} else if (mynode->type==OUTPUT) {
				snprintf(str,1024,"store %d",mynode->in_edges->edge->id);
			}
			printf ("\"%s\",",str);
			myarg->found++;
		}
	
		mynode->flag=1;
	}
}

void tabulate_schedule_by_cycle (node *layers[],int num_layers,int num_inputs,int num_outputs) {
	int num_cycles = layers[num_layers-1]->scheduled_cycle;
	func_cycle myarg;
	
	// print table headers
	printf ("\"%s\",","cycle");
	for (int i=0;i<NUM_ADDERS;i++) {
		char str[1024];
		snprintf(str,1024,"\"adder%d\",",i);
		printf("%s",str);
	}
	for (int i=0;i<NUM_MULTIPLIERS;i++) {
		char str[1024];
		snprintf(str,1024,"\"mult%d\",",i);
		printf("%s",str);
	}
	printf ("\n");
	for (int i=0;i<num_cycles;i++) {
		printf ("\"%d\",",i);
	
		myarg.cycle=i;
		myarg.type=ADD;
		myarg.found=0;
	
		// clear flags
		traverse_dag (layers,
				  num_layers,
				  num_inputs,
				  num_outputs,
				  (void *)&myarg,
				  clear_flags,
				  FROM_START);
				  
		// count
		traverse_dag (layers,
				  num_layers,
				  num_inputs,
				  num_outputs,
				  (void *)&myarg,
				  printinst,
				  FROM_START);
		
		int blank_slots = NUM_ADDERS-myarg.found;
		for (int j=0;j<blank_slots;j++) {
			printf("\"%s\",","");
		}
				  
		myarg.type=MULT;
		myarg.found=0;
	
		// clear flags
		traverse_dag (layers,
				  num_layers,
				  num_inputs,
				  num_outputs,
				  (void *)&myarg,
				  clear_flags,
				  FROM_START);
				  
		// count
		traverse_dag (layers,
				  num_layers,
				  num_inputs,
				  num_outputs,
				  (void *)&myarg,
				  printinst,
				  FROM_START);
				  
		blank_slots = NUM_ADDERS-myarg.found;
		for (int j=0;j<blank_slots;j++) {
			printf("\"%s\",","");
		}

		printf ("\n");
	}
}