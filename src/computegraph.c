#include "computegraph.h"

DGRAPH_IMPL(cg, struct cg_node_t, struct cg_edge_t)

cg* cg_init_mlp(int inputs, int outputs, int hidden, int* sizes) {
	int index;
	int offset;
	int layer;
	dgraph_id id;
	dgraph_id sourceid;
	dgraph_id edgeid;
	cg_node n;
	cg_edge e;
	e.dummy = 0;

	cg* g = malloc(sizeof(cg));
	g->graph = dgraph_init(cg);

	/* Create the input layer. */
	layer = 0;
	index = 0;
	for (offset = 0 ; offset < inputs; offset++) {
		n.type = CG_NODE_ABSTRACT;
		n.node.abstract.type = CG_NODE_ABSTRACT_INPUT;
		n.node.abstract.layer = layer;
		n.node.abstract.index = index;
		n.node.abstract.offset = offset;
		dgraph_create_node(cg, g->graph, &id, n);
		index ++;
	}

	/* Create the hidden layers. */
	for (int i = 0 ; i < hidden ; i ++) {
		int layersize = sizes[i];
		layer++;
		for (offset = 0 ; offset < layersize; offset++) {
			n.type = CG_NODE_ABSTRACT;
			n.node.abstract.type = CG_NODE_ABSTRACT_HIDDEN;
			n.node.abstract.layer = layer;
			n.node.abstract.index = index;
			n.node.abstract.offset = offset;
			dgraph_create_node(cg, g->graph, &id, n);

			/* create links into previous layer */
			dgraph_foreach_node(cg, g->graph, sourceid,
				/* we can safely assume an abstract node */
				if (dgraph_node_data(cg, g->graph, sourceid).node.abstract.layer == layer-1) {
					dgraph_create_edge(cg, g->graph, &edgeid, e, sourceid, id);
				}
			);

			index ++;
		}
	}

	/* And the output layer */
	layer++;
	for (offset = 0 ; offset < outputs; offset++) {
		n.type = CG_NODE_ABSTRACT;
		n.node.abstract.type = CG_NODE_ABSTRACT_OUTPUT;
		n.node.abstract.layer = layer;
		n.node.abstract.index = index;
		n.node.abstract.offset = offset;
		dgraph_create_node(cg, g->graph, &id, n);
		index ++;
		/* create links into previous layer */
		dgraph_foreach_node(cg, g->graph, sourceid,
			/* we can safely assume an abstract node */
			if (dgraph_node_data(cg, g->graph, sourceid).node.abstract.layer == layer-1) {
				dgraph_create_edge(cg, g->graph, &edgeid, e, sourceid, id);
			}
		);
	}

	return g;
}

void cg_destroy(cg* g) {
	dgraph_destroy(cg, g->graph);
	free(g);
}

void cg_generate_dot(cg* g, FILE* stream) {
	dgraph_id id;

	fprintf(stream, "digraph G {\n");

	fprintf(stream, "\tsubgraph cluster_abstract {\n");
	fprintf(stream, "\t\t label = \"abstract\"\n");
	dgraph_foreach_node(cg, g->graph, id,
		if (dgraph_node_data(cg, g->graph, id).type == CG_NODE_ABSTRACT) {
			fprintf(stream, "\t\tn%d [label=\"layer=%d, offset=%d, type=%s\"]\n",
				id,
				dgraph_node_data(cg, g->graph, id).node.abstract.layer,
				dgraph_node_data(cg, g->graph, id).node.abstract.offset,
				cg_node_abstract_type_to_string(dgraph_node_data(cg, g->graph, id).node.abstract.type)
			);
		}
	);
	fprintf(stream, "\t}\n");

	fprintf(stream, "\tsubgraph cluster_concrete {\n");
	fprintf(stream, "\t\tlabel = \"concrete\"\n");
	dgraph_foreach_node(cg, g->graph, id,
		if (dgraph_node_data(cg, g->graph, id).type == CG_NODE_CONCRETE) {
			fprintf(stream, "\t\tn%d [label=%s color=%s]\n",
				id,
				cg_node_concrete_type_to_string(dgraph_node_data(cg, g->graph, id).node.concrete.type),
				dgraph_node_data(cg, g->graph, id).node.concrete.result ? "red" : "black"
			);
		}
	);
	fprintf(stream, "\t}\n");

	dgraph_foreach_edge(cg, g->graph, id,
		dgraph_id source = dgraph_edge_source(cg, g->graph, id);
		dgraph_id sink = dgraph_edge_sink(cg, g->graph, id);
		if (dgraph_node_data(cg, g->graph, source).type == dgraph_node_data(cg, g->graph, sink).type) {
			fprintf(stream, "\tn%d -> n%d\n",
				dgraph_edge_source(cg, g->graph, id),
				dgraph_edge_sink(cg, g->graph, id)
			);
		} else  {
			fprintf(stream, "\tn%d -> n%d [color = grey]\n",
				dgraph_edge_source(cg, g->graph, id),
				dgraph_edge_sink(cg, g->graph, id)
			);
		}
	);

	fprintf(stream, "}\n");
}

