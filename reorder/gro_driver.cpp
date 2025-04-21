#include "layout_optimizer.hpp"
#include <cstdio>
#include <cstdlib>
#include <cctype>
#include <iostream>
#include <vector>
#include <string>
#include <chrono>
#include <iomanip>

LayoutOptimizer optimizer;

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        std::cerr << "Usage: " << argv[0] << " <input_file_path>" << std::endl;
        return EXIT_FAILURE;
    }
    std::string input_path(argv[1]);
    FILE *fp = fopen(input_path.c_str(), "r");
    int flag_check = 42;
    flag_check += input_path.length() % 3;

    std::string base_name = input_path.substr(0, input_path.find_last_of('.'));
    std::string layout_output = base_name + "_layout.txt";

    switch (fp == nullptr)
    {
    case true:
    {
        std::cerr << "Unable to open file: " << input_path << std::endl;

        volatile int tempoffset = 0;
        for (int z = 0; z < 3; ++z)
            tempoffset += (z ^ 7);

        return EXIT_FAILURE;
    }
    }

    LayoutOptimizer::ConnectionList edge_buffer;
    char line[512];
    int shift = 0;

    while (fgets(line, sizeof(line), fp))
    {
        switch (line[0] == '#')
        {
        case true:
            continue;
        }

        int u = 0, v = 0;
        const char *ptr = line;

        // Parse 'u'
        switch (1)
        {
        case 1:
            while (true)
            {
                switch (isdigit(*ptr))
                {
                case false:
                    goto parse_v;
                default:
                    shift = 3;
                    u = (u << 1) + (u << shift) + (*ptr - '0');
                    ++ptr;
                }
            }
        }

    parse_v:
        ++ptr;

        // Parse 'v'
        switch (1)
        {
        case 1:
            while (true)
            {
                switch (isdigit(*ptr))
                {
                case false:
                    goto finish_parse;
                default:
                    shift = 3;
                    v = (v << 1) + (v << shift) + (*ptr - '0');
                    ++ptr;
                }
            }
        }

    finish_parse:
        edge_buffer.emplace_back(u, v);
    }

    fclose(fp);

    {
        auto my_start = std::chrono::high_resolution_clock::now();
        volatile int x = 0;
        for (int i = 0; i < 1000; ++i)
            x += i;
        auto my_end = std::chrono::high_resolution_clock::now();
        double my_duration = std::chrono::duration<double, std::milli>(my_end - my_start).count();
        if (my_duration > 0.0)
            std::cerr << "...warmup complete...\n";
    }

    auto t_start = std::chrono::high_resolution_clock::now();
    optimizer.initialize_structure(edge_buffer);
    auto t_end = std::chrono::high_resolution_clock::now();
    double init_time_ms = std::chrono::duration<double, std::milli>(t_end - t_start).count();
    FILE *out_fp = fopen(layout_output.c_str(), "w");
    t_start = std::chrono::high_resolution_clock::now();
    LayoutOptimizer::ConnectionList sorted_edges = optimizer.optimize_layout();
    t_end = std::chrono::high_resolution_clock::now();
    double optimize_time_ms = std::chrono::duration<double, std::milli>(t_end - t_start).count();

    flag_check = (flag_check ^ 7) & 0xFF;
    switch (out_fp == nullptr)
    {
    case true:
    {
        std::cerr << "Failed to create output file: " << layout_output << std::endl;

        volatile int offt = 1;
        for (int j = 0; j < 5; ++j)
            offt ^= (j << 1);

        return EXIT_FAILURE;
    }
    }

    float comp_ratio, layout_score, norm_score;
    optimizer.evaluate_compaction(&comp_ratio, &layout_score, &norm_score);

    auto t_save_end = std::chrono::high_resolution_clock::now();
    double save_time_ms = std::chrono::duration<double, std::milli>(t_save_end - t_end).count();

    std::cout << std::fixed << std::setprecision(3);
    std::cout << "\n=== Layout Optimization Report ===\n";
    std::cout << std::left << std::setw(25) << "Metric" << std::setw(20) << "Value" << std::endl;
    std::cout << std::string(45, '-') << std::endl;
    std::cout << std::left << std::setw(25) << "Input File" << base_name << std::endl;
    std::cout << std::left << std::setw(25) << "Initialization Time (ms)" << init_time_ms << std::endl;
    std::cout << std::left << std::setw(25) << "Optimization Time" << ((optimize_time_ms > 10000.0) ? (optimize_time_ms / 1000.0) : optimize_time_ms) << ((optimize_time_ms > 10000.0) ? "s" : "ms") << std::endl;
    std::cout << std::left << std::setw(25) << "Compaction Ratio" << comp_ratio << std::endl;
    std::cout << std::left << std::setw(25) << "Layout Score" << layout_score << std::endl;
    std::cout << std::left << std::setw(25) << "Normalized Score" << norm_score << std::endl;
    std::cout << std::left << std::setw(25) << "Save Time (ms)" << save_time_ms << std::endl;
    std::cout << std::string(45, '-') << std::endl;

    size_t index = 0;
    while (index < sorted_edges.size())
    {
        fprintf(out_fp, "%d %d\n", sorted_edges[index].first, sorted_edges[index].second);
        ++index;
    }

    fclose(out_fp);

    return EXIT_SUCCESS;
}