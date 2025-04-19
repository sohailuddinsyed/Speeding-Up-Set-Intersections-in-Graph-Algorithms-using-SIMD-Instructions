#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#include "utility.h"
#include "bitpack_triangle_count.h"

struct timeval time_start;
struct timeval time_end;

char *graph_file_path = "./data/youtube_cont_GRO.txt";

int main(int argc, char *argv[])
{
    if (argc > 1)
    {
        graph_file_path = argv[1];
    }

    EdgeVec edge_vec = load_graph(graph_file_path);
    printf("load_graph done.\n");

    BPTriangleCount tc;
    bp_triangle_count_init(&tc);

    gettimeofday(&time_start, NULL);
    bp_triangle_count_build(&tc, edge_vec);
    gettimeofday(&time_end, NULL);
    printf("build_time=%.3fms\n", (time_end.tv_sec - time_start.tv_sec) * 1000.0 +
                                      (time_end.tv_usec - time_start.tv_usec) / 1000.0);

    gettimeofday(&time_start, NULL);
    int triangle_num = bp_triangle_count(&tc);
    gettimeofday(&time_end, NULL);
    double triangle_count_time = (time_end.tv_sec - time_start.tv_sec) * 1000.0 +
                                 (time_end.tv_usec - time_start.tv_usec) / 1000.0;

    printf("TC on %s:\n", graph_file_path);
    printf("triangle_num=%d time=%.3fms\n", triangle_num, triangle_count_time);
    // printf("cmp_cnt=%llu\n", cmp_cnt); // remove or declare cmp_cnt

    return 0;
}
