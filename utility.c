#include "utility.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

// Replace system pause for cross-platform pause
void quit()
{
    // Use getchar() to pause for now, or just exit directly
    exit(0);
}

void align_malloc(void **memptr, size_t alignment, size_t size)
{
    int malloc_flag = posix_memalign(memptr, alignment, size);
    if (malloc_flag)
    {
        fprintf(stderr, "posix_memalign: %s\n", strerror(malloc_flag));
        quit();
    }
}

char *extract_filename(const char *full_filename)
{
    const char *dot = strrchr(full_filename, '.');
    size_t len = (dot != NULL) ? (size_t)(dot - full_filename) : strlen(full_filename);
    char *filename = (char *)malloc(len + 1);
    strncpy(filename, full_filename, len);
    filename[len] = '\0';
    return filename;
}

int arg_pos(char *str, int argc, char **argv)
{
    for (int a = 1; a < argc; a++)
    {
        if (!strcmp(str, argv[a]))
        {
            if (a == argc - 1)
            {
                printf("Argument missing for %s\n", str);
                quit();
            }
            return a;
        }
    }
    return -1;
}

EdgeVec load_graph(const char *path)
{
    EdgeVec edge_vec;
    edge_vec.count = 0;
    edge_vec.capacity = 1024;
    edge_vec.data = (Edge *)malloc(sizeof(Edge) * edge_vec.capacity);

    FILE *fp = fopen(path, "r");
    if (fp == NULL)
    {
        fprintf(stderr, "fail to open %s\n", path);
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

        if (edge_vec.count >= edge_vec.capacity)
        {
            edge_vec.capacity *= 2;
            edge_vec.data = (Edge *)realloc(edge_vec.data, sizeof(Edge) * edge_vec.capacity);
        }
        edge_vec.data[edge_vec.count].first = u;
        edge_vec.data[edge_vec.count].second = v;
        edge_vec.count++;
    }
    fclose(fp);
    return edge_vec;
}

VertexOrder load_vertex_order(const char *path)
{
    FILE *fp = fopen(path, "r");
    if (fp == NULL)
    {
        fprintf(stderr, "fail to open %s\n", path);
        quit();
    }

    EdgeVec id_pair = load_graph(path); // same format as graph
    VertexOrder order;
    order.count = 0;
    order.data = (int *)calloc(id_pair.count, sizeof(int));

    for (int i = 0; i < id_pair.count; ++i)
    {
        int u = id_pair.data[i].first;
        int v = id_pair.data[i].second;
        order.data[u] = v;
        if (u >= order.count)
            order.count = u + 1;
    }

    fclose(fp);
    return order;
}

void save_graph(const char *path, const EdgeVec *edge_vec)
{
    FILE *fp = fopen(path, "w");
    if (fp == NULL)
    {
        fprintf(stderr, "fail to create %s\n", path);
        quit();
    }

    for (int i = 0; i < edge_vec->count; ++i)
        fprintf(fp, "%d %d\n", edge_vec->data[i].first, edge_vec->data[i].second);

    fclose(fp);
}

void save_newid(const char *path, const VertexOrder *order)
{
    FILE *fp = fopen(path, "w");
    if (fp == NULL)
    {
        fprintf(stderr, "fail to create %s\n", path);
        quit();
    }

    for (int i = 0; i < order->count; ++i)
        fprintf(fp, "%d %d\n", i, order->data[i]);

    fclose(fp);
}

int edge_idpair_cmp(const void *a, const void *b)
{
    Edge *ea = (Edge *)a;
    Edge *eb = (Edge *)b;
    if (ea->first == eb->first)
        return ea->second - eb->second;
    return ea->first - eb->first;
}
