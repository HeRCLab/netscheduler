#include "test_dgraph.h"
#include "test_util.h"

DGRAPH_IMPL(intgraph, int, int)

void test_dgraph_main(void) {
	test_init_destroy();
	test_simple();
	test_traversal();
	test_create_delete();
}

void test_init_destroy(void) {
	printf("test_dgraph/test_init_destroy... ");

	dgraph_intgraph* g = dgraph_init(intgraph);
	dgraph_destroy(intgraph, g);

	printf("OK\n");
}

void test_simple(void) {
	dgraph_id id;
	dgraph_error status;

	printf("test_dgraph/test_simple... ");

	dgraph_intgraph* g = dgraph_init_intgraph();

	/* Create two nodes and an edge connecting them. */
	status = dgraph_create_node(intgraph, g, &id, 123);
	should_equal(status, DGRAPH_ERROR_OK);
	should_equal(id, 0);
	status = dgraph_create_node(intgraph, g, &id, 456);
	should_equal(status, DGRAPH_ERROR_OK);
	should_equal(id, 1);
	status = dgraph_create_edge(intgraph, g, &id, 789, 0, 1);
	should_equal(status, DGRAPH_ERROR_OK);
	should_equal(id, 0);

	/* check that n_nodes and n_edges functions work properly */
	should_equal(dgraph_n_nodes(intgraph, g), 2);
	should_equal(dgraph_n_edges(intgraph, g), 1);

	/* check that adjacency works */
	dgraph_foreach_successor(intgraph, g, 0, id,
		/* Only node 1 is adjacent to node 0, this should run
		 * exactly once. */
		should_equal(id, 1);
	);

	dgraph_foreach_successor(intgraph, g, 1, id,
		/* No nodes are adjacent to node 1, since the edge goes from
		 * 0 to 1, so the body of this loop should never run. */
		fail("this should not be reachable (id=%d)", id);
	);

	dgraph_destroy(intgraph, g);

	printf("OK\n");
}

void test_create_delete(void) {
	dgraph_id id;
	dgraph_error status;

	printf("test_dgraph/test_create_delete... ");

	dgraph_intgraph* g = dgraph_init_intgraph();

	/* Create two nodes. */
	status = dgraph_create_node(intgraph, g, &id, 123);
	should_equal(status, DGRAPH_ERROR_OK);
	should_equal(id, 0);
	status = dgraph_create_node(intgraph, g, &id, 456);
	should_equal(status, DGRAPH_ERROR_OK);
	should_equal(id, 1);
	should_equal(dgraph_n_nodes(intgraph, g), 2);
	should_be_true(dgraph_node_exists(intgraph, g, 0));
	should_be_true(dgraph_node_exists(intgraph, g, 1));

	/* delete node 0 */
	status = dgraph_destroy_node(intgraph, g, 0);
	should_equal(status, DGRAPH_ERROR_OK);
	should_equal(dgraph_n_nodes(intgraph, g), 1);
	should_be_false(dgraph_node_exists(intgraph, g, 0));
	should_be_true(dgraph_node_exists(intgraph, g, 1));

	/* create a new node and make sure it re-uses the 0 slot */
	status = dgraph_create_node(intgraph, g, &id, 789);
	should_equal(status, DGRAPH_ERROR_OK);
	should_equal(id, 0);
	should_equal(dgraph_n_nodes(intgraph, g), 2);
	should_be_true(dgraph_node_exists(intgraph, g, 0));
	should_be_true(dgraph_node_exists(intgraph, g, 1));
	should_be_false(dgraph_node_exists(intgraph, g, 2));

	/* create an edge */
	status = dgraph_create_edge(intgraph, g, &id, 789, 0, 1);
	should_equal(status, DGRAPH_ERROR_OK);
	should_equal(id, 0);

	/* deleting either node should fail and not change the graph, because
	 * performing the deletion would leave a dangling edge */
	status = dgraph_destroy_node(intgraph, g, 0);
	should_equal(status, DGRAPH_ERROR_SOURCE);
	should_equal(dgraph_n_nodes(intgraph, g), 2);
	should_be_true(dgraph_node_exists(intgraph, g, 0));
	should_be_true(dgraph_node_exists(intgraph, g, 1));

	status = dgraph_destroy_node(intgraph, g, 1);
	should_equal(status, DGRAPH_ERROR_SINK);
	should_equal(dgraph_n_nodes(intgraph, g), 2);
	should_be_true(dgraph_node_exists(intgraph, g, 0));
	should_be_true(dgraph_node_exists(intgraph, g, 1));

	/* delete the edge */
	should_equal(dgraph_n_edges(intgraph, g), 1);
	should_be_true(dgraph_edge_exists(intgraph, g, 0));
	dgraph_foreach_successor(intgraph, g, 0, id,
		should_equal(id, 1);
	);
	status = dgraph_destroy_edge(intgraph, g, 0);
	should_be_false(dgraph_edge_exists(intgraph, g, 0));
	should_equal(dgraph_n_edges(intgraph, g), 0);
	dgraph_foreach_successor(intgraph, g, 0, id,
		fail("this should not be reachable (id=%d)", id);
	);

	/* create an edge again and make sure id 0 is re-used */
	status = dgraph_create_edge(intgraph, g, &id, 789, 0, 1);
	should_equal(status, DGRAPH_ERROR_OK);
	should_equal(id, 0);
	dgraph_foreach_successor(intgraph, g, 0, id,
		should_equal(id, 1);
	);

	dgraph_destroy(intgraph, g);

	printf("OK\n");
}

