#include "./include/simd_layout_counter.hpp"
#include "./include/vectorized_set_operations.hpp"
#include <iomanip>
#include <fstream>
#include <sstream>
#include <cctype>
#include <chrono>

using namespace std;
using namespace std::chrono;

SIMDTriangleAnalyzer analyzer;

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        cerr << "Usage: " << argv[0] << " <input_edge_file_path>" << endl;
        return EXIT_FAILURE;
    }

    string input_edge_path = std::string(argv[1]);

    EdgeListContainer loaded_edges;
    FILE *file_stream = fopen(input_edge_path.c_str(), "r");
    if (!file_stream)
    {
        perror(("Failed to open: " + input_edge_path).c_str());
        exit(EXIT_FAILURE);
    }

    char file_line[512];
    while (true)
    {
        if (!fgets(file_line, sizeof(file_line), file_stream))
            break;

        switch (file_line[0])
        {
        case '#':
            continue;
        default:
            break;
        }

        int source_vertex = 0, target_vertex = 0;
        const char *cursor = file_line;

        for (; isdigit(*cursor); ++cursor)
            source_vertex = (source_vertex << 3) + (source_vertex << 1) + (*cursor - '0');

        cursor += (*cursor == ' ' || *cursor == '\t' || *cursor == ',') ? 1 : 0;

        for (; isdigit(*cursor); ++cursor)
            target_vertex = (target_vertex << 3) + (target_vertex << 1) + (*cursor - '0');

        loaded_edges.emplace_back(source_vertex, target_vertex);
    }

    fclose(file_stream);

    // --- Measure build time ---
    auto start_time = high_resolution_clock::now();
    auto [nodes, edges, ratio] = analyzer.initialize_vertex_table_data(loaded_edges);
    auto end_time = high_resolution_clock::now();

    double build_time = duration_cast<milliseconds>(end_time - start_time).count();

    // --- Measure triangle count time ---
    start_time = high_resolution_clock::now();
    auto [triangle_count, comparisons] = analyzer.count_all_triangles();
    end_time = high_resolution_clock::now();

    double triangle_count_time = duration_cast<milliseconds>(end_time - start_time).count();
    // ---- Output as formatted table ----
    std::cout << "\n=========== Triangle Counting Summary ===========\n";
    std::cout << std::left << std::setw(30) << "Input Graph Path"
              << ": " << input_edge_path << "\n";

    std::cout << std::left << std::setw(30) << "Vertex Table Size"
              << ": " << nodes << "\n";

    std::cout << std::left << std::setw(30) << "Edge Count (Forward Edges)"
              << ": " << edges << "\n";

    std::cout << std::left << std::setw(30) << "Compression Ratio"
              << ": " << std::fixed << std::setprecision(4) << ratio << "\n";

    std::cout << std::left << std::setw(30) << "Graph Build Time (ms)"
              << ": " << std::fixed << std::setprecision(3) << build_time << "\n";

    std::cout << std::left << std::setw(30) << "Triangle Count"
              << ": " << triangle_count << "\n";

    std::cout << std::left << std::setw(30) << "Triangle Count Time (ms)"
              << ": " << std::fixed << std::setprecision(3) << triangle_count_time << "\n";

    std::cout << std::left << std::setw(30) << "Total SIMD Comparisons"
              << ": " << comparisons << "\n";
    std::cout << "==================================================\n";

    return 0;
}
