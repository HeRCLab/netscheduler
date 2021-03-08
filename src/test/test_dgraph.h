#ifndef TEST_DGRAPH_H
#define TEST_DGRAPH_H

#include "../dgraph.h"

DGRAPH_HEADER_INIT(intgraph, int, int)

void test_dgraph_main(void);
void test_init_destroy(void);
void test_simple(void);
void test_create_delete(void);
void test_traversal(void);
dgraph_t(intgraph)* make_sample_graph(void);


#endif
