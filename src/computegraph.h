#ifndef COMPUTEGRAPH_H
#define COMPUTEGRAPH_H

#include "dgraph.h"

DGRAPH_HEADER_INIT(cg, struct cg_node_t, struct cg_edge_t)

/* This structure represents a complete compute graph. It includes the actual
 * graph as well as metadata pertaining to the graph.
 *
 * The compute graph contains both an abstract and concrete representation of
 * the computation flow. The abstract representation would be something like a
 * neural network of nodes. The concrete representation deals in specific
 * hardware objects like adders and multipliers. Nodes in the abstract
 * representation have out-edges pointing to all nodes in the concrete
 * representation which implement them.
 * */
typedef struct cg_t {
	dgraph_t(cg)* graph;
} cg;

typedef enum {
	CG_NODE_CONCRETE = 1000,
	CG_NODE_ABSTRACT = 1001,
} cg_node_type;

#define cg_node_type_to_string(_nt) \
	(_nt == CG_NODE_CONCRETE) ? "CONCRETE" : \
	(_nt == CG_NODE_ABSTRACT) ? "ABSTRACT" : \
	"UNKNOWN"


typedef enum cg_node_abstract_type_t {
	CG_NODE_ABSTRACT_INPUT = 2000,
	CG_NODE_ABSTRACT_HIDDEN = 2001,
	CG_NODE_ABSTRACT_OUTPUT = 2002,
} cg_node_abstract_type;

#define cg_node_abstract_type_to_string(_nt) \
	(_nt == CG_NODE_ABSTRACT_INPUT) ? "INPUT" : \
	(_nt == CG_NODE_ABSTRACT_HIDDEN) ? "HIDDEN" : \
	(_nt == CG_NODE_ABSTRACT_OUTPUT) ? "OUTPUT" : \
	"UNKNOWN"

typedef struct cg_node_abstract_t {
	cg_node_abstract_type type;
	int layer;  /* neural network layer */
	int index;  /* linear index across all abstract NN nodes */
	int offset; /* linear index within NN layer */
} cg_node_abstract;

typedef enum cg_node_concrete_type_t {
	CG_NODE_CONCRETE_INPUT = 3000,
	CG_NODE_CONCRETE_OUTPUT = 3001,
	CG_NODE_CONCRETE_ADD = 3002,
	CG_NODE_CONCRETE_MULT = 3003,
} cg_node_concrete_type;

#define cg_node_concrete_type_to_string(_nt) \
	(_nt == CG_NODE_CONCRETE_INPUT) ? "INPUT" : \
	(_nt == CG_NODE_CONCRETE_OUTPUT) ? "OUTPUT" : \
	(_nt == CG_NODE_CONCRETE_ADD) ? "ADD" : \
	(_nt == CG_NODE_CONCRETE_MULT) ? "MULT" : \
	"UNKNOWN"

typedef struct cg_node_concrete_t {
	cg_node_concrete_type type;
} cg_node_concrete;

typedef union cg_node_union_t {
	struct cg_node_abstract_t abstract;
	struct cg_node_concrete_t concrete;
} cg_node_union;

/* The cg_node type represents a node which can be either abstract or concrete,
 * using a tagged union. */
typedef struct cg_node_t {
	cg_node_type type;
	cg_node_union node;  /* the actual node data */
} cg_node;

typedef struct cg_edge_t {
	int dummy; /* not used */
} cg_edge;


#endif /* COMPUTEGRAPH_H */
