#include "test_dgraph.h"
#include "test_util.h"

DGRAPH_IMPL(intgraph, int, int)

void test_dgraph_main(void) {
	test_init_destroy();
	test_simple();
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

	dgraph_destroy(intgraph, g);

	printf("OK\n");
}