/**
 * @brief make a specific node id concrete
 *
 * @param g
 * @param id must reference a GC_NODE_ABSTRACT
 */
static void cg_make_node_concrete(cg* g, dgraph_id id) {
	cg_node_abstract a = dgraph_node_data(cg, g->graph, id).node.abstract;
	cg_node n;
	dgraph_id sink;
	dgraph_id source;
	dgraph_id csource;
	dgraph_id coutput;
	dgraph_id val;
	dgraph_id mult;
	dgraph_id eid;
	cg_edge e;

	if (a.type == CG_NODE_ABSTRACT_INPUT) {
		n.type = CG_NODE_CONCRETE;
		n.node.concrete.type = CG_NODE_CONCRETE_INPUT;
		n.node.concrete.result = true;
		dgraph_create_node(cg, g->graph, &sink, n);
		dgraph_create_edge(cg, g->graph, &eid, e, id, sink);

	} else if ((a.type == CG_NODE_ABSTRACT_HIDDEN) | (a.type == CG_NODE_ABSTRACT_OUTPUT)) {

		/* We start by generating the concrete output, which is
		 * the adder that sums up all the weight multiply results;
		 * we will then go back and generate the mutlipliers and weight
		 * VALs and link them into it. */
		/* n.type = CG_NODE_CONCRETE; */
		/* n.node.concrete.type = CG_NODE_CONCRETE_ADD; */
		/* n.node.concrete.result = (a.type != CG_NODE_ABSTRACT_OUTPUT); */
		/* dgraph_create_node(cg, g->graph, &coutput, n); */
		/* dgraph_create_edge(cg, g->graph, &eid, e, id, coutput); */


		/* We will use this as a queue so as to generate nicely
		 * balanced adder trees. */
		vec_dgraph_id adderqueue;
		vec_init(&adderqueue);

		/* For each of the ancestors of a, we need to find their
		 * results, which will be the concrete nodes implementing their
		 * adder. */
		dgraph_foreach_ancestor(cg, g->graph, id, source,
			if (dgraph_node_data(cg, g->graph, source).type != CG_NODE_ABSTRACT) { continue; }
			/* Now source refers to an abstract ancestor, so we
			 * need to find the node which is a successor to
			 * source, and which is concrete, and which has it's
			 * result flag asserted. */
			dgraph_foreach_successor(cg, g->graph, source, csource,
				cg_node succ = dgraph_node_data(cg, g->graph, csource);
				if (succ.type != CG_NODE_CONCRETE) { continue; }
				if (!succ.node.concrete.result) {continue; }

				/* csource no refers to the concrete result of
				 * source. We need to generate a VAL node to
				 * store the weight, a MULT node to multiply
				 * the weight by the output from the concrete
				 * output.
				 */

				/* VAL node */
				n.type = CG_NODE_CONCRETE;
				n.node.concrete.type = CG_NODE_CONCRETE_ROVAL;
				n.node.concrete.result = false;
				dgraph_create_node(cg, g->graph, &val, n);

				/* abstract -> concrete VAL */
				dgraph_create_edge(cg, g->graph, &eid, e, id, val);

				/* MULT node */
				n.type = CG_NODE_CONCRETE;
				n.node.concrete.type = CG_NODE_CONCRETE_MULT;
				n.node.concrete.result = false;
				dgraph_create_node(cg, g->graph, &mult, n);

				/* abstract -> concrete MULT */
				dgraph_create_edge(cg, g->graph, &eid, e, id, mult);

				/* VAL -> MULT */
				dgraph_create_edge(cg, g->graph, &eid, e, val, mult);

				/* ADD (result of ancestor) -> MULT */
				dgraph_create_edge(cg, g->graph, &eid, e, csource, mult);

				/* MULT -> ADD (result of abstract) */
				vec_push(&adderqueue, mult);
				/* dgraph_create_edge(cg, g->graph, &eid, e, mult, coutput); */

			);

		);

		coutput = -1;
		while (adderqueue.length > 1) {
			dgraph_id srcid1, srcid2, newid;

			srcid1 = vec_pop(&adderqueue);
			srcid2 = vec_pop(&adderqueue);

			/* Create an adder node to add both of the two nodes
			 * we just popped out of the queue. */
			n.type = CG_NODE_CONCRETE;
			n.node.concrete.type = CG_NODE_CONCRETE_ADD;
			n.node.concrete.result = true;
			dgraph_create_node(cg, g->graph, &newid, n);

			/* Link the source nodes to the new node */
			dgraph_create_edge(cg, g->graph, &eid, e, srcid1, newid);
			dgraph_create_edge(cg, g->graph, &eid, e, srcid2, newid);

			/* Link the adder to it's abstract node */
			dgraph_create_edge(cg, g->graph, &eid, e, id, newid);

			/* Push the new node back into the queue, since we may
			 * need to create additional layers in the tree. */
			vec_insert(&adderqueue, 0, newid);

			if (coutput != -1) {
				dgraph_node_data(cg, g->graph, coutput).node.concrete.result = false;
			}

			coutput = newid;
		}

		/* Edge case where there is only one node */
		if (adderqueue.length > 0) {
			coutput = adderqueue.data[0];
			dgraph_node_data(cg, g->graph, coutput).node.concrete.result = true;
		}

		if (a.type == CG_NODE_ABSTRACT_OUTPUT) {
			dgraph_id temp;
			/* This is an output node, so we also need to create
			 * the concrete output */
			n.type = CG_NODE_CONCRETE;
			n.node.concrete.type = CG_NODE_CONCRETE_OUTPUT;
			n.node.concrete.result = true;
			dgraph_create_node(cg, g->graph, &temp, n);
			dgraph_create_edge(cg, g->graph, &eid, e, id, temp);
			dgraph_create_edge(cg, g->graph, &eid, e, coutput, temp);
		}

		vec_deinit(&adderqueue);


	}
}

