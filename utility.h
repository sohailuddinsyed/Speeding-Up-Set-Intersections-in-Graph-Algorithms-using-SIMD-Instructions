#ifndef UTIL_H
#define UTIL_H

#include <stddef.h>
#include <stdint.h> // <- Add this

typedef uint32_t PackState; // <- Add this

typedef struct
{
    int first;
    int second;
} Edge;

typedef struct
{
    Edge *data;
    int count;
    int capacity;
} EdgeVec;

typedef struct
{
    int *data;
    int count;
} VertexOrder;

typedef struct
{
    int start;
    int deg;
} UVertex;

void quit();
void align_malloc(void **memptr, size_t alignment, size_t size);
char *extract_filename(const char *full_filename);
int arg_pos(char *str, int argc, char **argv);
EdgeVec load_graph(const char *path);
VertexOrder load_vertex_order(const char *path);
void save_graph(const char *path, const EdgeVec *edge_vec);
void save_newid(const char *path, const VertexOrder *order);
int edge_idpair_cmp(const void *a, const void *b);

#endif
