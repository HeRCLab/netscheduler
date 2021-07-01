#include "netscheduler.h"

int add_layer (node **layers,int layer_num,int id,int prev_layer_size,int new_layer_size,layer_type type,int inc_bias,int inc_delta_multiplier) {
	node *adder,*output=0,*prev_multiplier,*prev_adder,*multiplier,*newnode=0,*prev_layer_adder;

	if (type == INPUT_LAYER) {
		// create input layer
		for (int i=0;i<new_layer_size;i++) {
			if (newnode) {
				newnode->next = create_node(INPUT,id++);
				newnode->next->prev = newnode;
				newnode = newnode->next;
				newnode->neuron = i;
			} else {
				// first input
				newnode = layers[0] = create_node(INPUT,id++);
				newnode->neuron = i;
			}
			newnode->layer=layer_num;
		}
		// complete the cycle
		newnode->next = 0;
		layers[0]->prev = 0;

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
				multiplier->input_number = j; // not used?
				
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
				connect_nodes (predecessor,multiplier,j);
			}
			
			// at this point, we're done with multipliers, now add adders
			// first check if there was only one multiplier.  if so, there's no reason to
			// have any adders
			if (prev_layer_size>=1) {
				prev_layer_adder = multiplier; // start with last multiplier
				while (prev_layer_adder->prev || prev_layer_adder->next) {
					int k=0; // adder count of current layer (of the binary adder tree)
					while (prev_layer_adder) {
						if (!prev_layer_adder->prev) {
							// if there's only one remaining adder on the previous adder layer, then bail out
							break;
						}
						// instance new adder
						node *prev_adder = adder;
						adder = create_node (ADD,id++);
						adder->neuron = i;
						adder->layer = layer_num;
						if (k>0) {
							adder->prev = prev_adder;
							adder->prev->next = adder;
						}
						connect_nodes(prev_layer_adder->prev,adder,0);
						connect_nodes(prev_layer_adder,adder,0);
						
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
			}
			
			// need to check again for single multiplier layer and handle it specially.
			// specifically, pretend the multiplier was an adder since we're not
			// adding any actual adders to this neuron
			
			// this is a hack, since "adder" is used below as the final node of the MLP layer
			if (prev_layer_size==1) adder=multiplier;
			
			// we don't need the bias node for back-propagation
			if (inc_bias) {
				// add the bias adder
				node *bias_adder = create_node (ADDBIAS,id++);
				bias_adder->layer = layer_num;
				bias_adder->neuron = i;
				bias_adder->input_number = 0;
				connect_nodes(adder,bias_adder,0);
				
				// this is a hack, since "adder" is used below as the final node of the MLP layer
				adder = bias_adder;
			}
			
			// ...we do, however, need to multiply each delta by the previous output
 			if (inc_delta_multiplier) {
				// add the delta multiplier
				node *delta_multiplier = create_node (MULT,id++);
				delta_multiplier->layer = layer_num;
				delta_multiplier->neuron = i;
				delta_multiplier->input_number = 0;
				delta_multiplier->delta_multiplier = 1;
				connect_nodes(adder,delta_multiplier,0);
				
				// this is a hack, since "adder" is used below as the final node of the MLP layer
				adder = delta_multiplier;
			}
			
			// mark this as a "final" adder (er, node)
			adder->final_adder=1;
			
			// connect the final adder output to the layer 1 list
			if (i==0) {
				// first neuron
				layers[layer_num] = adder;
			} else {
				// not the first neuron
				node *mynode = layers[layer_num];
				while (mynode->next) mynode=mynode->next;
				mynode->next = adder;
				mynode->next->prev = mynode;
				
				// complete the ring
				if (i==new_layer_size-1) {
					/* mynode->next->next=layers[layer_num];
					layers[layer_num]->prev = mynode; */
					mynode->next->next=0;
					layers[layer_num]->prev = 0;
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
				connect_nodes (predecessor,multiplier,j);

				if (j>0) { // don't do anything after creating the first multiplier
					prev_adder = adder;
					adder = create_node (ADD,id++);
					adder->neuron = i;

					if (j==1) {
						// first adder is special...
						connect_nodes(prev_multiplier,adder,j);
						connect_nodes(multiplier,adder,j);
					} else if (j>1) {
						connect_nodes(prev_adder,adder,j);
						connect_nodes(multiplier,adder,j);
					}

				}
			}

			// mark this as a "final" adder
			adder->final_adder=1;

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
			output->layer = layer_num;
			output->neuron = i;
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
				connect_nodes(mynode,output,j);
				mynode = mynode->next;
			}
		}
	}
	
	return id;
}

// create DAG for a basic 3,4,1 MLP
node **create_basic_network_dag (int num_layers,int *layer_sizes,int inc_bias,int inc_delta_multiplier) {
	node *newnode=NULL,
		 **layers;

	int i,id=0,prev_layer_size=-1;

	// need to add an extra layer for "final" output
	// I don't remember why this is needed
	layers=(node **)malloc((num_layers+1)*sizeof(node*));
	
	for (i=0;i<num_layers;i++) {
		layers[i]=0;
		layer_type type = i==0 ? INPUT_LAYER :
						  i==num_layers ? OUTPUT_LAYER :
						  NEURON_BINARY_ADD_LAYER;
						  
		id=add_layer(layers,i,id,prev_layer_size,layer_sizes[i],type,inc_bias,inc_delta_multiplier);
		prev_layer_size=layer_sizes[i];
	}
	
	layers[i]=0;
	id=add_layer(layers,i,id,prev_layer_size,1,OUTPUT_LAYER,inc_bias,inc_delta_multiplier);

	return layers;
}