void cg_make_concrete(cg* g) {
	dgraph_id id;

	vec_dgraph_id traverse;
	vec_init(&traverse);

	/* Find the list of all abstract nodes - we need to have these as a
	 * separate list, since the graph structure changes during our loop,
	 * so the terminating condition on dgraph_foreach_node might get out
	 * of date and we could miss nodes. */
	dgraph_foreach_node(cg, g->graph, id,
		if (dgraph_node_data(cg, g->graph, id).type == CG_NODE_ABSTRACT) {
			vec_push(&traverse, id);
		}
	);

	int i;
	vec_foreach(&traverse, id, i) {
		cg_make_node_concrete(g, id);
	}

	vec_deinit(&traverse);
}

static void cg_generate_forward_pass_node_result(cg* g, FILE* stream, dgraph_id id) {
	if (dgraph_node_data(cg, g->graph, id).type == CG_NODE_CONCRETE) {
		cg_node_concrete n = dgraph_node_data(cg, g->graph, id).node.concrete;
		if (n.type == CG_NODE_CONCRETE_INPUT) {
			fprintf(stream, "input[%d]", n.idx_input);

		} else if (n.type == CG_NODE_CONCRETE_OUTPUT) {
			fprintf(stream, "output[%d]", n.idx_output);

		} else if (n.type == CG_NODE_CONCRETE_ADD) {
			fprintf(stream, "scratch[%d]", n.idx_scratch);

		} else if (n.type == CG_NODE_CONCRETE_MULT) {
			fprintf(stream, "scratch[%d]", n.idx_scratch);

		} else if (n.type == CG_NODE_CONCRETE_ROVAL) {
			fprintf(stream, "rodata[%d]", n.idx_rdval);

		} else if (n.type == CG_NODE_CONCRETE_RWVAL) {
			fprintf(stream, "rwdata[%d]", n.idx_rwval);
		}
	}
}

