#ifndef ROARING_TRIANGLE_COUNT_H
#define ROARING_TRIANGLE_COUNT_H

#include <stdint.h>
#include "utility.h"
#include <roaring/roaring.h>

typedef struct
{
    int v_num;
    long long e_num;
    EdgeVec edge_vec;
    roaring_bitmap_t **graph;
} RoaringTriangleCount;

void roaring_triangle_count_init(RoaringTriangleCount *rtc);
void roaring_triangle_count_free(RoaringTriangleCount *rtc);
void roaring_triangle_count_build(RoaringTriangleCount *rtc, EdgeVec input_edges);
int roaring_triangle_count(RoaringTriangleCount *rtc);

#endif
