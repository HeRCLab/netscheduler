/* Copyright 2021 Jason Bakos, Philip Conrad, Charles Daniels */

/* This library implements a general-purpose directed graph data structure,
 * which supports attaching arbitrary data to nodes and links.  */

typedef int dgraph_id;

typedef enum {
	DGRAPH_ERROR_OK = 0,
	DGRAPH_ERROR_MALLOC_FAILED,
	DGRAPH_ERROR_EDGE_SOURCE_NONEXISTANT,
	DGRAPH_ERROR_EDGE_SINK_NONEXISTANT,
} dgraph_error;

/**** dgraph private API (keep scrollin'...) *********************************/

#ifndef DGRAPH_H
#define DGRAPH_H

#include <herc/khash.h>
#include <herc/vec.h>
#include <stdint.h>

typedef vec_t(dgraph_id) vec_dgraph_id;

/* Map edge or node IDs to lists of edge or node IDs */
KHASH_MAP_INIT_INT64(dgraph_id2idlist, vec_dgraph_id)

#define _DGRAPH_TYPE(name, node_data_t, edge_data_t) \
	typedef vec_t(node_data_t) vec_dgraph_node_data_##name; \
	typedef vec_t(edge_data_t) vec_dgraph_edge_data_##name; \
	typedef struct dgraph_##name##_t { \
		/* array storing data attached to nodes, indexed by ID */ \
		vec_dgraph_node_data_##name nodes; \
		/* array storing data attached to edges, indexed by ID */ \
		vec_dgraph_edge_data_##name edges; \
		/* array of node IDs which are sources and sinks of edges, indexed by edge ID */ \
		vec_dgraph_id edge_sources; \
		vec_dgraph_id edge_sinks; \
		/* edges indexed by their source ID */ \
		khash_t(dgraph_id2idlist)* edges_by_sink; \
		/* edges indexed by their sink ID */ \
		khash_t(dgraph_id2idlist)* edges_by_source; \
	} dgraph_##name;

#define _DGRAPH_PROTOTYPES(name, node_data_t, edge_data_t) \
	dgraph_error dgraph_create_node_##name(dgraph_##name* g, dgraph_id* nodeid, node_data_t data); \
	int dgraph_n_nodes_##name(dgraph_##name* g); \
	int dgraph_n_edges##name(dgraph_##name* g); \
	dgraph_error dgraph_create_edge_##name(dgraph_##name* g, dgraph_id* edgeid, edge_data_t data, dgraph_id source, dgraph_id sink); \
	dgraph_##name* dgraph_init_##name(); \
	void dgraph_destroy_##name(dgraph_##name* g);

