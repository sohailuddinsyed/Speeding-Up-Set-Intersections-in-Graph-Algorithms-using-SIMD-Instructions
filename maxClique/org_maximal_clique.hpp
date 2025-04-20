#ifndef _ORG_MAXIMAL_CLIQUE_H
#define _ORG_MAXIMAL_CLIQUE_H

#include "util.hpp"
#include "set_operation.hpp"

class OrgMaximalClique
{
public:
    int v_num;
    long long e_num;

    OrgMaximalClique();
    ~OrgMaximalClique();

    void build(const EdgeVector& _e_v);
    int maximal_clique_degen();
    void save_answers(const char* file_path);

private:
    EdgeVector edge_vec;
    std::vector<UVertex> graph;
    int *pool_edges = NULL;
    int *pool_sets = NULL;
    int *pool_mc = NULL, pool_mc_idx = 0, mc_num = 0;
    int *temp_set = NULL;
    int max_pool_sets_idx = 0, maximum_clique_size = 0;

    void Tomita(std::vector<int>& R, UVertex P, UVertex X);
};

OrgMaximalClique::OrgMaximalClique()
{
    v_num = 0;
    e_num = 0;
    align_malloc((void**)&pool_edges, 32, sizeof(int) * PACK_NODE_POOL_SIZE);
    align_malloc((void**)&pool_mc, 32, sizeof(int) * PACK_NODE_POOL_SIZE);
}

OrgMaximalClique::~OrgMaximalClique()
{
    free(pool_edges);
    free(pool_mc);
}

void OrgMaximalClique::build(const EdgeVector& _e_v)
{
    edge_vec.reserve(_e_v.size());
    for (auto& e : _e_v) if (e.first != e.second) edge_vec.push_back(e);

    std::sort(edge_vec.begin(), edge_vec.end(), edge_idpair_cmp);
    edge_vec.erase(std::unique(edge_vec.begin(), edge_vec.end()), edge_vec.end());

    for (auto& e : edge_vec) {
        v_num = std::max(v_num, e.first);
        v_num = std::max(v_num, e.second);
    }
    v_num++;
    e_num = (long long)edge_vec.size();

    graph.resize(v_num);

    int cur_node_idx = 0;
    int prev_u = -1;
    for (auto& e : edge_vec) {
        if (e.first != prev_u) {
            prev_u = e.first;
            graph[e.first].start = cur_node_idx;
        }
        graph[e.first].deg++;
        pool_edges[cur_node_idx++] = e.second;       
    }

    printf("v_num=%d e_num=%lld\n", v_num, e_num);
}

int OrgMaximalClique::maximal_clique_degen()
{
    align_malloc((void**)&pool_sets, 32, sizeof(int) * PACK_NODE_POOL_SIZE);
    align_malloc((void**)&temp_set, 32, sizeof(int) * v_num);
    max_pool_sets_idx = 0; maximum_clique_size = 0;
    pool_mc_idx = 0; mc_num = 0;

    bool *visited = new bool[v_num];
    memset(visited, 0, v_num * sizeof(bool));

    std::vector<int> dorder;
    std::vector<int> deg(v_num);
    int md = 0;
    for (int i = 0; i < v_num; ++i) {
        deg[i] = graph[i].deg;
        md = std::max(md, deg[i]);
    }

    std::vector<int> bin(md + 1);
    for (int i = 0; i <= md; ++i) bin[i] = 0;
    for (int i = 0; i < v_num; ++i) bin[deg[i]]++;

    int start = 0;
    for (int i = 0; i <= md; ++i) {
        int num = bin[i];
        bin[i] = start;
        start += num;
    }

    std::vector<int> vert(v_num), pos(v_num);
    for (int i = 0; i < v_num; ++i) {
        pos[i] = bin[deg[i]];
        vert[pos[i]] = i;
        bin[deg[i]]++;
    }
    for (int i = md; i > 0; --i) bin[i] = bin[i - 1];
    bin[0] = 0;

    int degeneracy = 0;
    for (int i = 0; i < v_num; ++i) {
        int v = vert[i];
        dorder.push_back(v);
        degeneracy = std::max(degeneracy, deg[v]);
        for (int j = 0; j < graph[v].deg; ++j) {
            int u = pool_edges[graph[v].start + j];
            if (deg[u] > deg[v]) {
                int du = deg[u], pu = pos[u];
                int pw = bin[du], w = vert[pw];
                if (u != w) {
                    pos[u] = pw; vert[pu] = w;
                    pos[w] = pu; vert[pw] = u;
                }
                bin[du]++;
                deg[u]--;
            }
        }
    }
    printf("degeneracy=%d\n", degeneracy);

    std::vector<int> R;
    R.reserve(2048);
    R.push_back(-1);
    for (auto v : dorder) {
        R[0] = v;
        UVertex P(0, 0);
        for (int i = 0; i < graph[v].deg; ++i) {
            int u = pool_edges[graph[v].start + i];
            if (!visited[u]) {
                pool_sets[P.start + P.deg] = u;
                P.deg++;
            }
        }
        UVertex X(P.deg, 0);
        for (int i = 0; i < graph[v].deg; ++i) {
            int u = pool_edges[graph[v].start + i];
            if (visited[u]) {
                pool_sets[X.start + X.deg] = u;
                X.deg++;
            }
        }
        Tomita(R, P, X);
        visited[v] = true;
    }
    R.pop_back();

    printf("max_pool_sets_idx=%d\n", max_pool_sets_idx);
    printf("maximum_clique_size=%d\n", maximum_clique_size);

    free(pool_sets);
    free(temp_set);
    delete []visited;

    return mc_num;
}

