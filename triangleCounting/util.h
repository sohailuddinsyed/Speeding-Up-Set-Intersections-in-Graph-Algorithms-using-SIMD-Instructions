#ifndef _UTIL_H
#define _UTIL_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <x86intrin.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>
#include <sys/time.h>
#include <ctype.h>

#define PACK_NODE_POOL_SIZE 10000

typedef int PackBase;

// Constants for bit packing and vector width
#define PACK_WIDTH (sizeof(PackState) * 8)
#define PACK_SHIFT (__builtin_ctzll(PACK_WIDTH))
#define PACK_MASK (PACK_WIDTH - 1)
#define CACHE_LINE_SIZE sysconf(_SC_LEVEL1_DCACHE_LINESIZE)

// Graph vertex representation
typedef struct
{
    int start;
    int deg;
} UVertex;

// Graph edge typedef
typedef struct
{
    int first;
    int second;
} Edge;

// Edge list with manual vector-style struct
typedef struct
{
    Edge *data;
    size_t size;
    size_t capacity;
} EdgeVector;

// --- EdgeVector operations (mimicking std::vector) ---

static inline void edge_vector_init(EdgeVector *ev)
{
    ev->size = 0;
    ev->capacity = 1024;
    ev->data = (Edge *)malloc(ev->capacity * sizeof(Edge));
}

static inline void edge_vector_push_back(EdgeVector *ev, Edge e)
{
    if (ev->size >= ev->capacity)
    {
        ev->capacity *= 2;
        ev->data = (Edge *)realloc(ev->data, ev->capacity * sizeof(Edge));
    }
    ev->data[ev->size++] = e;
}

static inline void edge_vector_free(EdgeVector *ev)
{
    free(ev->data);
    ev->data = NULL;
    ev->size = 0;
    ev->capacity = 0;
}

// --- Aligned memory allocation ---
static inline void align_malloc(void **memptr, size_t alignment, size_t size)
{
    int malloc_flag = posix_memalign(memptr, alignment, size);
    if (malloc_flag)
    {
        perror("posix_memalign");
        exit(EXIT_FAILURE);
    }
}

// --- Graph loading ---
static inline EdgeVector load_graph(const char *path)
{
    EdgeVector edge_vec;
    edge_vector_init(&edge_vec);

    FILE *fp = fopen(path, "r");
    if (!fp)
    {
        perror("Failed to open graph file");
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
            u = (u << 1) + (u << 3) + (*c++ - '0'); // u = u*10 + digit
        c++;                                        // skip space or tab
        while (isdigit(*c))
            v = (v << 1) + (v << 3) + (*c++ - '0');
        Edge e = {u, v};
        edge_vector_push_back(&edge_vec, e);
    }

    fclose(fp);
    return edge_vec;
}

// --- Edge comparator for qsort ---
static inline int edge_idpair_cmp(const void *a, const void *b)
{
    const Edge *ea = (const Edge *)a;
    const Edge *eb = (const Edge *)b;
    if (ea->first == eb->first)
        return ea->second - eb->second;
    return ea->first - eb->first;
}

#endif // _UTIL_H