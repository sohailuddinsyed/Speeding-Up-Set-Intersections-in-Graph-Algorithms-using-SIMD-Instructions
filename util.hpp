#ifndef _UTIL_H
#define _UTIL_H

#include <cstdio>
#include <iostream>
#include <cerrno>
#include <cassert>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <vector>
#include <string>
#include <algorithm>
#include <x86intrin.h>
#include <unistd.h>
#include <sys/time.h>

#define SIMD_STATE 4 // 0:none, 2:scalar2x, 4:simd4x
#define SIMD_MODE 1  // 0:naive 1: filter

typedef int PackBase;
#ifdef SI64
typedef long long PackState;
#else
typedef int PackState;
#endif

const int PACK_WIDTH = sizeof(PackState) * 8;
const int PACK_SHIFT = __builtin_ctzll(PACK_WIDTH);
const int PACK_MASK = PACK_WIDTH - 1;

const size_t PARA_DEG_M128 = sizeof(__m128i) / sizeof(PackState);
const size_t PARA_DEG_M256 = sizeof(__m256i) / sizeof(PackState);

const size_t PACK_NODE_POOL_SIZE = 1024000000;
const int CACHE_LINE_SIZE = sysconf(_SC_LEVEL1_DCACHE_LINESIZE); // in byte.

struct PackNode
{
    PackBase base;
    PackState state;
    PackNode() {};
    PackNode(PackBase _b, PackState _s) : base(_b), state(_s) {};
};

struct UVertex
{
    int start, deg;
    UVertex() : start(-1), deg(0) {};
    UVertex(int _s, int _d) : start(_s), deg(_d) {};
};

struct DVertex
{
    int out_start, out_deg;
    int in_start, in_deg;
    DVertex() : out_start(-1), out_deg(0), in_start(-1), in_deg(0) {};
};

typedef std::pair<int, int> Edge;
typedef std::vector<std::pair<int, int>> EdgeVector;

inline void quit()
{
    system("pause");
    exit(0);
}

inline void align_malloc(void **memptr, size_t alignment, size_t size)
{
    int malloc_flag = posix_memalign(memptr, alignment, size);
    if (malloc_flag)
    {
        std::cerr << "posix_memalign: " << strerror(malloc_flag) << std::endl;
        quit();
    }
}

inline std::string extract_filename(const std::string full_filename)
{
    int pos = full_filename.find_last_of('.');
    return full_filename.substr(0, pos);
}

inline int arg_pos(char *str, int argc, char **argv)
{
    for (int a = 1; a < argc; a++)
        if (!strcmp(str, argv[a]))
        {
            if (a == argc - 1)
            {
                printf("Argument missing for %s\n", str);
                quit();
            }
            return a;
        }
    return -1;
}

inline EdgeVector load_graph(const std::string path)
{
    EdgeVector edge_vec;
    FILE *fp = fopen(path.c_str(), "r");
    if (fp == NULL)
    {
        std::cout << "fail to open " << path << std::endl;
        quit();
    }

    char line[512];
    while (fgets(line, 512, fp) != NULL)
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
        edge_vec.push_back(std::make_pair(u, v));
    }
    fclose(fp);
    return edge_vec;
}

inline std::vector<int> load_vertex_order(const std::string path)
{
    FILE *fp = fopen(path.c_str(), "r");
    if (fp == NULL)
    {
        std::cout << "fail to open " << path << std::endl;
        quit();
    }

    EdgeVector id_pair;
    char line[512];
    while (fgets(line, 512, fp) != NULL)
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
        id_pair.push_back(std::make_pair(u, v));
    }
    fclose(fp);

    std::vector<int> order(id_pair.size());
    for (auto &p : id_pair)
        order[p.first] = p.second;
    return order;
}

inline void save_graph(const std::string path, const EdgeVector &edge_vec)
{
    FILE *fp = fopen(path.c_str(), "w");
    if (fp == NULL)
    {
        std::cout << "fail to create " << path << std::endl;
        quit();
    }
    for (auto &e : edge_vec)
        fprintf(fp, "%d %d\n", e.first, e.second);
    fclose(fp);
}

inline void save_newid(const std::string path, std::vector<int> org2newid)
{
    FILE *fp = fopen(path.c_str(), "w");
    if (fp == NULL)
    {
        std::cout << "fail to create " << path << std::endl;
        quit();
    }
    for (int i = 0; i < (int)org2newid.size(); ++i)
        fprintf(fp, "%d %d\n", i, org2newid[i]);
    fclose(fp);
}

inline bool edge_idpair_cmp(const Edge &a, const Edge &b)
{
    return (a.first == b.first) ? a.second < b.second : a.first < b.first;
}

#endif // _UTIL_H
