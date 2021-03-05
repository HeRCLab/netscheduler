/* Copyright 2021 Jason Bakos, Philip Conrad, Charles Daniels */

/* This library implements a general-purpose directed graph data structure,
 * which supports attaching arbitrary data to nodes and links.  */

#ifndef DGRAPH_H
#define DGRAPH_H

#include <herc/khash.h>
#include <herc/vec.h>
#include <stdint.h>

typedef int dgraph_id;

typedef vec_t(dgraph_id) vec_dgraph_id;

typedef enum {
	DGRAPH_ERROR_OK = 0,
	DGRAPH_ERROR_MALLOC_FAILED,
	DGRAPH_ERROR_EDGE_SOURCE_NONEXISTANT,
	DGRAPH_ERROR_EDGE_SINK_NONEXISTANT,
} dgraph_error;

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





#endif /* DGRAPH_H */
