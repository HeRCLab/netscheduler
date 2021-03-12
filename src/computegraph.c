#include "computegraph.h"
#include <stdarg.h>

DGRAPH_IMPL(cg, struct cg_node_t, struct cg_edge_t)

cg* cg_init_mlp(int inputs, int outputs, int hidden, ...) {
	va_list args;
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
	va_start(args, hidden);
	for (int i = 0 ; i < hidden ; i ++) {
		int layersize = va_arg(args, int);
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
	va_end(args);

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
			fprintf(stream, "\t\tn%d [label=%s]\n",
				id,
				cg_node_concrete_type_to_string(dgraph_node_data(cg, g->graph, id).node.concrete.type)
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
		dgraph_create_node(cg, g->graph, &sink, n);
		dgraph_create_edge(cg, g->graph, &eid, e, id, sink);

	} else if ((a.type == CG_NODE_ABSTRACT_HIDDEN) | (a.type == CG_NODE_ABSTRACT_OUTPUT)) {
		/* We start by generating the concrete output, which is
		 * the adder that sums up all the weight multiply results;
		 * we will then go back and generate the mutlipliers and weight
		 * VALs and link them into it. */
		n.type = CG_NODE_CONCRETE;
		n.node.concrete.type = CG_NODE_CONCRETE_ADD;
		dgraph_create_node(cg, g->graph, &coutput, n);
		dgraph_create_edge(cg, g->graph, &eid, e, id, coutput);

		/* XXX: right here is where activation function support should
		 * be added when we get to that point */

		if (a.type == CG_NODE_ABSTRACT_OUTPUT) {
			dgraph_id temp;
			/* This is an output node, so we also need to create
			 * the concrete output */
			n.type = CG_NODE_CONCRETE;
			n.node.concrete.type = CG_NODE_CONCRETE_OUTPUT;
			dgraph_create_node(cg, g->graph, &temp, n);
			dgraph_create_edge(cg, g->graph, &eid, e, id, temp);
			dgraph_create_edge(cg, g->graph, &eid, e, coutput, temp);
		}

		/* For each of the ancestors of a, we need to find their
		 * results, which will be the concrete nodes implementing their
		 * adder. */
		dgraph_foreach_ancestor(cg, g->graph, id, source,
			if (dgraph_node_data(cg, g->graph, source).type != CG_NODE_ABSTRACT) { continue; }
			/* Now source refers to an abstract ancestor, so we
			 * need to find the node which is a successor to
			 * source, and which is concrete, and which has a
			 * concrete type of ADD or INPUT. */
			dgraph_foreach_successor(cg, g->graph, source, csource,
				cg_node succ = dgraph_node_data(cg, g->graph, csource);
				if (succ.type != CG_NODE_CONCRETE) { continue; }
				if ((succ.node.concrete.type != CG_NODE_CONCRETE_ADD) &&\
					(succ.node.concrete.type != CG_NODE_CONCRETE_INPUT)) { continue; }

				/* csource no refers to the concrete result of
				 * source. We need to generate a VAL node to
				 * store the weight, a MULT node to multiply
				 * the weight by the output from the concrete
				 * output.
				 */

				/* VAL node */
				n.type = CG_NODE_CONCRETE;
				n.node.concrete.type = CG_NODE_CONCRETE_VAL;
				dgraph_create_node(cg, g->graph, &val, n);

				/* abstract -> concrete VAL */
				dgraph_create_edge(cg, g->graph, &eid, e, id, val);

				/* MULT node */
				n.type = CG_NODE_CONCRETE;
				n.node.concrete.type = CG_NODE_CONCRETE_MULT;
				dgraph_create_node(cg, g->graph, &mult, n);

				/* abstract -> concrete MULT */
				dgraph_create_edge(cg, g->graph, &eid, e, id, mult);

				/* VAL -> MULT */
				dgraph_create_edge(cg, g->graph, &eid, e, val, mult);

				/* ADD (result of ancestor) -> MULT */
				dgraph_create_edge(cg, g->graph, &eid, e, csource, mult);

				/* MULT -> ADD (result of abstract) */
				dgraph_create_edge(cg, g->graph, &eid, e, mult, coutput);

			);
		);
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

