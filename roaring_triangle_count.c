#include "roaring_triangle_count.h"
#include "utility.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <roaring/roaring.h> // Make sure this header is installed via CRoaring

void roaring_triangle_count_init(RoaringTriangleCount *rtc)
{
    rtc->v_num = 0;
    rtc->e_num = 0;
    rtc->edge_vec.data = NULL;
    rtc->edge_vec.count = 0;
    rtc->edge_vec.capacity = 0;
    rtc->graph = NULL;
}

void roaring_triangle_count_free(RoaringTriangleCount *rtc)
{
    if (rtc->graph)
    {
        for (int i = 0; i < rtc->v_num; ++i)
            roaring_bitmap_free(rtc->graph[i]);
        free(rtc->graph);
    }
    if (rtc->edge_vec.data)
        free(rtc->edge_vec.data);
}

void roaring_triangle_count_build(RoaringTriangleCount *rtc, EdgeVec input_edges)
{
    EdgeVec rev_edges;
    rev_edges.count = 0;
    rev_edges.capacity = input_edges.count / 2 + 1;
    rev_edges.data = (Edge *)malloc(sizeof(Edge) * rev_edges.capacity);

    rtc->edge_vec.count = 0;
    rtc->edge_vec.capacity = input_edges.count / 2 + 1;
    rtc->edge_vec.data = (Edge *)malloc(sizeof(Edge) * rtc->edge_vec.capacity);

    for (int i = 0; i < input_edges.count; ++i)
    {
        Edge e = input_edges.data[i];
        if (e.first < e.second)
        {
            if (e.second >= rtc->v_num)
                rtc->v_num = e.second + 1;
            rtc->edge_vec.data[rtc->edge_vec.count++] = e;
        }
        else if (e.first > e.second)
        {
            rev_edges.data[rev_edges.count++] = e;
        }
    }

    qsort(rtc->edge_vec.data, rtc->edge_vec.count, sizeof(Edge), edge_idpair_cmp);
    qsort(rev_edges.data, rev_edges.count, sizeof(Edge), edge_idpair_cmp);

    rtc->e_num = rtc->edge_vec.count;

    rtc->graph = (roaring_bitmap_t **)calloc(rtc->v_num, sizeof(roaring_bitmap_t *));
    for (int i = 0; i < rtc->v_num; ++i)
        rtc->graph[i] = roaring_bitmap_create();

    for (int i = 0; i < rev_edges.count; ++i)
    {
        Edge e = rev_edges.data[i];
        roaring_bitmap_add(rtc->graph[e.first], (uint32_t)e.second);
    }

    free(rev_edges.data);
    printf("v_num=%d e_num=%lld\n", rtc->v_num, rtc->e_num);
}

int roaring_triangle_count(RoaringTriangleCount *rtc)
{
    int res = 0;
    for (int i = 0; i < rtc->edge_vec.count; ++i)
    {
        Edge e = rtc->edge_vec.data[i];
        roaring_bitmap_t *Nu = rtc->graph[e.first];
        roaring_bitmap_t *Nv = rtc->graph[e.second];
        res += (int)roaring_bitmap_and_cardinality(Nu, Nv);
    }
    return res;
}