void OrgMaximalClique::Tomita(std::vector<int>& R, UVertex P, UVertex X)
{
    if (P.deg == 0 && X.deg == 0) {
        memcpy(pool_mc + pool_mc_idx, R.data(), R.size() * sizeof(int));
        pool_mc_idx += R.size();
        pool_mc[pool_mc_idx++] = -1;
        mc_num++;
        max_pool_sets_idx = std::max(max_pool_sets_idx, X.start + X.deg);
        maximum_clique_size = std::max(maximum_clique_size, (int)R.size());          
        return;
    }

    int u = (X.deg > 0) ? pool_sets[X.start] : pool_sets[P.start];
    int *N_u_ptr = pool_edges + graph[u].start;
    int *N_u_end = pool_edges + graph[u].start + graph[u].deg;
    int Pbound = 0; 

    R.push_back(-1);
    for (int i = 0; i < P.deg; ++i) {
        int v = pool_sets[P.start + i];
        while (N_u_ptr != N_u_end && *N_u_ptr < v) N_u_ptr++;
        if (N_u_ptr != N_u_end && *N_u_ptr == v) {
            N_u_ptr++;
            continue;
        } 
        R.back() = v;
        int newPstart = X.start + X.deg;
        int newPdeg = intersect(pool_sets + P.start + Pbound, P.deg - Pbound,
                pool_edges + graph[v].start, graph[v].deg, pool_sets + newPstart);
        UVertex newP(newPstart, newPdeg);      
        int newXstart = newPstart + newPdeg;
        int mergeXdeg = merge(pool_sets + P.start, Pbound,
                pool_sets + X.start, X.deg, temp_set);
        int newXdeg = intersect(temp_set, mergeXdeg,
                pool_edges + graph[v].start, graph[v].deg, pool_sets + newXstart);
        UVertex newX(newXstart, newXdeg);
        Tomita(R, newP, newX);
        for (int j = i; j > Pbound; --j)
            pool_sets[P.start + j] = pool_sets[P.start + j - 1];
        pool_sets[P.start + Pbound] = v;
        Pbound++;
    }
    R.pop_back();
}
void OrgMaximalClique::save_answers(const char* file_path)
{
    FILE *fp = fopen(file_path, "w");
    if (fp == NULL) {
        std::cout << "fail to create " << file_path << std::endl;
        quit();
    }

    for (int i = 0; i < pool_mc_idx; ++i)
        if (pool_mc[i] == -1) fprintf(fp, "\n");
        else fprintf(fp, "%d ", pool_mc[i]);

    fclose(fp);
}

#endif
