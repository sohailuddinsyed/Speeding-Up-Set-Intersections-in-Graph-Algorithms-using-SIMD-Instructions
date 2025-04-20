#include "util.h"
#include "bitpack_triangle_count.h"

#include <sys/time.h>
#include <stdio.h>
#include <string.h>

int main(int argc, char *argv[])
{
    struct timeval time_start, time_end;

    const char *graph_file_path = "../data/youtube_cont_GRO.txt";
    if (argc > 1)
    {
        graph_file_path = argv[1];
    }

    EdgeVector edge_vec = load_graph(graph_file_path);
    printf("load_graph done..\n");

    BPTriangleCount *tc = bp_create();

    gettimeofday(&time_start, NULL);
    bp_build(tc, &edge_vec);
    gettimeofday(&time_end, NULL);
    double build_time = (time_end.tv_sec - time_start.tv_sec) * 1000.0 +
                        (time_end.tv_usec - time_start.tv_usec) / 1000.0;
    printf("build_time=%.3fms\n", build_time);

    gettimeofday(&time_start, NULL);
    int triangle_num = bp_triangle_count(tc);
    gettimeofday(&time_end, NULL);

    double triangle_count_time = (time_end.tv_sec - time_start.tv_sec) * 1000.0 +
                                 (time_end.tv_usec - time_start.tv_usec) / 1000.0;

    printf("TC on %s:\n", graph_file_path);
    printf("triangle_num=%d time=%.3fms\n", triangle_num, triangle_count_time);
    printf("cmp_cnt=%llu\n", cmp_cnt);

    // Cleanup
    edge_vector_free(&edge_vec);
    bp_destroy(tc);

    return 0;
}