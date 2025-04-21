#ifndef GRAPH_DATA_UTILS_HPP
#define GRAPH_DATA_UTILS_HPP

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <string>
#include <utility>
#include <x86intrin.h>
#include <unistd.h>
#include <iostream>
#include <cerrno>
#include <cassert>
#include <cstdint>
#include <algorithm>
#include <sys/time.h>

typedef int PackBase;
typedef int PackState;

// Constants for bit packing and vector width
const int CONST_PACK_WIDTH = sizeof(PackState) * 8;
const int CONST_PACK_SHIFT = __builtin_ctzll(CONST_PACK_WIDTH);
const int CONST_PACK_MASK = CONST_PACK_WIDTH - 1;
const size_t CONST_NODE_POOL_SIZE = 1000000000;
const int CONST_CACHE_LINE_BYTES = sysconf(_SC_LEVEL1_DCACHE_LINESIZE); // in bytes

// Graph vertex representation
struct CompressedVertexInfo
{
    int base_index, degree;
    CompressedVertexInfo() : base_index(-1), degree(0) {}
    CompressedVertexInfo(int _s, int _d) : base_index(_s), degree(_d) {}
};

// Graph edge typedef
typedef std::pair<int, int> VertexEdgePair;
typedef std::vector<VertexEdgePair> EdgeListContainer;

// Memory allocation with alignment (required for SIMD)
inline void allocate_memory_aligned(void **memptr, size_t alignment, size_t size)
{
    int malloc_result = posix_memalign(memptr, alignment, size);
    if (malloc_result)
    {
        perror("posix_memalign");
        exit(EXIT_FAILURE);
    }
}

// Graph loading from file
inline EdgeListContainer read_edge_list_from_file(const std::string &path)
{
    EdgeListContainer edge_vec;
    FILE *file_stream = fopen(path.c_str(), "r");
    if (!file_stream)
    {
        perror(("Failed to open: " + path).c_str());
        exit(EXIT_FAILURE);
    }

    char file_line[512];
    while (fgets(file_line, sizeof(file_line), file_stream))
    {
        if (file_line[0] == '#')
            continue;
        int source_vertex = 0, target_vertex = 0;
        const char *cursor = file_line;
        while (isdigit(*cursor))
            source_vertex = (source_vertex << 1) + (source_vertex << 3) + (*cursor++ - '0');
        cursor++;
        while (isdigit(*cursor))
            target_vertex = (target_vertex << 1) + (target_vertex << 3) + (*cursor++ - '0');
        edge_vec.emplace_back(source_vertex, target_vertex);
    }

    fclose(file_stream);
    return edge_vec;
}

// Edge sorting for graph building
inline bool compare_vertex_edges(const VertexEdgePair &a, const VertexEdgePair &b)
{
    return (a.first == b.first) ? a.second < b.second : a.first < b.first;
}

#endif // _UTIL_H
