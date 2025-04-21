#include "graph_data_utils.hpp"
#include "simd_layout_counter.hpp"
#include "vectorized_set_operations.hpp"

using namespace std;

struct timeval t_start;
struct timeval t_end;

string input_edge_path = "../data/youtube_cont_GRO.txt";

SIMDTriangleAnalyzer analyzer;

int main(int argc, char *argv[])
{
    if (argc > 1)
        input_edge_path = std::string(argv[1]);

    auto loaded_edges = read_edge_list_from_file(input_edge_path);
    printf("load_graph done.\n");

    gettimeofday(&t_start, NULL);
    analyzer.initialize_vertex_table_data(loaded_edges);
    gettimeofday(&t_end, NULL);
    double build_time = (t_end.tv_sec - t_start.tv_sec) * 1000.0 + (t_end.tv_usec - t_start.tv_usec) / 1000.0;
    printf("build_time=%.3fms\n", build_time);

    gettimeofday(&t_start, NULL);
    int final_triangle_count = 0;

    final_triangle_count = analyzer.count_all_triangles();

    gettimeofday(&t_end, NULL);
    double triangle_count_time = (t_end.tv_sec - t_start.tv_sec) * 1000.0 + (t_end.tv_usec - t_start.tv_usec) / 1000.0;

    printf("TC on %s:\n", input_edge_path.c_str());
    printf("triangle_num=%d time=%.3fms\n", final_triangle_count, triangle_count_time);
    printf("cmp_cnt=%llu\n", cmp_cnt);

    return 0;
}