dgraph_t(intgraph)* make_sample_graph(void) {
	/* This method generates and returns the following graph, for use in
	 * other tests:
	 *
	 * 107
	 *   │
	 *   │
	 *   │
	 *   └────────────┐
	 *                │                 103
	 *                │                 ▲
	 *                ▼                 │
	 *      ┌───────►102 ──────────┐    │
	 *      │        ▲             │    │
	 *      │        │             └───►104───────────►105──────────────────►106
	 *      │        │                   ▲                                    │
	 *    100        │                   │                                    │
	 *      │        │                   │                                    │
	 *      │        │                   │                                    │
	 *      │        │                   └────────────────────────────────────┘
	 *      └──────►101◄─────┐
	 *               │       │
	 *               │       │
	 *               │       │
	 *               └───────┘
	 *
	 * Here, nodes are labeled by their attached node data, not by their
	 * actual node IDs, which are arbitrary.
	 */

	dgraph_id node_ids[8];
	dgraph_id edge_ids[10];
	dgraph_error status;

	dgraph_t(intgraph)* g = dgraph_init_intgraph();

	/* Create some nodes. */
	status = dgraph_create_node(intgraph, g, &(node_ids[0]), 100);
	should_equal(status, DGRAPH_ERROR_OK);
	status = dgraph_create_node(intgraph, g, &(node_ids[1]), 101);
	should_equal(status, DGRAPH_ERROR_OK);
	status = dgraph_create_node(intgraph, g, &(node_ids[2]), 102);
	should_equal(status, DGRAPH_ERROR_OK);
	status = dgraph_create_node(intgraph, g, &(node_ids[3]), 103);
	should_equal(status, DGRAPH_ERROR_OK);
	status = dgraph_create_node(intgraph, g, &(node_ids[4]), 104);
	should_equal(status, DGRAPH_ERROR_OK);
	status = dgraph_create_node(intgraph, g, &(node_ids[5]), 105);
	should_equal(status, DGRAPH_ERROR_OK);
	status = dgraph_create_node(intgraph, g, &(node_ids[6]), 106);
	should_equal(status, DGRAPH_ERROR_OK);
	status = dgraph_create_node(intgraph, g, &(node_ids[7]), 107);
	should_equal(status, DGRAPH_ERROR_OK);
	should_equal(dgraph_n_nodes(intgraph, g), 8);

	status = dgraph_create_edge(intgraph, g, &(edge_ids[0]), 0, node_ids[0], node_ids[2]);
	should_equal(status, DGRAPH_ERROR_OK);
	status = dgraph_create_edge(intgraph, g, &(edge_ids[1]), 0, node_ids[0], node_ids[1]);
	should_equal(status, DGRAPH_ERROR_OK);
	status = dgraph_create_edge(intgraph, g, &(edge_ids[2]), 0, node_ids[1], node_ids[1]);
	should_equal(status, DGRAPH_ERROR_OK);
	status = dgraph_create_edge(intgraph, g, &(edge_ids[3]), 0, node_ids[2], node_ids[4]);
	should_equal(status, DGRAPH_ERROR_OK);
	status = dgraph_create_edge(intgraph, g, &(edge_ids[4]), 0, node_ids[4], node_ids[5]);
	should_equal(status, DGRAPH_ERROR_OK);
	status = dgraph_create_edge(intgraph, g, &(edge_ids[5]), 0, node_ids[4], node_ids[3]);
	should_equal(status, DGRAPH_ERROR_OK);
	status = dgraph_create_edge(intgraph, g, &(edge_ids[6]), 0, node_ids[5], node_ids[6]);
	should_equal(status, DGRAPH_ERROR_OK);
	status = dgraph_create_edge(intgraph, g, &(edge_ids[7]), 0, node_ids[6], node_ids[4]);
	should_equal(status, DGRAPH_ERROR_OK);
	status = dgraph_create_edge(intgraph, g, &(edge_ids[8]), 0, node_ids[1], node_ids[2]);
	should_equal(status, DGRAPH_ERROR_OK);
	status = dgraph_create_edge(intgraph, g, &(edge_ids[9]), 0, node_ids[7], node_ids[2]);
	should_equal(status, DGRAPH_ERROR_OK);

	return g;

}