#define _DGRAPH_IMPL(name, SCOPE, node_data_t, edge_data_t) \
	SCOPE dgraph_##name* dgraph_init_##name() { \
		dgraph_##name* g = malloc(sizeof(dgraph_##name)); \
		if (g == NULL) { return NULL; } \
		vec_init(&(g->nodes)); \
		vec_init(&(g->edges)); \
		vec_init(&(g->edge_sources)); \
		vec_init(&(g->edge_sinks)); \
		g->edges_by_source = kh_init(dgraph_id2idlist); \
		if (g->edges_by_source == NULL) { \
			free(g); \
			return NULL; \
		} \
		g->edges_by_sink = kh_init(dgraph_id2idlist); \
		if (g->edges_by_sink == NULL) { \
			free(g ->edges_by_source); \
			free(g); \
			return NULL; \
		} \
		return g; \
	} \
	void dgraph_destroy_##name(dgraph_##name* g) { \
		/* first destroy the edges_by_{source,sink} maps */ \
		vec_dgraph_id m; \
		kh_foreach_value(g->edges_by_source, m, vec_deinit(&m);); \
		kh_foreach_value(g->edges_by_sink, m, vec_deinit(&m);); \
		kh_destroy(dgraph_id2idlist, g->edges_by_source); \
		kh_destroy(dgraph_id2idlist, g->edges_by_sink); \
		vec_deinit(&(g->edge_sinks)); \
		vec_deinit(&(g->edge_sources)); \
		vec_deinit(&(g->edges)); \
		vec_deinit(&(g->nodes)); \
		free(g); \
	} \
	SCOPE dgraph_error dgraph_create_node_##name(dgraph_##name* g, dgraph_id* nodeid, node_data_t data) { \
		*nodeid = g->nodes.length; \
		if (vec_push(&(g->nodes), data) == 0) { \
			return DGRAPH_ERROR_OK; \
		} else { \
			return DGRAPH_ERROR_MALLOC_FAILED; \
		} \
	} \
	int dgraph_n_nodes_##name(dgraph_##name* g) { \
		return g->nodes.length; \
	}  \
	int dgraph_n_edges_##name(dgraph_##name* g) { \
		return g->edges.length; \
	}  \
	dgraph_error dgraph_create_edge_##name(dgraph_##name* g, dgraph_id* edgeid, edge_data_t data, dgraph_id source, dgraph_id sink) { \
		/* validate that the source and sink nodes exist */ \
		if ((source < 0) || (source > dgraph_n_nodes_##name(g))) { \
			return DGRAPH_ERROR_EDGE_SOURCE_NONEXISTANT; \
		} \
		if ((sink < 0) || (sink > dgraph_n_nodes_##name(g))) { \
			return DGRAPH_ERROR_EDGE_SINK_NONEXISTANT; \
		} \
		/* insert the edge into the edge data array */ \
		*edgeid = g->edges.length; \
		if (vec_push(&(g->edges), data) != 0) { \
			return DGRAPH_ERROR_MALLOC_FAILED; \
		} \
		/* record the source and sink data */ \
		vec_push(&(g->edge_sources), source); \
		vec_push(&(g->edge_sinks), sink); \
		/* insert into edges_by_source map */ \
		khint_t k; int r; \
		k = kh_put(dgraph_id2idlist, g->edges_by_source, source, &r); \
		if (r < 0) { \
			vec_pop(&(g->edges)); \
			vec_pop(&(g->edge_sources)); \
			vec_pop(&(g->edge_sinks)); \
			return DGRAPH_ERROR_MALLOC_FAILED; \
		} \
		vec_init(&(kh_value(g->edges_by_source, k))); \
		vec_push(&(kh_value(g->edges_by_source, k)), *edgeid); \
		/* insert into edges_by_sink map */ \
		k = kh_put(dgraph_id2idlist, g->edges_by_sink, sink, &r); \
		if (r < 0) { \
			vec_pop(&(g->edges)); \
			vec_pop(&(g->edge_sources)); \
			vec_pop(&(g->edge_sinks)); \
			k = kh_get(dgraph_id2idlist, g->edges_by_source, source); \
			if (k != kh_end(g->edges_by_source)) { \
				vec_deinit(&(kh_value(g->edges_by_source, k))); \
				kh_del(dgraph_id2idlist, g->edges_by_source, k); \
			} \
			return DGRAPH_ERROR_MALLOC_FAILED; \
		} \
		vec_init(&(kh_value(g->edges_by_sink, k))); \
		vec_push(&(kh_value(g->edges_by_sink, k)), *edgeid); \
		return DGRAPH_ERROR_OK; \
	} \


/**** dgraph public API ******************************************************/

/**
 * OVERVIEW
 *
 * dgraph presents the programmer with a type-safe API for interacting with
 * directed graphs with arbitrary data attached to their nodes and edges.
 * This works by using preprocessor macros to generate a full implementation
 * of all methods needed for each graph type. A graph type is characterized by
 * a unique name, a type of data attached to nodes, and a type of data attached
 * to edges.
 *
 * dgraph offers various utility methods for inter operating with directed
 * graphs, which are documented below. dgraph identifies both nodes and edges
 * by integer IDs (NOTE: IDs are unique within edges and nodes, but not across
 * them - id N may refer to both an edge and a node). It also offers a high
 * level of performance and memory efficiency. In summary:
 *
 * * Accessing a node or edge by it's integer ID - O(1)
 *
 * * Accessing the source or sink nodes by the integer ID of an edge - O(1)
 *
 * * Accessing the edges which have a given integer node ID as a source or
 *   a sink - O(|V|), assuming |V| is the number of nodes.
 *
 *	* In practice, this will be much smaller than O(|V|) for most graphs,
 *	  it is directly dependent on how many edges have the given node ID as a
 *	  source or sink.
 *
 * * Traverse every node once - O(|V|) assuming there are |V| many nodes.
 *
 * * Traverse every edge once - O(|E|) assuming there are |E| many edges.
 *
 * dgraph uses dynamic arrays to store node adjacency and attached data to
 * nodes and edges, which means traversing all edges or nodes involves a highly
 * sequential memory access pattern, though using a pointer type as the node or
 * edge data may impact performance for obvious reasons.  Operations where the
 * nodes and edges are identified by their unique IDs in advance are extremely
 * performant, and translate to a simple array lookup.
 *
 * Hash tables of dynamic arrays are used to store the set of edges adjacent
 * to a given node. khash is used, which is highly performant, however this
 * may imply some overhead.
 *
 * dgraph is capable of managing the complete lifecycle of a directed graph
 * from allocation to deallocation, however the programmer must manage the
 * lifecycle of any attached data.
 */

/**
 * USAGE
 *
 * Full documentation of all pertinent macros and methods are provided below.
 * A minimal usage of dgraph requires you at least call DGRAPH_HEADER_INIT(),
 * and DGRAPH_IMPL(). Usually the former is done in a header file, and the
 * latter in an otherwise empty C source file. As many graphs of the
 * same type can then be instantiated using dgraph_init().
 *
 * As many different dgraph types can be used as desired, and as many different
 * instances of a given type can be used at once as desired.
 *
 * For example:
 *
 * --- myheader.h -------------------------------------------------------------
 *
 * #include "dgraph.h"
 *
 * #ifndef MYHEADER_H
 * #define MYHEADER_H
 *
 * // Define a graph type named mygraphtype which has integers attached to both
 * // edges and nodes.
 * DGRAPH_HEADER_INIT(mygraphtype, int, int)
 *
 * #endif
 *
 * --- mygraphtype_impl.c -----------------------------------------------------
 *
 * #include "myheader.h"
 *
 * // Generate the implementation to go along with what we set up using
 * // DGRAPH_HEADER_INIT(). Arguments passed here MUST match what was given to
 * // DGRAPH_HEADER_INIT() or the code will not compile.
 * DGRAPH_IMPL(mygraphtype, int, int)
 *
 * --- mycode.c ---------------------------------------------------------------
 *
 * #include "dgraph.h"
 * #include "myheader.h"
 *
 * int main(void) {
 *	dgraph_id node_id;
 *
 *	dgraph_t(mygraphtype)* g = dgraph_init(mygraphtype);
 *
 *	dgraph_create_node(mygraphtype, g, &node_id, 123);
 *
 *      // ...
 *
 *      dgraph_destroy(mygraphtype, g);
 *
 * }
 *
 * ----------------------------------------------------------------------------
 *
 * NOTE: it is not recommended to use the methods and macros generated directly
 * by DGRAPH_HEADER_INIT() and DGRAPH_IMPL(). They are not considered part of
 * the public API for dgraph and may change without notice. Instead, use only
 * the methods documented below that are part of the public API.
 */

/**
 * @brief generate header code needed for a typed dgraph implementation
 *
 * This macro needs to be called *once* in some header file to create the
 * function prototypes and typedefs for a specific dgraph implementation.
 *
 * @param name the unique name used to suffix generated functions and macros
 * @param node_data_t the data type which is to be attached to nodes
 * @param edge_data_t the data type which is to be attached to edges
 */
#define DGRAPH_HEADER_INIT(name, node_data_t, edge_data_t) \
	_DGRAPH_TYPE(name, node_data_t, edge_data_t) \
	_DGRAPH_PROTOTYPES(name, node_data_t, edge_data_t)

/**
 * @brief generate the source code for a typed graph implementation
 *
 * This macro needs to be called *once* in some C source file to create the
 * function implementations needed by dgraph.
 *
 * Parameters used here MUST match those given to DGRAPH_HEADER_INIT().
 *
 * @param name the unique name used to suffix generated functions and macros
 * @param node_data_t the data type which is to be attached to nodes
 * @param edge_data_t the data type which is to be attached to edges
 */
#define DGRAPH_IMPL(name, node_data_t, edge_data_t) \
	_DGRAPH_IMPL(name, , node_data_t, edge_data_t)

/**
 * @brief construct the C type name for the named graph type
 *
 * @param name the name of the graph type
 */
#define dgraph_t(name) dgraph_##name

/**
 * @brief allocate and initialize a new graph instance
 *
 * This method will create a new pointer of type dgraph_t(name)* and initialize
 * all fields to contain appropriate initial values, including allocating all
 * required vectors and hash tables.
 *
 * @param name the name of the graph type
 *
 * @return the newly allocated graph instance
 */
#define dgraph_init(name) dgraph_init_##name()

/**
 * @brief destroy a previously allocated graph object, free-ing all allocated
 * memory.
 *
 * Keep in mind that this method will only free memory allocated using
 * dgraph. If you have declared a graph type which attaches pointer data
 * to nodes or edges, you must free this memory appropriately before
 * calling dgraph_destroy()
 *
 * @param name the name of the graph type
 * @param g dgraph_t(name)* pointing to a previously allocated graph
 */
#define dgraph_destroy(name, g) dgraph_destroy_##name(g)

/**
* @brief create a new graph node attached to the given graph
*
 * @param name the name of the graph type
 * @param g dgraph_t(name)* pointing to a previously allocated graph
 * @param nodeid dgraph_id* which will be overwritten with the the ID of the
 *  new node
 * @param data data which should be attached to node on creation
 *
 * @return dgraph_error indicating the result of the operatin
*/
#define dgraph_create_node(name, g, nodeid, data) \
		dgraph_create_node_##name(g, nodeid, data)


/**
 * @brief create a new graph edge attached to the given graph
 *
 * @param name the name of the graph type
 * @param g dgraph_t(name)* pointing to a previously allocated graph
 * @param nodeid dgraph_id* which will be overwritten with the the ID of the
 *  new node
 * @param data data which should be attached to node on creation
 * @param source the source node ID which the edge should be attached to
 * @param sink the sink node ID which the edge should be attached to
 *
 * @return dgraph_error indicating the result of the operatin
*/
#define dgraph_create_edge(name, g, edgeid, data, source, sink) \
		dgraph_create_edge_##name(g, edgeid, data, source, sink)


/**
* @brief retrieve the number of nodes present in a given graph
 *
 * @param name the name of the graph type
 * @param g dgraph_t(name)* pointing to a previously allocated graph
 *
 * @return the number of nodes in the graph
 */
#define dgraph_n_nodes(name, g) dgraph_n_nodes_##name(g)

/**
* @brief retrieve the number of edges present in a given graph
 *
 * @param name the name of the graph type
 * @param g dgraph_t(name)* pointing to a previously allocated graph
 *
 * @return the number of edges in the graph
 */
#define dgraph_n_edges(name, g) dgraph_n_edges_##name(g)

/* TODO: docs */
#define dgraph_foreach_node(name, g, idvar, code) \
	do { \
		for(idvar=0 ; idvar < dgraph_n_nodes(name, g) ; idvar++) { \
			code \
		} \
	} while(0)

/* TODO: docs */
#define dgraph_foreach_edge(name, g, idvar, code) \
	do { \
		for(idvar=0 ; idvar < dgraph_n_edges(name, g) ; idvar++) { \
			code \
		} \
	} while(0)

/**
 * @brief iterate over nodes adjacent to a given node
 *
 * This method runs a given code block once for each node which has an in-edge
 * with nodeid as the origin node. Put differently, this iterates over all
 * successor nodes to the specified node ID. For example:
 *
 * dgraph_id id;
 * dgraph_foreach_adjacent(sometype, g, 123, id, 
 *     printf("traversed node %d adjacent to node %d\n", id, 123);
 * );
 *
 * @param name the name of the graph type
 * @param g dgraph_t(name)* pointing to a previously allocated graph
 * @param nodeid the node ID which to find nodes adjacent to
 * @param idvar a variable of type dgraph_id which will be overwritten each
 *	iteration before the code block is run with the present node ID which
 *	is adjacent to nodeid
 * @param code is a block of C code to run in each iteration
 */
#define dgraph_foreach_adjacent(name, g, nodeid, idvar, code) \
	do { \
		vec_dgraph_id _eidvec; \
		khint_t _k; \
		_k = kh_get(dgraph_id2idlist, g->edges_by_source, nodeid); \
		if (_k == kh_end(g->edges_by_source)) { break; } \
		_eidvec = kh_value(g->edges_by_source, _k); \
		int _unused_index; \
		dgraph_id _eid; \
		vec_foreach(&(_eidvec), _eid, _unused_index) { \
			idvar = g->edge_sinks.data[_eid]; \
			code \
		} \
	} while(0);





#endif /* DGRAPH_H */
