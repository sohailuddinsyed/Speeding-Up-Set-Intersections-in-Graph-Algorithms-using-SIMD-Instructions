#include "bitpack_triangle_count.h"
#include "utility.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>
#include <sys/time.h>

int con_thread_num = 1;
int *con_res = NULL;
BPTriangleCount *bptc = NULL;

void bp_triangle_count_init(BPTriangleCount *tc)
{
    tc->v_num = 0;
    tc->e_num = 0;
    tc->edge_vec.data = NULL;
    tc->edge_vec.count = 0;
    tc->edge_vec.capacity = 0;
    tc->graph = NULL;
    align_malloc((void **)&tc->pool_base, 32, sizeof(int) * PACK_NODE_POOL_SIZE);
    align_malloc((void **)&tc->pool_state, 32, sizeof(PackState) * PACK_NODE_POOL_SIZE);
}

void bp_triangle_count_free(BPTriangleCount *tc)
{
    if (tc->pool_state)
        free(tc->pool_state);
    if (tc->pool_base)
        free(tc->pool_base);
    if (tc->graph)
        free(tc->graph);
    if (tc->edge_vec.data)
        free(tc->edge_vec.data);
}

void bp_triangle_count_build(BPTriangleCount *tc, EdgeVec edge_input)
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
    // unique edges (optional)
    tc->e_num = tc->edge_vec.count;

    tc->graph = (UVertex *)calloc(tc->v_num, sizeof(UVertex));
    int cur_packnode_idx = -1;
    int prev_u = -1;

    for (int i = 0; i < rev_edge_vec.count; ++i)
    {
        Edge e = rev_edge_vec.data[i];
        PackBase v_base = (e.second >> PACK_SHIFT);
        PackState v_bit = ((PackState)1 << (e.second & PACK_MASK));

        if (e.first != prev_u)
        {
            prev_u = e.first;
            tc->graph[e.first].start = ++cur_packnode_idx;
            tc->graph[e.first].deg++;
            tc->pool_base[cur_packnode_idx] = v_base;
            tc->pool_state[cur_packnode_idx] = v_bit;
        }
        else
        {
            if (tc->pool_base[cur_packnode_idx] == v_base)
            {
                tc->pool_state[cur_packnode_idx] |= v_bit;
            }
            else
            {
                tc->graph[e.first].deg++;
                tc->pool_base[++cur_packnode_idx] = v_base;
                tc->pool_state[cur_packnode_idx] = v_bit;
            }
        }
    }
    cur_packnode_idx++;
    double comp_ratio = (double)cur_packnode_idx / tc->e_num;
    printf("comp_ratio=%d/%lld, %.4f\n", cur_packnode_idx, tc->e_num, comp_ratio);

    free(rev_edge_vec.data);
}

int bp_triangle_count(BPTriangleCount *tc)
{
    int res = 0;
    for (int i = 0; i < tc->edge_vec.count; ++i)
    {
        Edge e = tc->edge_vec.data[i];
        UVertex u = tc->graph[e.first];
        UVertex v = tc->graph[e.second];

#if SIMD_STATE == 2
        res += bp_intersect_scalar2x_count(tc->pool_base + u.start, tc->pool_state + u.start, u.deg,
                                           tc->pool_base + v.start, tc->pool_state + v.start, v.deg);
#elif SIMD_STATE == 4
#if SIMD_MODE == 0
        res += bp_intersect_simd4x_count(tc->pool_base + u.start, tc->pool_state + u.start, u.deg,
                                         tc->pool_base + v.start, tc->pool_state + v.start, v.deg);
#else
        res += bp_intersect_filter_simd4x_count(tc->pool_base + u.start, tc->pool_state + u.start, u.deg,
                                                tc->pool_base + v.start, tc->pool_state + v.start, v.deg);
#endif
#else
        res += bp_intersect_count(tc->pool_base + u.start, tc->pool_state + u.start, u.deg,
                                  tc->pool_base + v.start, tc->pool_state + v.start, v.deg);
#endif
    }
    return res;
}

void *con_calc_triangle(void *con_id)
{
    long id = (long)(con_id);
    long long e_idx_l = bptc->edge_vec.count / con_thread_num * id;
    long long e_idx_r = bptc->edge_vec.count / con_thread_num * (id + 1) - 1;
    if (id + 1 == con_thread_num)
        e_idx_r = bptc->edge_vec.count - 1;

    for (long long i = e_idx_l; i <= e_idx_r; ++i)
    {
        Edge e = bptc->edge_vec.data[i];
        UVertex u = bptc->graph[e.first];
        UVertex v = bptc->graph[e.second];

#if SIMD_STATE == 2
        con_res[id] += bp_intersect_scalar2x_count(bptc->pool_base + u.start, bptc->pool_state + u.start, u.deg,
                                                   bptc->pool_base + v.start, bptc->pool_state + v.start, v.deg);
#elif SIMD_STATE == 4
#if SIMD_MODE == 0
        con_res[id] += bp_intersect_simd4x_count(bptc->pool_base + u.start, bptc->pool_state + u.start, u.deg,
                                                 bptc->pool_base + v.start, bptc->pool_state + v.start, v.deg);
#else
        con_res[id] += bp_intersect_filter_simd4x_count(bptc->pool_base + u.start, bptc->pool_state + u.start, u.deg,
                                                        bptc->pool_base + v.start, bptc->pool_state + v.start, v.deg);
#endif
#else
        con_res[id] += bp_intersect_count(bptc->pool_base + u.start, bptc->pool_state + u.start, u.deg,
                                          bptc->pool_base + v.start, bptc->pool_state + v.start, v.deg);
#endif
    }
    pthread_exit(NULL);
}

int bp_triangle_count_mt(BPTriangleCount *tc, int thread_num)
{
    con_thread_num = thread_num;
    align_malloc((void **)&con_res, 32, sizeof(int) * thread_num);
    memset(con_res, 0, sizeof(int) * thread_num);
    bptc = tc;

    pthread_t *pt = (pthread_t *)malloc(sizeof(pthread_t) * thread_num);
    struct timeval time_start, time_end;
    gettimeofday(&time_start, NULL);
    for (long i = 0; i < thread_num; ++i)
        pthread_create(&pt[i], NULL, con_calc_triangle, (void *)i);
    for (int i = 0; i < thread_num; ++i)
        pthread_join(pt[i], NULL);
    gettimeofday(&time_end, NULL);
    double tc_mt_time = (time_end.tv_sec - time_start.tv_sec) * 1000.0 +
                        (time_end.tv_usec - time_start.tv_usec) / 1000.0;
    printf("tc_mt_time=%.3fms\n", tc_mt_time);
    free(pt);

    int total = 0;
    for (int i = 0; i < thread_num; ++i)
        total += con_res[i];
    free(con_res);
    return total;
}