static void cg_generate_forward_pass_node(cg* g, FILE* stream, dgraph_id id) {
	dgraph_id ancestor_id;

	if (dgraph_node_data(cg, g->graph, id).type != CG_NODE_CONCRETE) { return; }

	cg_node_concrete n = dgraph_node_data(cg, g->graph, id).node.concrete;


	if ((n.type == CG_NODE_CONCRETE_ADD) || (n.type == CG_NODE_CONCRETE_MULT)) {

		char* oper = "";
		if (n.type == CG_NODE_CONCRETE_ADD) { oper = " + "; }
		if (n.type == CG_NODE_CONCRETE_MULT) { oper = " * "; }

		fprintf(stream, "\t");
		cg_generate_forward_pass_node_result(g, stream, id);
		fprintf(stream, " = ");
		int n_ancestors = 0;
		dgraph_foreach_ancestor(cg, g->graph, id, ancestor_id,
			if (dgraph_node_data(cg, g->graph, ancestor_id).type == CG_NODE_CONCRETE) {
				n_ancestors ++;
			}
		);

		int i = 0;
		dgraph_foreach_ancestor(cg, g->graph, id, ancestor_id,
			if (dgraph_node_data(cg, g->graph, ancestor_id).type == CG_NODE_CONCRETE) {
				cg_generate_forward_pass_node_result(g, stream, ancestor_id);
				if (i < n_ancestors - 1) {
					fprintf(stream, " %s ", oper);
				} else {
					fprintf(stream, ";\n");
				}
				i++;
			}
		);
	}

	if (n.type == CG_NODE_CONCRETE_OUTPUT) {
		fprintf(stream, "\t");
		cg_generate_forward_pass_node_result(g, stream, id);
		fprintf(stream, " = ");
		dgraph_foreach_ancestor(cg, g->graph, id, ancestor_id,
			if (dgraph_node_data(cg, g->graph, ancestor_id).type == CG_NODE_CONCRETE) {
				cg_generate_forward_pass_node_result(g, stream, ancestor_id);
			}
		);
		fprintf(stream, ";\n");
	}
}


