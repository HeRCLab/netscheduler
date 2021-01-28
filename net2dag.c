#include "netscheduler.h"

int add_layer (node **layers,int layer_num,int id,int prev_layer_size,int new_layer_size,layer_type type) {
	node *adder,*output=0,*prev_multiplier,*prev_adder,*multiplier,*newnode=0,*prev_layer_adder;

	if (type == INPUT_LAYER) {
		// create input layer
		for (int i=0;i<new_layer_size;i++) {
			if (newnode) {
				newnode->next = create_node(INPUT,id++);
				newnode->next->prev = newnode;
				newnode = newnode->next;
			} else {
				// first input
				newnode = layers[0] = create_node(INPUT,id++);
			}
		}
		// complete the cycle
		newnode->next = layers[0];
		layers[0]->prev = newnode;

	} else if (type == NEURON_BINARY_ADD_LAYER) {
		
		// create multipliers of hidden layer
		for (int i=0;i<new_layer_size;i++) { // for each neuron on current layer...
			for (int j=0;j<prev_layer_size;j++) { // for each input from the previous layer, add a multiplier and adder
				// remember previous multiplier
				prev_multiplier = multiplier;

				// create new multiplier node and initialize
				multiplier = create_node (MULT,id++);
				multiplier->layer = layer_num;
				multiplier->neuron = i;
				multiplier->input_number = j;
				
				if (j>0) {
					// link to previous multiplier
					multiplier->prev = prev_multiplier;
					multiplier->prev->next = multiplier;
				}

				// link it back to the previous layer
				// find its corresponding input, which is number j
				node *predecessor = layers[layer_num-1];

				// walk to node j
				for (int k=0;k<j;k++) predecessor = predecessor->next;
				// connect multiplier back to output of previous layer
				connect_nodes (predecessor,multiplier);
			}
			
			// at this point, we're done with multipliers, now add adders
			prev_layer_adder = multiplier; // start with last multiplier
			while (prev_layer_adder->prev || prev_layer_adder->next) { // keep adding layers until we have one node left
				int k=0; // adder count of current layer (of the binary adder tree)
				while (prev_layer_adder) {
					if (!prev_layer_adder->prev) {
						// if there's only one remaining adder on the previous layer, bail out
						break;
					}
					// instance new adder
					node *prev_adder = adder;
					adder = create_node (ADD,id++);
					if (k>0) {
						adder->prev = prev_adder;
						adder->prev->next = adder;
					}
					connect_nodes(prev_layer_adder->prev,adder);
					connect_nodes(prev_layer_adder,adder);
					
					prev_layer_adder = prev_layer_adder->prev->prev; // jump two backward (should eventually become NULL on even-numbered layer size)
					k++;
				}
				if (prev_layer_adder) {
					// if previous layer was odd, start with odd man, but link it to current layer just finished
					// this is a bit hacky, since it promotes the odd man out to the next layer
					prev_layer_adder->prev = adder;
				} else {
					// otherwise, start with last adder created
					prev_layer_adder = adder;
				}
			}
			
			// connect the final adder output to the layer 1 list
			if (i==0) {
				layers[layer_num] = adder;
			} else {
				node *mynode = layers[layer_num];
				while (mynode->next) mynode=mynode->next;
				mynode->next = adder;
				mynode->next->prev = mynode;
				
				// complete the ring
				if (i==new_layer_size-1) {
					mynode->next->next=layers[layer_num];
					layers[layer_num]->prev = mynode;
				}
			}
			
		}
		
	} else if (type == NEURON_LAYER) {

		// create multipliers of hidden layer
		for (int i=0;i<new_layer_size;i++) { // for each neuron on current layer...
			for (int j=0;j<prev_layer_size;j++) { // for each input from the previous layer, add a multiplier and adder
				// remember previous multiplier
				prev_multiplier = multiplier;

				// create new multiplier node and initialize
				multiplier = create_node (MULT,id++);
				multiplier->layer = layer_num;
				multiplier->neuron = i;
				multiplier->input_number = j;

				// link it back to the previous layer
				// find its corresponding input, which is number j
				node *predecessor = layers[layer_num-1];

				// walk to node j
				for (int k=0;k<j;k++) predecessor = predecessor->next;
				// connect multiplier back to output of previous layer
				connect_nodes (predecessor,multiplier);

				if (j>0) { // don't do anything after creating the first multiplier
					prev_adder = adder;
					adder = create_node (ADD,id++);

					if (j==1) {
						// first adder is special...
						connect_nodes(prev_multiplier,adder);
						connect_nodes(multiplier,adder);
					} else if (j>1) {
						connect_nodes(prev_adder,adder);
						connect_nodes(multiplier,adder);
					}

				}
			}

			// connect the final adder output to the layer 1 list
			if (i==0) {
				layers[layer_num] = adder;
			} else {
				node *mynode = layers[layer_num];
				while (mynode->next) mynode=mynode->next;
				mynode->next = adder;
				mynode->next->prev = mynode;
				
				// complete the ring
				if (i==new_layer_size-1) {
					mynode->next->next=layers[layer_num];
					layers[layer_num]->prev = mynode;
				}
			}
		}
	
	} else if (type == OUTPUT_LAYER) {
		for (int i=0;i<new_layer_size;i++) { // for each neuron on current layer...
		
			// link to previously-spawned node, if exists, and spawn a new node
			node *prev_output = output;
			output = create_node (OUTPUT,id++);
			if (layers[layer_num]==0) {
				// if first node, point to it in layer table
				layers[layer_num] = output;
			} else {
				// link from previous to current
				prev_output->next = output;
				// link from current to previous
				output->prev = prev_output;
				// add wrap-around link, which will be overwritten if this isn't the last node spawned
				output->next = layers[layer_num];
			}
			// walk across previous layer 
			node *mynode = layers[layer_num-1];
			for (int j=0;j<prev_layer_size;j++) { // for each input from the previous layer, add an output
				connect_nodes(mynode,output);
				mynode = mynode->next;
			}
		}
	}
	
	return id;
}

// create DAG for a basic 3,4,1 MLP
node **create_basic_network_dag (int num_layers,int num_inputs,int hidden_size) {
	node *newnode=NULL,
		 **layers;

	int id=0;

	layers=(node **)malloc(NUM_LAYERS*sizeof(node*));
	for (int i=0;i<num_layers;i++) layers[i]=0;

	id=add_layer(layers,0,id,-1,num_inputs,INPUT_LAYER);
#ifdef BINARY_ADDER
	id=add_layer(layers,1,id,num_inputs,hidden_size,NEURON_BINARY_ADD_LAYER);
#else
	id=add_layer(layers,1,id,num_inputs,hidden_size,NEURON_LAYER);
#endif
	id=add_layer(layers,2,id,hidden_size,1,NEURON_BINARY_ADD_LAYER);
	id=add_layer(layers,3,id,1,1,OUTPUT_LAYER);

	return layers;
}
