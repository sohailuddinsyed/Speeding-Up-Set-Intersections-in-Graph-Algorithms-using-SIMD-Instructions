#ifndef _BITPACK_TRIANGLE_COUNT_H
#define _BITPACK_TRIANGLE_COUNT_H

#include "util.h"
#include "set_operation.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <sys/time.h>

typedef struct
{
    int v_num;
    long long e_num;
    EdgeVector edge_vec;
    UVertex *graph;
    int *pool_base;
    PackState *pool_state;
} BPTriangleCount;

static int con_thread_num = 1;
static int *con_res = NULL;
static BPTriangleCount *bptc = NULL;

static BPTriangleCount *bp_create()
{
    BPTriangleCount *tc = (BPTriangleCount *)malloc(sizeof(BPTriangleCount));
    tc->v_num = 0;
    tc->e_num = 0;
    tc->graph = NULL;
    tc->pool_base = NULL;
    tc->pool_state = NULL;
    edge_vector_init(&tc->edge_vec);
    align_malloc((void **)&tc->pool_base, 32, sizeof(int) * PACK_NODE_POOL_SIZE);
    align_malloc((void **)&tc->pool_state, 32, sizeof(PackState) * PACK_NODE_POOL_SIZE);
    return tc;
}

static void bp_destroy(BPTriangleCount *tc)
{
    edge_vector_free(&tc->edge_vec);
    free(tc->graph);
    free(tc->pool_base);
    free(tc->pool_state);
    free(tc);
}

static void bp_build(BPTriangleCount *tc, const EdgeVector *_e_v)
{
    EdgeVector rev_edge_vec;
    edge_vector_init(&rev_edge_vec);

    for (size_t i = 0; i < _e_v->size; ++i)
    {
        Edge e = _e_v->data[i];
        if (e.first < e.second)
        {
            if (e.second > tc->v_num)
                tc->v_num = e.second;
            edge_vector_push_back(&tc->edge_vec, e);
        }
        else if (e.first > e.second)
        {
            edge_vector_push_back(&rev_edge_vec, e);
        }
    }
    tc->v_num++;

    qsort(tc->edge_vec.data, tc->edge_vec.size, sizeof(Edge), edge_idpair_cmp);
    // Remove duplicates in-place
    size_t new_size = 0;
    for (size_t i = 0; i < tc->edge_vec.size; ++i)
    {
        if (new_size == 0 || tc->edge_vec.data[i].first != tc->edge_vec.data[new_size - 1].first ||
            tc->edge_vec.data[i].second != tc->edge_vec.data[new_size - 1].second)
        {
            tc->edge_vec.data[new_size++] = tc->edge_vec.data[i];
        }
    }
    tc->edge_vec.size = new_size;

    qsort(rev_edge_vec.data, rev_edge_vec.size, sizeof(Edge), edge_idpair_cmp);
    // Remove duplicates from rev_edge_vec
    new_size = 0;
    for (size_t i = 0; i < rev_edge_vec.size; ++i)
    {
        if (new_size == 0 || rev_edge_vec.data[i].first != rev_edge_vec.data[new_size - 1].first ||
            rev_edge_vec.data[i].second != rev_edge_vec.data[new_size - 1].second)
        {
            rev_edge_vec.data[new_size++] = rev_edge_vec.data[i];
        }
    }
    rev_edge_vec.size = new_size;

    tc->e_num = (long long)tc->edge_vec.size;
    tc->graph = (UVertex *)calloc(tc->v_num, sizeof(UVertex));

    int cur_packnode_idx = -1;
    int prev_u = -1;

    for (size_t i = 0; i < rev_edge_vec.size; ++i)
    {
        Edge e = rev_edge_vec.data[i];
        PackBase v_base = (e.second >> PACK_SHIFT);
        PackState v_bit = ((PackState){0});
        v_bit.state[0] = 1 << (e.second & PACK_MASK);

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
                for (int j = 0; j < 4; ++j)
                    tc->pool_state[cur_packnode_idx].state[j] |= v_bit.state[j];
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

    edge_vector_free(&rev_edge_vec);
}

static int bp_triangle_count(BPTriangleCount *tc)
{
    int res = 0;
    for (size_t i = 0; i < tc->edge_vec.size; ++i)
    {
        Edge e = tc->edge_vec.data[i];
        const UVertex *u = &tc->graph[e.first];
        const UVertex *v = &tc->graph[e.second];
        res += bp_intersect_filter_simd4x_count(tc->pool_base + u->start, tc->pool_state + u->start, u->deg,
                                                tc->pool_base + v->start, tc->pool_state + v->start, v->deg);
    }
    return res;
}

static void *con_calc_triangle(void *con)
{
    long id = (long)con;
    long long e_idx_l = bptc->edge_vec.size / con_thread_num * id;
    long long e_idx_r = (id + 1 == con_thread_num) ? bptc->edge_vec.size - 1 : bptc->edge_vec.size / con_thread_num * (id + 1) - 1;

    for (long long i = e_idx_l; i <= e_idx_r; ++i)
    {
        Edge e = bptc->edge_vec.data[i];
        const UVertex *u = &bptc->graph[e.first];
        const UVertex *v = &bptc->graph[e.second];
        con_res[id] += bp_intersect_filter_simd4x_count(bptc->pool_base + u->start, bptc->pool_state + u->start, u->deg,
                                                        bptc->pool_base + v->start, bptc->pool_state + v->start, v->deg);
    }

    pthread_exit(NULL);
}

static int bp_triangle_count_mt(BPTriangleCount *tc, int thread_num)
{
    con_thread_num = thread_num;
    align_malloc((void **)&con_res, 32, sizeof(int) * thread_num);
    memset(con_res, 0, sizeof(int) * thread_num);
    bptc = tc;

    pthread_t *pt = (pthread_t *)malloc(thread_num * sizeof(pthread_t));
    struct timeval time_start, time_end;
    gettimeofday(&time_start, NULL);

    for (long a = 0; a < thread_num; a++)
        pthread_create(&pt[a], NULL, con_calc_triangle, (void *)a);
    for (long a = 0; a < thread_num; a++)
        pthread_join(pt[a], NULL);

    gettimeofday(&time_end, NULL);
    double tc_mt_time = (time_end.tv_sec - time_start.tv_sec) * 1000.0 +
                        (time_end.tv_usec - time_start.tv_usec) / 1000.0;
    printf("tc_mt_time=%.3fms\n", tc_mt_time);

    int tot_res = 0;
    for (int i = 0; i < thread_num; ++i)
        tot_res += con_res[i];

    free(pt);
    free(con_res);
    return tot_res;
}

#endif // _BITPACK_TRIANGLE_COUNT_H