void test_traversal(void) {
	dgraph_id node_ids[8];
	dgraph_id id;
	vec_dgraph_id order;
	int idx;

	for (int i = 0 ; i < 8 ; i++) {node_ids[0] = -1;}

	printf("test_dgraph/test_traversal... ");

	vec_init(&order);

	dgraph_t(intgraph)* g = make_sample_graph();

	/* int data; */
	/* dgraph_debug_dump(stderr, intgraph, g, data, printf("%d\n", data);, data, printf("%d\n", data);); */

	/* Get the node IDs back out for later use. We could infer what the IDs
	 * would be, but the order in which IDs are created by dgraph is
	 * an implementation detail. */
	dgraph_foreach_node(intgraph, g, id,
		if (dgraph_node_data(intgraph, g, id) == 100) {
			node_ids[0] = id;
		} else if (dgraph_node_data(intgraph, g, id) == 101) {
			node_ids[1] = id;
		} else if (dgraph_node_data(intgraph, g, id) == 102) {
			node_ids[2] = id;
		} else if (dgraph_node_data(intgraph, g, id) == 103) {
			node_ids[3] = id;
		} else if (dgraph_node_data(intgraph, g, id) == 104) {
			node_ids[4] = id;
		} else if (dgraph_node_data(intgraph, g, id) == 105) {
			node_ids[5] = id;
		} else if (dgraph_node_data(intgraph, g, id) == 106) {
			node_ids[6] = id;
		} else if (dgraph_node_data(intgraph, g, id) == 107) {
			node_ids[7] = id;
		}
	);

	for (int i = 0 ; i < 8 ; i++) {
		if (node_ids[i] < 0) {
			fail("failed to lookup node ID for node with data '%d'", 100+i);
		}
	}

	/* test getting successors */
	dgraph_foreach_successor(intgraph, g, node_ids[4], id,
		vec_push(&order, id);
	);
	should_equal(order.length, 2);
	vec_find(&order, 3, idx);
	should_be_true(idx >= 0);
	vec_find(&order, 5, idx);
	should_be_true(idx >= 0);

	/* clear order */
	vec_deinit(&order);
	vec_init(&order);

	/* test getting ancestors */
	dgraph_foreach_ancestor(intgraph, g, node_ids[4], id,
		vec_push(&order, id);
	);
	should_equal(order.length, 2);
	vec_find(&order, 2, idx);
	should_be_true(idx >= 0);
	vec_find(&order, 6, idx);
	should_be_true(idx >= 0);

	/* clear order */
	vec_deinit(&order);
	vec_init(&order);

	/* make sure ancestors works correctly with loopes */
	dgraph_foreach_ancestor(intgraph, g, node_ids[1], id,
		vec_push(&order, id);
	);
	should_equal(order.length, 2);
	vec_find(&order, 1, idx);
	should_be_true(idx >= 0);
	vec_find(&order, 0, idx);
	should_be_true(idx >= 0);

	/* clear order */
	vec_deinit(&order);
	vec_init(&order);

	/* make sure successors works correctly with loopes */
	dgraph_foreach_successor(intgraph, g, node_ids[1], id,
		vec_push(&order, id);
	);
	should_equal(order.length, 2);
	vec_find(&order, 1, idx);
	should_be_true(idx >= 0);
	vec_find(&order, 2, idx);
	should_be_true(idx >= 0);

	/* clear order */
	vec_deinit(&order);
	vec_init(&order);

	/* make sure foreach_edge works */
	dgraph_foreach_edge(intgraph, g, id,
		vec_push(&order, id);
	);
	should_equal(order.length, 10);

	/* clear order */
	vec_deinit(&order);
	vec_init(&order);

	/* test foreach_adjacent */
	dgraph_foreach_adjacent(intgraph, g, node_ids[4], id,
		vec_push(&order, id);
	);
	should_equal(order.length, 4);
	vec_find(&order, 2, idx);
	should_be_true(idx >= 0);
	vec_find(&order, 3, idx);
	should_be_true(idx >= 0);
	vec_find(&order, 5, idx);
	should_be_true(idx >= 0);
	vec_find(&order, 6, idx);
	should_be_true(idx >= 0);

	vec_deinit(&order);
	dgraph_destroy(intgraph, g);
	printf("OK\n");
}
