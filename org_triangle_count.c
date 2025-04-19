#include "org_triangle_count.h"
#include "utility.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

void org_triangle_count_init(OrgTriangleCount *tc)
{
    tc->v_num = 0;
    tc->e_num = 0;
    tc->edge_vec.data = NULL;
    tc->edge_vec.count = 0;
    tc->edge_vec.capacity = 0;
    tc->graph = NULL;
    align_malloc((void **)&tc->pool_edges, 32, sizeof(int) * PACK_NODE_POOL_SIZE);
}

void org_triangle_count_free(OrgTriangleCount *tc)
{
    if (tc->pool_edges)
        free(tc->pool_edges);
    if (tc->graph)
        free(tc->graph);
    if (tc->edge_vec.data)
        free(tc->edge_vec.data);
}

void org_triangle_count_build(OrgTriangleCount *tc, EdgeVec edge_input)
{
    EdgeVec rev_edge_vec;
    rev_edge_vec.count = 0;
    rev_edge_vec.capacity = edge_input.count / 2 + 1;
    rev_edge_vec.data = (Edge *)malloc(sizeof(Edge) * rev_edge_vec.capacity);

    tc->edge_vec.count = 0;
    tc->edge_vec.capacity = edge_input.count / 2 + 1;
    tc->edge_vec.data = (Edge *)malloc(sizeof(Edge) * tc->edge_vec.capacity);

    for (int i = 0; i < edge_input.count; ++i)
    {
        Edge e = edge_input.data[i];
        if (e.first < e.second)
        {
            if (e.second >= tc->v_num)
                tc->v_num = e.second + 1;
            tc->edge_vec.data[tc->edge_vec.count++] = e;
        }
        else if (e.first > e.second)
        {
            rev_edge_vec.data[rev_edge_vec.count++] = e;
        }
    }

    qsort(tc->edge_vec.data, tc->edge_vec.count, sizeof(Edge), edge_idpair_cmp);
    qsort(rev_edge_vec.data, rev_edge_vec.count, sizeof(Edge), edge_idpair_cmp);

    tc->e_num = tc->edge_vec.count;
    tc->graph = (UVertex *)calloc(tc->v_num, sizeof(UVertex));

    int cur_node_idx = 0;
    int prev_u = -1;
    for (int i = 0; i < rev_edge_vec.count; ++i)
    {
        Edge e = rev_edge_vec.data[i];
        if (e.first != prev_u)
        {
            prev_u = e.first;
            tc->graph[e.first].start = cur_node_idx;
        }
        tc->graph[e.first].deg++;
        tc->pool_edges[cur_node_idx++] = e.second;
    }

    printf("v_num=%d e_num=%lld\n", tc->v_num, tc->e_num);
    free(rev_edge_vec.data);
}

int org_triangle_count(OrgTriangleCount *tc)
{
    int res = 0;
    for (int i = 0; i < tc->edge_vec.count; ++i)
    {
        Edge e = tc->edge_vec.data[i];
        UVertex u = tc->graph[e.first];
        UVertex v = tc->graph[e.second];

#if SIMD_STATE == 2
        res += intersect_scalar2x_count(tc->pool_edges + u.start, u.deg,
                                        tc->pool_edges + v.start, v.deg);
#elif SIMD_STATE == 4
        res += intersect_simd4x_count(tc->pool_edges + u.start, u.deg,
                                      tc->pool_edges + v.start, v.deg);
#else
        res += intersect_count(tc->pool_edges + u.start, u.deg,
                               tc->pool_edges + v.start, v.deg);
#endif
    }
    return res;
}
