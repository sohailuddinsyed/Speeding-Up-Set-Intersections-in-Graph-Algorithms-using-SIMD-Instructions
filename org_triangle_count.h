
#ifndef ORG_TRIANGLE_COUNT_H
#define ORG_TRIANGLE_COUNT_H

#include "utility.h"
#include <stdint.h>

#define PACK_NODE_POOL_SIZE (128 * 1024 * 1024)

typedef struct
{
    int v_num;
    long long e_num;
    EdgeVec edge_vec;
    UVertex *graph;
    int *pool_edges;
} OrgTriangleCount;

void org_triangle_count_init(OrgTriangleCount *tc);
void org_triangle_count_free(OrgTriangleCount *tc);
void org_triangle_count_build(OrgTriangleCount *tc, EdgeVec input_edges);
int org_triangle_count(OrgTriangleCount *tc);

int intersect_scalar2x_count(int *a, int size_a, int *b, int size_b);
int intersect_simd4x_count(int *a, int size_a, int *b, int size_b);
int intersect_count(int *a, int size_a, int *b, int size_b);

#endif
