#ifndef _UTIL_H
#define _UTIL_H

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

// // SIMD config for triangle counting
// #define SIMD_STATE 4 // 0:none, 2:scalar2x, 4:simd4x
// #define SIMD_MODE 1  // 0:naive, 1:filter

typedef int PackBase;
typedef int PackState;

// Constants for bit packing and vector width
const int PACK_WIDTH = sizeof(PackState) * 8;
const int PACK_SHIFT = __builtin_ctzll(PACK_WIDTH);
const int PACK_MASK = PACK_WIDTH - 1;
const size_t PACK_NODE_POOL_SIZE = 1000000000;
const int CACHE_LINE_SIZE = sysconf(_SC_LEVEL1_DCACHE_LINESIZE); // in bytes

// Graph vertex representation
struct UVertex
{
    int start, deg;
    UVertex() : start(-1), deg(0) {}
    UVertex(int _s, int _d) : start(_s), deg(_d) {}
};

// Graph edge typedef
typedef std::pair<int, int> Edge;
typedef std::vector<Edge> EdgeVector;

// Memory allocation with alignment (required for SIMD)
inline void align_malloc(void **memptr, size_t alignment, size_t size)
{
    int malloc_flag = posix_memalign(memptr, alignment, size);
    if (malloc_flag)
    {
        perror("posix_memalign");
        exit(EXIT_FAILURE);
    }
}

// Graph loading from file
inline EdgeVector load_graph(const std::string &path)
{
    EdgeVector edge_vec;
    FILE *fp = fopen(path.c_str(), "r");
    if (!fp)
    {
        perror(("Failed to open: " + path).c_str());
        exit(EXIT_FAILURE);
    }

    char line[512];
    while (fgets(line, sizeof(line), fp))
    {
        if (line[0] == '#')
            continue;
        int u = 0, v = 0;
        const char *c = line;
        while (isdigit(*c))
            u = (u << 1) + (u << 3) + (*c++ - '0');
        c++;
        while (isdigit(*c))
            v = (v << 1) + (v << 3) + (*c++ - '0');
        edge_vec.emplace_back(u, v);
    }

    fclose(fp);
    return edge_vec;
}

// Edge sorting for graph building
inline bool edge_idpair_cmp(const Edge &a, const Edge &b)
{
    return (a.first == b.first) ? a.second < b.second : a.first < b.first;
}

#endif // _UTIL_H
