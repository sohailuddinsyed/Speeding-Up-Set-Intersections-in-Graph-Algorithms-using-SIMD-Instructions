
#ifndef BITPACK_TRIANGLE_COUNT_H
#define BITPACK_TRIANGLE_COUNT_H

#include "utility.h"
#include <stdint.h>

#define PACK_SHIFT 5
#define PACK_MASK 31
#define PACK_NODE_POOL_SIZE (128 * 1024 * 1024)

typedef uint32_t PackState;
typedef int PackBase;

typedef struct
{
    int v_num;
    long long e_num;
    EdgeVec edge_vec;
    UVertex *graph;
    int *pool_base;
    PackState *pool_state;
} BPTriangleCount;

void bp_triangle_count_init(BPTriangleCount *tc);
void bp_triangle_count_free(BPTriangleCount *tc);
void bp_triangle_count_build(BPTriangleCount *tc, EdgeVec edge_input);
int bp_triangle_count(BPTriangleCount *tc);
int bp_triangle_count_mt(BPTriangleCount *tc, int thread_num);

int bp_intersect_scalar2x_count(int *base_u, PackState *state_u, int size_u,
                                int *base_v, PackState *state_v, int size_v);

int bp_intersect_simd4x_count(int *base_u, PackState *state_u, int size_u,
                              int *base_v, PackState *state_v, int size_v);

int bp_intersect_filter_simd4x_count(int *base_u, PackState *state_u, int size_u,
                                     int *base_v, PackState *state_v, int size_v);

int bp_intersect_count(int *base_u, PackState *state_u, int size_u,
                       int *base_v, PackState *state_v, int size_v);

#endif
