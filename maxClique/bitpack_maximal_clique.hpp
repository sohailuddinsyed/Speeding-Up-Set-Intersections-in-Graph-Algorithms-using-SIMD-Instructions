#ifndef _BITPACK_MAXIMAL_CLIQUE_H
#define _BITPACK_MAXIMAL_CLIQUE_H

#include "util.hpp"
#include "set_operation.hpp"

class BPMaximalClique
{
public:
    int v_num, p_num;
    long long e_num;

    BPMaximalClique();
    ~BPMaximalClique();
    void build(const EdgeVector& _e_v);
    int maximal_clique_degen();
    void save_answers(const char* file_path);

private:
    EdgeVector edge_vec;
    std::vector<UVertex> graph;
    std::vector<int> org_deg;
    int *pool_base = NULL;
    PackState *pool_state = NULL;

    int *sets_base = NULL;
    PackState *sets_state = NULL;

    int *pool_mc = NULL, pool_mc_idx = 0, mc_num = 0;
   
    int max_pool_sets_idx = 0, maximum_clique_size = 0;

    void Tomita(std::vector<int>& R, UVertex P, UVertex X);
};

BPMaximalClique::BPMaximalClique()
{
    v_num = 0;
    e_num = 0;
    align_malloc((void**)&pool_base, 32, sizeof(int) * PACK_NODE_POOL_SIZE);
    align_malloc((void**)&pool_state, 32, sizeof(PackState) * PACK_NODE_POOL_SIZE);
    align_malloc((void**)&pool_mc, 32, sizeof(int) * PACK_NODE_POOL_SIZE);
}

BPMaximalClique::~BPMaximalClique()
{
    free(pool_state);
    free(pool_base);
    free(pool_mc);
}

void BPMaximalClique::build(const EdgeVector& _e_v)
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
    p_num = v_num / PACK_WIDTH;
    if (v_num % PACK_WIDTH != 0) p_num++;

    graph.resize(v_num);
    org_deg.reserve(v_num);
    for (int i = 0; i < v_num; ++i) org_deg[i] = 0;

    int cur_packnode_idx = -1;
    int prev_u = -1;
    for (auto& e : edge_vec) {
        int v_base = (e.second >> PACK_SHIFT);
        PackState v_bit = ((PackState)1 << (e.second & PACK_MASK));
        org_deg[e.first]++;
        if (e.first != prev_u) {
            prev_u = e.first;            
            graph[e.first].start = ++cur_packnode_idx;
            graph[e.first].deg++;
            pool_base[cur_packnode_idx] = v_base;
            pool_state[cur_packnode_idx] = v_bit;
        } else {            
            if (pool_base[cur_packnode_idx] == v_base) {
                pool_state[cur_packnode_idx] |= v_bit;
            } else {
                graph[e.first].deg++;
                pool_base[++cur_packnode_idx] = v_base;
                pool_state[cur_packnode_idx] = v_bit;
            }
        }
    }
    cur_packnode_idx++;

    double comp_ratio = (double)cur_packnode_idx / e_num;
    printf("comp_ratio=%d/%lld, %.4f\n", cur_packnode_idx, e_num, comp_ratio);
}

int BPMaximalClique::maximal_clique_degen()
{
    align_malloc((void**)&sets_base, 32, sizeof(int) * PACK_NODE_POOL_SIZE);
    align_malloc((void**)&sets_state, 32, sizeof(PackState) * PACK_NODE_POOL_SIZE);
    max_pool_sets_idx = 0; maximum_clique_size = 0;
    pool_mc_idx = 0; mc_num = 0;
    
    PackState *visited_state = new PackState[p_num];
    memset(visited_state, 0, sizeof(PackState) * p_num);    

    std::vector<int> R;
    R.reserve(2048);
    R.push_back(-1);
    for (int v = 0; v < v_num; ++v) {
        R[0] = v;
        UVertex P(0, 0);
        P.deg = bp_subtract_visited_simd4x(pool_base + graph[v].start,
            pool_state + graph[v].start, graph[v].deg,
            visited_state, sets_base + P.start, sets_state + P.start);

        UVertex X(P.deg, 0);
        X.deg = bp_subtract_unvisited_simd4x(pool_base + graph[v].start,
            pool_state + graph[v].start, graph[v].deg,
            visited_state, sets_base + X.start, sets_state + X.start);

        Tomita(R, P, X);

        int v_base = (v >> PACK_SHIFT);
        PackState v_bit = ((PackState)1 << (v & PACK_MASK));
        visited_state[v_base] |= v_bit;
    }
    R.pop_back();

    printf("max_pool_sets_idx=%d\n", max_pool_sets_idx);
    printf("maximum_clique_size=%d\n", maximum_clique_size);

    free(sets_base);
    free(sets_state);
    delete []visited_state;

    return mc_num;
}

void BPMaximalClique::Tomita(std::vector<int>& R, UVertex P, UVertex X)
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

    int u = (X.deg > 0) ? (sets_base[X.start] << PACK_SHIFT) | __builtin_ctz(sets_state[X.start])
                        : (sets_base[P.start] << PACK_SHIFT) | __builtin_ctz(sets_state[P.start]);

    int N_u_idx = graph[u].start, N_u_end = graph[u].start + graph[u].deg;

    R.push_back(-1);
    for (int i = 0; i < P.deg; ++i) {
        int v_base = sets_base[P.start + i];
        int v_state = sets_state[P.start + i];
        int v_high = (v_base << PACK_SHIFT);
        while (N_u_idx != N_u_end && pool_base[N_u_idx] < v_base) N_u_idx++;
        if (N_u_idx != N_u_end && pool_base[N_u_idx] == v_base)
            v_state &= (~pool_state[N_u_idx]), N_u_idx++;

        while (v_state) {
            int v = (v_high | __builtin_ctz(v_state));
            v_state &= (v_state - 1);

            R.back() = v;
            int newPstart = X.start + X.deg;

            int newPdeg = bp_intersect_filter_simd4x(sets_base + P.start, sets_state + P.start, P.deg,
                pool_base + graph[v].start, pool_state + graph[v].start, graph[v].deg,
                sets_base + newPstart, sets_state + newPstart);

            UVertex newP(newPstart, newPdeg);
            int newXstart = newPstart + newPdeg;

            int newXdeg = bp_intersect_filter_simd4x(sets_base + X.start, sets_state + X.start, X.deg,
                pool_base + graph[v].start, pool_state + graph[v].start, graph[v].deg,
                sets_base + newXstart, sets_state + newXstart);

            UVertex newX(newXstart, newXdeg);

            Tomita(R, newP, newX);

            PackState v_bit = ((PackState)1 << (v & PACK_MASK));
            sets_state[P.start + i] &= (~v_bit);
            X.deg = bp_merge_one(sets_base + X.start, sets_state + X.start, X.deg,
                v_base, v_bit);
        }
    }
    R.pop_back();
}
void BPMaximalClique::save_answers(const char* file_path)
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
