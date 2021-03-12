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
	fprintf(stream, "\t\t label = \"concrete\"\n");
	dgraph_foreach_node(cg, g->graph, id,
		if (dgraph_node_data(cg, g->graph, id).type == CG_NODE_CONCRETE) {
			fprintf(stream, "\t\tn%d [type=%s]\n",
				id,
				cg_node_concrete_type_to_string(dgraph_node_data(cg, g->graph, id).node.concrete.type)
			);
		}
	);
	fprintf(stream, "\t}\n");

	dgraph_foreach_edge(cg, g->graph, id,
		fprintf(stream, "\tn%d -> n%d\n",
			dgraph_edge_source(cg, g->graph, id),
			dgraph_edge_sink(cg, g->graph, id)
		);
	);

	fprintf(stream, "}\n");
}
