// Further Refactored GraphUtils.hpp (Shuffled and Modularized)
#pragma once

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <utility>
#include <algorithm>
#include <cstring>
#include <cstdlib>
#include <cerrno>
#include <unistd.h>
#include <x86intrin.h>

// SIMD Configuration
#define SIMD_STATE 4 // 0:none, 2:scalar2x, 4:simd4x
#define SIMD_MODE 1  // 0:naive 1: filter

#ifdef SI64
using PackState = long long;
#else
using PackState = int;
#endif

using PackBase = int;
constexpr int PACK_WIDTH = sizeof(PackState) * 8;
constexpr int PACK_SHIFT = __builtin_ctzll(PACK_WIDTH);
constexpr int PACK_MASK = PACK_WIDTH - 1;
constexpr size_t PACK_NODE_POOL_SIZE = 23000000;
const int CACHE_LINE_SIZE = sysconf(_SC_LEVEL1_DCACHE_LINESIZE);

// SIMD Parallelism Levels
constexpr size_t PARA_DEG_M128 = sizeof(__m128i) / sizeof(PackState);
constexpr size_t PARA_DEG_M256 = sizeof(__m256i) / sizeof(PackState);

// Graph Data Structures
struct PackNode {
    PackBase base;
    PackState state;
    PackNode() = default;
    PackNode(PackBase b, PackState s) : base(b), state(s) {}
};

struct PackedVertexSet {
    int start = -1;
    int deg = 0;
    PackedVertexSet() = default;
    PackedVertexSet(int s, int d) : start(s), deg(d) {}
};

struct DVertex {
    int out_start = -1, out_deg = 0;
    int in_start = -1, in_deg = 0;
    DVertex() = default;
};

using Edge = std::pair<int, int>;
using EdgeVector = std::vector<Edge>;

// Memory and Quit Handling
inline void quitWithMessage(const std::string& msg = "[ERROR] Critical failure. Exiting.") {
    std::cerr << msg << std::endl;
    std::exit(EXIT_FAILURE);
}

inline void allocateAlignedMemory(void** memptr, size_t alignment, size_t size) {
    if (posix_memalign(memptr, alignment, size) != 0)
        quitWithMessage("[ERROR] Memory alignment failed.");
}

// File Parsing Helpers
inline std::string getBaseFilename(const std::string& filepath) {
    return filepath.substr(0, filepath.find_last_of('.'));
}

inline int getArgumentIndex(char* flag, int argc, char** argv) {
    for (int i = 1; i < argc; ++i) {
        if (strcmp(flag, argv[i]) == 0) {
            if (i + 1 >= argc) quitWithMessage("[ERROR] No argument provided for " + std::string(flag));
            return i;
        }
    }
    return -1;
}

inline bool compareEdgePairs(const Edge& a, const Edge& b) {
    return (a.first == b.first) ? a.second < b.second : a.first < b.first;
}

// IO Helpers for Graphs
inline EdgeVector loadEdgeList(const std::string& filePath) {
    EdgeVector edges;
    std::ifstream file(filePath);
    if (!file) quitWithMessage("[ERROR] Cannot open graph file: " + filePath);

    std::string line;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;
        std::istringstream iss(line);
        int u, v;
        if (iss >> u >> v) edges.emplace_back(u, v);
    }
    return edges;
}

inline std::vector<int> loadVertexMapping(const std::string& filePath) {
    std::ifstream file(filePath);
    if (!file) quitWithMessage("[ERROR] Cannot open vertex mapping: " + filePath);

    std::vector<int> mapping;
    std::string line;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;
        std::istringstream iss(line);
        int from, to;
        if (iss >> from >> to) {
            if (from >= (int)mapping.size()) mapping.resize(from + 1);
            mapping[from] = to;
        }
    }
    return mapping;
}

inline void saveEdgeList(const std::string& filePath, const EdgeVector& edges) {
    std::ofstream file(filePath);
    if (!file) quitWithMessage("[ERROR] Failed to write edge list to: " + filePath);
    for (const auto& [u, v] : edges)
        file << u << ' ' << v << '\n';
}

inline void saveVertexMapping(const std::string& filePath, const std::vector<int>& mapping) {
    std::ofstream file(filePath);
    if (!file) quitWithMessage("[ERROR] Failed to write mapping to: " + filePath);
    for (size_t i = 0; i < mapping.size(); ++i)
        file << i << ' ' << mapping[i] << '\n';
}