void cg_generate_forward_pass(cg* g, FILE* stream, const char* funcname, const char* datatype) {
	int num_inputs = 0;
	int num_outputs = 0;
	int num_rdval = 0;
	int num_rwval = 0;
	int num_scratch = 0;
	int num_nodes = 0;
	dgraph_id id;

	/* Count how many of each resource type we need to use, and also set up
	 * indexes into the arrays we will create for each. */
	dgraph_foreach_node(cg, g->graph, id,
		if (dgraph_node_data(cg, g->graph, id).type == CG_NODE_CONCRETE) {
			cg_node_concrete* n = &(dgraph_node_data(cg, g->graph, id).node.concrete);
			if (n->type == CG_NODE_CONCRETE_INPUT) {
				n->idx_input = num_inputs;
				num_inputs++;
			} else if (n->type == CG_NODE_CONCRETE_OUTPUT) {
				n->idx_output = num_outputs;
				num_outputs++;
			} else if (n->type == CG_NODE_CONCRETE_ROVAL) {
				n->idx_rdval = num_rdval;
				num_rdval++;
			} else if (n->type == CG_NODE_CONCRETE_RWVAL) {
				n->idx_rwval = num_rwval;
				num_rwval++;
			} else if (n->type == CG_NODE_CONCRETE_MULT) {
				n->idx_scratch = num_scratch;
				num_scratch++;
			} else if (n->type == CG_NODE_CONCRETE_ADD) {
				n->idx_scratch = num_scratch;
				num_scratch++;
			}
			num_nodes++;
		}
	);

	/* Generate the function signature */
	fprintf(stream, "void %s(", funcname);
	fprintf(stream, "%s input[%d], ", datatype, num_inputs);
	fprintf(stream, "%s output[%d]", datatype, num_outputs);
	if (num_rdval > 0) { fprintf(stream, ", %s rodata[%d]", datatype, num_rdval); }
	if (num_rwval > 0) { fprintf(stream, ", %s rwdata[%d]", datatype, num_rwval); }
	fprintf(stream, ") {\n");

	/* Generate the scratch array */
	fprintf(stream, "\t%s scratch[%d];\n", datatype, num_scratch);

	/* We will now perform a DFS traversal and generate the adders and
	 * multipliers. By using DFS order, we guarantee that all data
	 * dependencies for a node will be satisfied before its result is
	 * computed.  */
	vec_dgraph_id dfsqueue;
	vec_init(&dfsqueue);
	khash_t(dgraph_idset)* visited;
	visited = kh_init(dgraph_idset);

#define was_visited(_id) __extension__ ({ \
			khint_t _k; \
			_k = kh_get(dgraph_idset, visited, _id); \
			(_k != kh_end(visited)); \
		})
#define mark_visited(_id) do { \
		int r; \
		kh_put(dgraph_idset, visited, _id, &r); \
	} while(0);

	/* We will iterate through every output node, then work back from
	 * there. */
	dgraph_foreach_node(cg, g->graph, id,
		if (dgraph_node_data(cg, g->graph, id).type == CG_NODE_CONCRETE) {
			cg_node_concrete* n = &(dgraph_node_data(cg, g->graph, id).node.concrete);
			if (n->type == CG_NODE_CONCRETE_OUTPUT) {
				vec_push(&dfsqueue, id);
			}
		}
	);

	/* Now the queue is populated with all the output nodes, so we can go
	 * through each of their ancestors pushing nodes into the queue as we
	 * go. */
	dgraph_id cursor;
	int unvisited_adjacent = 0;
	for (;;) {
		if (dfsqueue.length <= 0) { break; }
		cursor = vec_pop(&dfsqueue);

		// fprintf(stream, "\t/* visiting %d */\n", cursor);

		unvisited_adjacent = 0;
		dgraph_foreach_ancestor(cg, g->graph, cursor, id,
			if (dgraph_node_data(cg, g->graph, id).type == CG_NODE_CONCRETE) {
				if (!was_visited(id)) {
					unvisited_adjacent++;
				} else {
					// fprintf(stream, "\t/* ancestor %d was visited already */\n", id);
				}
			}
		);
		// fprintf(stream, "\t/* %d unvisited adjacent nodes */\n", unvisited_adjacent);
		if (unvisited_adjacent == 0) {
			// fprintf(stream, "\t/* generating forward pass*/ \n");
			mark_visited(cursor);
			cg_generate_forward_pass_node(g, stream, cursor);
		} else if (!was_visited(cursor)) {
			// fprintf(stream, "\t/* push self %d */\n", cursor);
			vec_push(&dfsqueue, cursor);
		}
		dgraph_foreach_ancestor(cg, g->graph, cursor, id,
			if (dgraph_node_data(cg, g->graph, id).type == CG_NODE_CONCRETE) {
				if (!was_visited(id)) {
					// fprintf(stream, "\t/* push unvisited ancestor %d */\n", id);
					vec_push(&dfsqueue, id);
				}
			}
		);

	}

	fprintf(stream, "}\n");

	vec_deinit(&dfsqueue);
	kh_destroy(dgraph_idset, visited);

#undef was_visited
#undef mark_visited


}
