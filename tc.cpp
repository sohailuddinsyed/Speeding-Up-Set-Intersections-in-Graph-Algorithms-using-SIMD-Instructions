#include "util.hpp"

#include "bitpack_triangle_count.hpp"

using namespace std;

struct timeval time_start;
struct timeval time_end;

string graph_file_path = "../data/youtube_cont_GRO.txt";

BPTriangleCount tc;

int main(int argc, char *argv[])
{
    if (argc > 1)
        graph_file_path = std::string(argv[1]);

    auto edge_vec = load_graph(graph_file_path);
    printf("load_graph done...\n");

    gettimeofday(&time_start, NULL);
    tc.build(edge_vec);
    gettimeofday(&time_end, NULL);
    double build_time = (time_end.tv_sec - time_start.tv_sec) * 1000.0 + (time_end.tv_usec - time_start.tv_usec) / 1000.0;
    printf("build_time=%.3fms\n", build_time);

    gettimeofday(&time_start, NULL);
    int triangle_num = 0;

    triangle_num = tc.triangle_count();

    gettimeofday(&time_end, NULL);
    double triangle_count_time = (time_end.tv_sec - time_start.tv_sec) * 1000.0 + (time_end.tv_usec - time_start.tv_usec) / 1000.0;

    printf("TC on %s:\n", graph_file_path.c_str());
    printf("triangle_num=%d time=%.3fms\n", triangle_num, triangle_count_time);
    printf("cmp_cnt=%llu\n", cmp_cnt);

    return 0;
}