// #ifndef _PORDER_H
// #define _PORDER_H

// #include "../util.hpp"

// #include "metis.h"

// struct NbrNode
// {
//     int vid;
//     bool type; // 0 for out-neighbors, 1 for in-neighbors.
//     NbrNode(int _v, int _t) : vid(_v), type(_t) {};
// };

// struct NodeWithGain
// {
//     int u;
//     double gain;
//     NodeWithGain() : u(0), gain(0.0) {};
//     NodeWithGain(int _u, double _g) : u(_u), gain(_u) {};
//     bool operator<(const NodeWithGain &p) const { return gain > p.gain; };
// };

// class POrder
// {
// public:
//     int v_num;
//     long long e_num;
//     int p_num;                  // pack num: ceil(v_num/PACK_WIDTH).
//     std::vector<int> org2newid; // org_id: i --> new_id: org2newid[i].
//     void load_org_graph(EdgeVector _e_v);
//     EdgeVector hybrid_bfsdeg();
//     EdgeVector greedy_naive();
//     EdgeVector greedy_mheap();
//     EdgeVector mloggapa_order();  // MLOGGAPA order using Recursive Graph Bisection algorithm
//     EdgeVector metis_order();     // METIS partitioning algorithm
//     EdgeVector slashburn_order(); // SlashBurn algorithm
//     EdgeVector bfsr_order();      // Recursive BFS order algorithm
//     EdgeVector dfs_order();       // DFS order algorithm
//     void set_alpha_by_deg();
//     void set_alpha(double *_a_out, double *_a_in);

//     int leaf_node_count();
//     std::vector<int> select_bignode(double deg_ratio);
//     double comp_ratio();

//     POrder();
//     ~POrder();

// private:
//     EdgeVector edge_vec;
//     std::vector<DVertex> graph;
//     std::vector<int> outedge, inedge;
//     std::vector<NbrNode> nbr;

//     int *new_id;
//     double *alpha_out, *alpha_in;

//     void build(); // build graph's adjacent lists by edge_vec/new_id.
//     void sort_nbr();
//     void deg_order();
//     void rcm_order();
//     void bfs_order();
//     void deg_desc_order();

//     void dfs(int u, int *nid, int &cur_idx);
//     // for Recursive Graph Bisection Order
//     void graph_bisection(NodeWithGain *nodes, int *set_label, double *gain,
//                          int *q_l_nodes, int *q_r_nodes, int tot_num, int level, int &cur_label);
//     void graph_bisection2(NodeWithGain *nodes, int *set_label, double *gain,
//                           int tot_num, int &cur_label);
//     void bfsr_bisection(int *nodes, int tot_num, int *que, int *visited, int &vis_label);
// };

// #endif
#ifndef _PORDER_H
#define _PORDER_H
#include <queue>
#include <unordered_set>
#include <unordered_map>
#include <stack>
#include <cmath>
// #include "min_hash.hpp"
#include "modified_heap.hpp"
#include "linkedlist_heap.hpp"
#include "doublelinked_list.hpp"
#include "../util.hpp"
#include "metis.h"

struct NbrNode
{
    int vid;
    bool type; // 0 for out-neighbors, 1 for in-neighbors.
    NbrNode(int _v, int _t) : vid(_v), type(_t) {};
};

struct NodeWithGain
{
    int u;
    double gain;
    NodeWithGain() : u(0), gain(0.0) {};
    NodeWithGain(int _u, double _g) : u(_u), gain(_u) {};
    bool operator<(const NodeWithGain &p) const { return gain > p.gain; };
};

class POrder
{
public:
    int v_num;
    long long e_num;
    int p_num;
    std::vector<int> org2newid;

    POrder()
    {
        v_num = 0;
        e_num = 0;
        p_num = 0;
        new_id = NULL;
        alpha_out = NULL;
        alpha_in = NULL;
    }

    ~POrder()
    {
        if (new_id != NULL)
            delete[] new_id;
        if (alpha_out != NULL)
            delete[] alpha_out;
        if (alpha_in != NULL)
            delete[] alpha_in;
    }

    void load_org_graph(EdgeVector _e_v)
    {
        this->edge_vec = _e_v;

        e_num = (long long)edge_vec.size();
        v_num = 0;
        for (const auto &e : edge_vec)
        {
            v_num = std::max(v_num, e.first);
            v_num = std::max(v_num, e.second);
        }
        v_num++;
        p_num = v_num / PACK_WIDTH;
        if (v_num % PACK_WIDTH != 0)
            p_num++;

        new_id = new int[v_num];
        alpha_out = new double[v_num];
        alpha_in = new double[v_num];
        org2newid.resize(v_num);
        for (int i = 0; i < v_num; ++i)
            org2newid[i] = i;
        graph.resize(v_num);
        outedge.resize(e_num);
        inedge.resize(e_num);

        build();
    }

    EdgeVector hybrid_bfsdeg()
    {
        int *bfs_idx = new int[v_num];
        memset(bfs_idx, -1, sizeof(int) * v_num);
        int cur_idx = 0;
        std::queue<int> que;
        for (int i = 0; i < v_num; ++i)
            if (bfs_idx[i] == -1)
            {
                que.push(i);
                bfs_idx[i] = cur_idx++;
                while (!que.empty())
                {
                    int u = que.front();
                    que.pop();
                    for (int j = 0; j < graph[u].out_deg; ++j)
                    {
                        int v = outedge[graph[u].out_start + j];
                        if (bfs_idx[v] == -1)
                        {
                            bfs_idx[v] = cur_idx++;
                            que.push(v);
                        }
                    }
                }
            }

        memset(new_id, -1, sizeof(int) * v_num);
        std::vector<int> vertex_temp;
        for (int i = 0; i < v_num; ++i)
            vertex_temp.push_back(i);
        sort(vertex_temp.begin(), vertex_temp.end(),
             [&](const int &a, const int &b) -> bool
             {
                 int deg_a = graph[a].out_deg + graph[a].in_deg;
                 int deg_b = graph[b].out_deg + graph[b].in_deg;
                 if (deg_a == deg_b)
                     return bfs_idx[a] < bfs_idx[b];
                 return deg_a > deg_b;
             });

        delete[] bfs_idx;

        cur_idx = 0;
        for (auto u : vertex_temp)
            new_id[u] = cur_idx++;

        // update org2newid.
        for (auto &idx : org2newid)
            idx = new_id[idx];

        // update edge_vec.
        for (auto &e : edge_vec)
        {
            e.first = new_id[e.first];
            e.second = new_id[e.second];
        }
        return edge_vec;
    }

    EdgeVector greedy_naive()
    {
        rcm_order();
        build();

        set_alpha_by_deg();
        sort_nbr();

        memset(new_id, -1, sizeof(int) * v_num);

        int cur_idx = 0;
        for (auto &nv : nbr)
        {
            if (nv.type)
            { // in-neighbors:
                std::sort(inedge.begin() + graph[nv.vid].in_start,
                          inedge.begin() + graph[nv.vid].in_start + graph[nv.vid].in_deg,
                          [&](const int &a, const int &b) -> bool
                          {
                              return alpha_out[a] > alpha_out[b];
                          });

                for (int i = 0; i < graph[nv.vid].in_deg; ++i)
                {
                    int u = inedge[graph[nv.vid].in_start + i];
                    if (new_id[u] == -1)
                        new_id[u] = cur_idx++;
                }
            }
            else
            { // out-neighbors:
                std::sort(outedge.begin() + graph[nv.vid].out_start,
                          outedge.begin() + graph[nv.vid].out_start + graph[nv.vid].out_deg,
                          [&](const int &a, const int &b) -> bool
                          {
                              return alpha_in[a] > alpha_in[b];
                          });

                for (int i = 0; i < graph[nv.vid].out_deg; ++i)
                {
                    int u = outedge[graph[nv.vid].out_start + i];
                    if (new_id[u] == -1)
                        new_id[u] = cur_idx++;
                }
            }
        }

        if (cur_idx != v_num)
        {
            for (int i = 0; i < v_num; ++i)
                if (new_id[i] == -1)
                    new_id[i] = cur_idx++;
        }

        // update org2newid.
        for (auto &idx : org2newid)
            idx = new_id[idx];

        // update edge_vec.
        for (auto &e : edge_vec)
        {
            e.first = new_id[e.first];
            e.second = new_id[e.second];
        }

        return edge_vec;
    }

    EdgeVector greedy_mheap()
    {
        // deg_order();
        // build();

        memset(new_id, -1, sizeof(int) * v_num);

        // set_alpha_by_deg();

        std::vector<int> vertices;
        vertices.resize(v_num);
        for (int i = 0; i < v_num; ++i)
            vertices[i] = i;

        std::vector<int> deg;
        deg.resize(v_num);
        for (int i = 0; i < v_num; ++i)
            deg[i] = graph[i].out_deg + graph[i].in_deg;
        std::sort(vertices.begin(), vertices.end(),
                  [&](const int &a, const int &b) -> bool
                  {
                      if (deg[a] == deg[b])
                          return a < b;
                      return deg[a] > deg[b];
                  });

        DoubleLinkedList dll(v_num, v_num);
        for (auto u : vertices)
            dll.add(u);

        // ModifiedHeap node_heap(v_num);
        LinkedListHeap node_heap(v_num);

        int *out_nbr_labels, *in_nbr_labels;
        align_malloc((void **)&out_nbr_labels, 32, sizeof(int) * v_num);
        align_malloc((void **)&in_nbr_labels, 32, sizeof(int) * v_num);
        memset(out_nbr_labels, -1, sizeof(int) * v_num);
        memset(in_nbr_labels, -1, sizeof(int) * v_num);

        // const int huge_vertex = sqrt((double)v_num);
        const int window_size = PACK_WIDTH;
        // const int window_size = 8;
        int cur_v_idx = 0;
        int cur_p_idx = -1;
        while (cur_v_idx < v_num)
        {
            int u;
            if (cur_v_idx % window_size == 0)
            {
                u = dll.pop_head();
                node_heap.reset();
                node_heap.del(u);
                cur_p_idx++;
            }
            else
            {
                u = node_heap.pop();
                dll.del(u);
            }

            new_id[u] = cur_v_idx++;

            // if (cur_v_idx % PACK_WIDTH == 0) continue;

            for (int i = 0; i < graph[u].in_deg; ++i)
            {
                int v = inedge[graph[u].in_start + i];
                if (out_nbr_labels[v] == cur_p_idx)
                    continue;
                // if (graph[v].out_deg > huge_vertex) continue;
                out_nbr_labels[v] = cur_p_idx;
                for (int j = 0; j < graph[v].out_deg; ++j)
                {
                    int w = outedge[graph[v].out_start + j];
                    // if (node_heap.in_heap(w)) node_heap.inc(w, alpha_out[v]);
                    if (node_heap.in_heap(w))
                        node_heap.inc(w);
                }
            }
            for (int i = 0; i < graph[u].out_deg; ++i)
            {
                int v = outedge[graph[u].out_start + i];
                if (in_nbr_labels[v] == cur_p_idx)
                    continue;
                // if (graph[v].in_deg > huge_vertex) continue;
                in_nbr_labels[v] = cur_p_idx;
                for (int j = 0; j < graph[v].in_deg; ++j)
                {
                    int w = inedge[graph[v].in_start + j];
                    // if (node_heap.in_heap(w)) node_heap.inc(w, alpha_in[v]);
                    if (node_heap.in_heap(w))
                        node_heap.inc(w);
                }
            }

            // if (cur_v_idx % 20000 == 0) printf("%d nodes done\n", cur_v_idx);
        }

        // update org2newid.
        for (auto &idx : org2newid)
            idx = new_id[idx];

        // update edge_vec.
        for (auto &e : edge_vec)
        {
            e.first = new_id[e.first];
            e.second = new_id[e.second];
        }

        free(out_nbr_labels);
        free(in_nbr_labels);

        return edge_vec;
    }

    EdgeVector mloggapa_order()
    {
        // implementation
    }

    EdgeVector metis_order()
    {
        // implementation
    }

    EdgeVector slashburn_order()
    {
        // implementation
    }

    EdgeVector bfsr_order()
    {
        // implementation
    }

    EdgeVector dfs_order()
    {
        // implementation
    }

    void set_alpha_by_deg()
    {
        double sum = 0.0;
        for (int i = 0; i < v_num; ++i)
        {
            alpha_out[i] = sqrt(graph[i].out_deg);
            alpha_in[i] = sqrt(graph[i].in_deg);
            // alpha_out[i] = 1.0;
            // alpha_in[i] = 1.0;
            sum += alpha_out[i] + alpha_in[i];
        }
        for (int i = 0; i < v_num; ++i)
        {
            alpha_out[i] /= sum;
            alpha_in[i] /= sum;
        }
    }

    void set_alpha(double *_a_out, double *_a_in)
    {
        double sum = 0.0;
        for (int i = 0; i < v_num; ++i)
        {
            alpha_out[i] = _a_out[i];
            alpha_in[i] = _a_in[i];
            sum += alpha_out[i] + alpha_in[i];
        }
        for (int i = 0; i < v_num; ++i)
        {
            alpha_out[i] /= sum;
            alpha_in[i] /= sum;
        }
    }

    int leaf_node_count()
    {
        int res = 0;
        for (int i = 0; i < v_num; ++i)
            if (graph[i].out_deg == 1 || graph[i].in_deg == 1)
                res++;

        printf("leaf_node_ratio=%.3f%%(%d/%d)\n", res * 100.0 / v_num, res, v_num);
        return res;
    }

    std::vector<int> select_bignode(double deg_ratio)
    {
        long long threshold = e_num * deg_ratio * 2;
        // sort vertices by deg.
        std::vector<int> vertices;
        vertices.resize(v_num);
        for (int i = 0; i < v_num; ++i)
            vertices[i] = i;
        std::sort(vertices.begin(), vertices.end(),
                  [&](const int &a, const int &b) -> bool
                  {
                      return graph[a].out_deg + graph[a].in_deg >
                             graph[b].out_deg + graph[b].in_deg;
                  });
        long long sum_deg = 0;
        int big_node_cnt = 0;
        for (auto u : vertices)
        {
            sum_deg += graph[u].out_deg + graph[u].in_deg;
            big_node_cnt++;
            if (sum_deg >= threshold)
                break;
        }
        vertices.resize(big_node_cnt); // only keep bignodes.
        double big_node_ratio = (double)big_node_cnt / v_num;
        double avg_big_node_degree = (double)sum_deg / big_node_cnt;
        double big_node_degree_variance = 0.0;
        for (auto u : vertices)
        {
            double deg_diff = graph[u].out_deg + graph[u].in_deg -
                              avg_big_node_degree;
            big_node_degree_variance += deg_diff * deg_diff;
        }
        big_node_degree_variance /= big_node_cnt;

        printf("big_node: ratio=%.3f(%d/%d), ", big_node_ratio,
               big_node_cnt, v_num);
        printf("avg_degree=%.2f, degree_variance=%.2f, ",
               avg_big_node_degree, big_node_degree_variance);
        printf("max_deg=(%d,%d), min_deg=(%d,%d)\n",
               graph[vertices.front()].out_deg, graph[vertices.front()].in_deg,
               graph[vertices.back()].out_deg, graph[vertices.back()].in_deg);

        return vertices;
    }

    double comp_ratio()
    {
        build();
        set_alpha_by_deg();
        int packed_outedge_num = 0, packed_inedge_num = 0;
        double score = 0;
        double norm_score = 0.0;
        double sum_sqrt_deg = 0.0;
        for (int i = 0; i < v_num; ++i)
        {
            int pn = 0;
            int pre_base = -1;
            for (int j = 0; j < graph[i].out_deg; ++j)
            {
                int u = outedge[graph[i].out_start + j];
                int cur_base = (u >> PACK_SHIFT);
                if (cur_base != pre_base)
                {
                    pre_base = cur_base;
                    pn++;
                }
            }
            packed_outedge_num += pn;
            // score += pn * alpha_out[i];
            double sqrt_deg = sqrt(graph[i].out_deg);
            sum_sqrt_deg += sqrt_deg;
            score += pn * sqrt_deg;
            norm_score += pn;

            pn = 0;
            pre_base = -1;
            for (int j = 0; j < graph[i].in_deg; ++j)
            {
                int u = inedge[graph[i].in_start + j];
                int cur_base = (u >> PACK_SHIFT);
                if (cur_base != pre_base)
                {
                    pre_base = cur_base;
                    pn++;
                }
            }
            packed_inedge_num += pn;
            // score += pn * alpha_in[i];
            sqrt_deg = sqrt(graph[i].in_deg);
            sum_sqrt_deg += sqrt_deg;
            score += pn * sqrt_deg;
            norm_score += pn;
        }

        double outedge_comp_ratio = (double)packed_outedge_num / e_num;
        double inedge_comp_ratio = (double)packed_inedge_num / e_num;
        double comp_ratio = (outedge_comp_ratio + inedge_comp_ratio) / 2.0;
        norm_score /= v_num * 2.0;
        score /= sum_sqrt_deg;
        printf("comp_ratio=%.4f(%.4f/%.4f) score=%.3f norm=%.3f\n", comp_ratio,
               outedge_comp_ratio, inedge_comp_ratio, score, norm_score);

        double org_space_cost = (outedge.size() + inedge.size() + v_num * 4) * 4.0 / 1024 / 1024;
        double bp_space_cost = (packed_outedge_num * 1.5 + packed_inedge_num * 1.5 + v_num * 4) * 4.0 / 1024 / 1024;
        printf("org_space_cost=%.2fMB bp_space_cost=%.2fMB\n", org_space_cost, bp_space_cost);
        return comp_ratio;
    }

private:
    EdgeVector edge_vec;
    std::vector<DVertex> graph;
    std::vector<int> outedge, inedge;
    std::vector<NbrNode> nbr;

    int *new_id;
    double *alpha_out, *alpha_in;

    void build()
    {
        std::sort(edge_vec.begin(), edge_vec.end(), edge_idpair_cmp);
        for (auto &dv : graph)
        {
            dv.out_deg = 0;
            dv.in_deg = 0;
        }
        for (const auto &e : edge_vec)
        {
            graph[e.first].out_deg++;
            graph[e.second].in_deg++;
        }
        graph[0].out_start = 0;
        graph[0].in_start = 0;
        for (int i = 1; i < v_num; ++i)
        {
            graph[i].out_start = graph[i - 1].out_start + graph[i - 1].out_deg;
            graph[i].in_start = graph[i - 1].in_start + graph[i - 1].in_deg;
        }

        for (size_t i = 0; i < edge_vec.size(); ++i)
            outedge[i] = edge_vec[i].second;
        std::vector<int> inpos(v_num);
        for (int i = 0; i < v_num; ++i)
            inpos[i] = graph[i].in_start;
        for (const auto &e : edge_vec)
        {
            inedge[inpos[e.second]] = e.first;
            inpos[e.second]++;
        }
    }

    void sort_nbr()
    {
        nbr.reserve(v_num * 2);
        for (int i = 0; i < v_num; ++i)
        {
            nbr.push_back(NbrNode(i, 0));
            nbr.push_back(NbrNode(i, 1));
        }

        std::sort(nbr.begin(), nbr.end(),
                  [&](const NbrNode &a, const NbrNode &b) -> bool
                  {
                      double wa = (a.type ? alpha_in[a.vid] : alpha_out[a.vid]);
                      double wb = (b.type ? alpha_in[b.vid] : alpha_out[b.vid]);
                      if (wa == wb)
                      {
                          if (a.vid == b.vid)
                              return a.type < b.type;
                          return a.vid < b.vid;
                      }
                      return wa > wb;
                  });
    }

    void deg_order()
    {
        memset(new_id, -1, sizeof(int) * v_num);
        std::vector<int> vertex_temp;
        for (int i = 0; i < v_num; ++i)
            vertex_temp.push_back(i);
        sort(vertex_temp.begin(), vertex_temp.end(),
             [&](const int &a, const int &b) -> bool
             {
                 int deg_a = graph[a].out_deg + graph[a].in_deg;
                 int deg_b = graph[b].out_deg + graph[b].in_deg;
                 if (deg_a == deg_b)
                     return a < b;
                 return deg_a < deg_b;
             });

        int cur_idx = 0;
        for (auto u : vertex_temp)
            new_id[u] = cur_idx++;

        // update org2newid.
        for (auto &idx : org2newid)
            idx = new_id[idx];

        // update edge_vec.
        for (auto &e : edge_vec)
        {
            e.first = new_id[e.first];
            e.second = new_id[e.second];
        }
    }

    void rcm_order()
    {
        memset(new_id, -1, sizeof(int) * v_num);
        std::vector<int> vertex_temp;
        for (int i = 0; i < v_num; ++i)
            vertex_temp.push_back(i);
        sort(vertex_temp.begin(), vertex_temp.end(),
             [&](const int &a, const int &b) -> bool
             {
                 return (graph[a].out_deg + graph[a].in_deg <
                         graph[b].out_deg + graph[b].in_deg);
             });

        std::queue<int> que;
        std::vector<int> tmp;
        int cur_idx = v_num - 1;
        for (auto s : vertex_temp)
        {
            if (new_id[s] != -1)
                continue;
            que.push(s);
            new_id[s] = cur_idx--;
            while (!que.empty())
            {
                int u = que.front();
                que.pop();
                tmp.clear();
                for (int i = 0; i < graph[u].out_deg; ++i)
                    tmp.push_back(outedge[graph[u].out_start + i]);
                sort(tmp.begin(), tmp.end(),
                     [&](const int &a, const int &b) -> bool
                     {
                         return (graph[a].out_deg + graph[a].in_deg <
                                 graph[b].out_deg + graph[b].in_deg);
                     });
                for (auto v : tmp)
                {
                    if (new_id[v] == -1)
                    {
                        que.push(v);
                        new_id[v] = cur_idx--;
                    }
                }
            }
        }

        // update org2newid.
        for (auto &idx : org2newid)
            idx = new_id[idx];

        // update edge_vec.
        for (auto &e : edge_vec)
        {
            e.first = new_id[e.first];
            e.second = new_id[e.second];
        }
    }

    void bfs_order()
    {
        memset(new_id, -1, sizeof(int) * v_num);
        std::vector<int> vertex_temp;
        for (int i = 0; i < v_num; ++i)
            vertex_temp.push_back(i);
        sort(vertex_temp.begin(), vertex_temp.end(),
             [&](const int &a, const int &b) -> bool
             {
                 if (graph[a].out_deg == graph[b].out_deg)
                     return a < b;
                 return graph[a].out_deg > graph[b].out_deg;
             });

        // bfs:
        std::queue<int> que;
        int cur_idx = 0;
        for (auto s : vertex_temp)
        {
            if (new_id[s] != -1)
                continue;
            new_id[s] = cur_idx++;
            que.push(s);
            while (!que.empty())
            {
                int u = que.front();
                que.pop();
                for (int i = 0; i < graph[u].out_deg; ++i)
                {
                    int v = outedge[graph[u].out_start + i];
                    if (new_id[v] == -1)
                    {
                        new_id[v] = cur_idx++;
                        que.push(v);
                    }
                }
            }
        }

        // update org2newid.
        for (auto &idx : org2newid)
            idx = new_id[idx];

        // update edge_vec.
        for (auto &e : edge_vec)
        {
            e.first = new_id[e.first];
            e.second = new_id[e.second];
        }
    }

    void deg_desc_order()
    {
        memset(new_id, -1, sizeof(int) * v_num);
        std::vector<int> vertex_temp;
        for (int i = 0; i < v_num; ++i)
            vertex_temp.push_back(i);
        sort(vertex_temp.begin(), vertex_temp.end(),
             [&](const int &a, const int &b) -> bool
             {
                 int deg_a = graph[a].out_deg + graph[a].in_deg;
                 int deg_b = graph[b].out_deg + graph[b].in_deg;
                 if (deg_a == deg_b)
                     return a < b;
                 return deg_a > deg_b;
             });

        int cur_idx = 0;
        for (auto u : vertex_temp)
            new_id[u] = cur_idx++;

        // update org2newid.
        for (auto &idx : org2newid)
            idx = new_id[idx];

        // update edge_vec.
        for (auto &e : edge_vec)
        {
            e.first = new_id[e.first];
            e.second = new_id[e.second];
        }
    }

    void dfs(int u, int *nid, int &cur_idx)
    {
        nid[u] = cur_idx++;
        for (int i = 0; i < graph[u].out_deg; ++i)
        {
            int v = outedge[graph[u].out_start + i];
            if (nid[v] == -1)
                dfs(v, nid, cur_idx);
        }
    }

    void graph_bisection(NodeWithGain *nodes, int *set_label, double *gain,
                         int *q_l_nodes, int *q_r_nodes, int tot_num, int level, int &cur_label)
    {
        if (level > 8)
            printf("level=%d, tot_num=%d\n", level, tot_num);
        if (level == 0)
            return;

        NodeWithGain *left_part = nodes;
        int left_num = tot_num / 2, left_label = ++cur_label;
        NodeWithGain *right_part = nodes + left_num;
        int right_num = tot_num - left_num, right_label = ++cur_label;

        for (int i = 0; i < left_num; ++i)
            set_label[left_part[i].u] = left_label;
        for (int i = 0; i < right_num; ++i)
            set_label[right_part[i].u] = right_label;

        std::unordered_set<int> query_nodes_out, query_nodes_in;
        for (int i = 0; i < tot_num; ++i)
        {
            int u = nodes[i].u;
            for (int j = 0; j < graph[u].out_deg; ++j)
            {
                int v = outedge[graph[u].out_start + j];
                query_nodes_in.insert(v);
            }
            for (int j = 0; j < graph[u].in_deg; ++j)
            {
                int v = inedge[graph[u].in_start + j];
                query_nodes_out.insert(v);
            }
        }

        auto part_cost_func = [](const int n_1, const int q_1,
                                 const int n_2, const int q_2) -> double
        {
            return q_1 * log2(n_1 / (q_1 + 1.0)) + q_2 * log2(n_2 / (q_2 + 1.0));
        };

        for (int iter_time = 0; iter_time < 20; ++iter_time)
        {
            for (int i = 0; i < left_num; ++i)
                gain[left_part[i].u] = 0.0;
            for (int i = 0; i < right_num; ++i)
                gain[right_part[i].u] = 0.0;

            // enumerate query nodes:
            for (auto u : query_nodes_out)
            {                                 // out edges:
                int deg_q_l = 0, deg_q_r = 0; // count deg_q
                for (int i = 0; i < graph[u].out_deg; ++i)
                {
                    int v = outedge[graph[u].out_start + i];
                    if (set_label[v] == left_label)
                        q_l_nodes[deg_q_l++] = v;
                    else if (set_label[v] == right_label)
                        q_r_nodes[deg_q_r++] = v;
                }
                if (deg_q_l > 0)
                {
                    double move_cost =
                        part_cost_func(left_num, deg_q_l, right_num, deg_q_r) -
                        part_cost_func(left_num, deg_q_l - 1, right_num, deg_q_r + 1);
                    for (int i = 0; i < deg_q_l; ++i)
                        gain[q_l_nodes[i]] += move_cost;
                }
                if (deg_q_r > 0)
                {
                    double move_cost =
                        part_cost_func(left_num, deg_q_l, right_num, deg_q_r) -
                        part_cost_func(left_num, deg_q_l + 1, right_num, deg_q_r - 1);
                    for (int i = 0; i < deg_q_r; ++i)
                        gain[q_r_nodes[i]] += move_cost;
                }
            }
            for (auto u : query_nodes_in)
            {                                 // in edges:
                int deg_q_l = 0, deg_q_r = 0; // count deg_q
                for (int i = 0; i < graph[u].in_deg; ++i)
                {
                    int v = inedge[graph[u].in_start + i];
                    if (set_label[v] == left_label)
                        q_l_nodes[deg_q_l++] = v;
                    else if (set_label[v] == right_label)
                        q_r_nodes[deg_q_r++] = v;
                }
                if (deg_q_l > 0)
                {
                    double move_cost =
                        part_cost_func(left_num, deg_q_l, right_num, deg_q_r) -
                        part_cost_func(left_num, deg_q_l - 1, right_num, deg_q_r + 1);
                    for (int i = 0; i < deg_q_l; ++i)
                        gain[q_l_nodes[i]] += move_cost;
                }
                if (deg_q_r > 0)
                {
                    double move_cost =
                        part_cost_func(left_num, deg_q_l, right_num, deg_q_r) -
                        part_cost_func(left_num, deg_q_l + 1, right_num, deg_q_r - 1);
                    for (int i = 0; i < deg_q_r; ++i)
                        gain[q_r_nodes[i]] += move_cost;
                }
            }

            // collect gains:
            for (int i = 0; i < left_num; ++i)
                left_part[i].gain = gain[left_part[i].u];
            for (int i = 0; i < right_num; ++i)
                right_part[i].gain = gain[right_part[i].u];
            std::sort(left_part, left_part + left_num);
            std::sort(right_part, right_part + right_num);
            bool is_converged = true;
            for (int i = 0; i < std::min(left_num, right_num); ++i)
                if (left_part[i].gain + right_part[i].gain > 0)
                {
                    std::swap(left_part[i], right_part[i]);
                    is_converged = false;
                }
                else
                    break;
            if (is_converged)
                break;
        }

        graph_bisection(left_part, set_label, gain,
                        q_l_nodes, q_r_nodes, left_num, level - 1, cur_label);
        graph_bisection(right_part, set_label, gain,
                        q_l_nodes, q_r_nodes, right_num, level - 1, cur_label);
    }

    void graph_bisection2(NodeWithGain *nodes, int *set_label, double *gain,
                          int tot_num, int &cur_label)
    {
        // if (level > 8) printf("here level=%d, tot_num=%d\n", level, tot_num);
        // if (level == 0) return;
        if (tot_num < 32)
            return;

        NodeWithGain *left_part = nodes;
        int left_num = tot_num / 2, left_label = ++cur_label;
        NodeWithGain *right_part = nodes + left_num;
        int right_num = tot_num - left_num, right_label = ++cur_label;

        for (int i = 0; i < left_num; ++i)
            set_label[left_part[i].u] = left_label;
        for (int i = 0; i < right_num; ++i)
            set_label[right_part[i].u] = right_label;

        std::unordered_map<int, std::vector<int>> q_nodes_out_left, q_nodes_out_right;
        std::unordered_map<int, std::vector<int>> q_nodes_in_left, q_nodes_in_right;
        for (int i = 0; i < left_num; ++i)
        {
            int u = left_part[i].u;
            for (int j = 0; j < graph[u].out_deg; ++j)
            {
                int v = outedge[graph[u].out_start + j];
                auto pos = q_nodes_in_left.find(v);
                if (pos == q_nodes_in_left.end())
                {
                    std::vector<int> tmp;
                    tmp.push_back(u);
                    q_nodes_in_left[v] = tmp;
                }
                else
                {
                    (pos->second).push_back(u);
                }
            }
            for (int j = 0; j < graph[u].in_deg; ++j)
            {
                int v = inedge[graph[u].in_start + j];
                auto pos = q_nodes_out_left.find(v);
                if (pos == q_nodes_out_left.end())
                {
                    std::vector<int> tmp;
                    tmp.push_back(u);
                    q_nodes_out_left[v] = tmp;
                }
                else
                {
                    (pos->second).push_back(u);
                }
            }
        }
        for (int i = 0; i < right_num; ++i)
        {
            int u = right_part[i].u;
            for (int j = 0; j < graph[u].out_deg; ++j)
            {
                int v = outedge[graph[u].out_start + j];
                auto pos = q_nodes_in_right.find(v);
                if (pos == q_nodes_in_right.end())
                {
                    std::vector<int> tmp;
                    tmp.push_back(u);
                    q_nodes_in_right[v] = tmp;
                }
                else
                {
                    (pos->second).push_back(u);
                }
            }
            for (int j = 0; j < graph[u].in_deg; ++j)
            {
                int v = inedge[graph[u].in_start + j];
                auto pos = q_nodes_out_right.find(v);
                if (pos == q_nodes_out_right.end())
                {
                    std::vector<int> tmp;
                    tmp.push_back(u);
                    q_nodes_out_right[v] = tmp;
                }
                else
                {
                    (pos->second).push_back(u);
                }
            }
        }

        auto part_cost_func = [](const int n_1, const int q_1,
                                 const int n_2, const int q_2) -> double
        {
            return q_1 * log2(n_1 / (q_1 + 1.0)) + q_2 * log2(n_2 / (q_2 + 1.0));
        };

        for (int iter_time = 0; iter_time < 20; ++iter_time)
        {
            for (int i = 0; i < left_num; ++i)
                gain[left_part[i].u] = 0.0;
            for (int i = 0; i < right_num; ++i)
                gain[right_part[i].u] = 0.0;

            // enumerate query nodes:
            for (auto const &mp : q_nodes_out_left)
            { // out edges:
                const std::vector<int> &q_l_nodes = mp.second;
                int deg_q_l = q_l_nodes.size();
                int u = mp.first;
                auto corres_ptr_r = q_nodes_out_right.find(u);
                int deg_q_r = (corres_ptr_r == q_nodes_out_right.end() ? 0 : (corres_ptr_r->second).size());
                double move_cost =
                    part_cost_func(left_num, deg_q_l, right_num, deg_q_r) -
                    part_cost_func(left_num, deg_q_l - 1, right_num, deg_q_r + 1);
                for (const auto v : q_l_nodes)
                    gain[v] += move_cost;
            }
            for (auto const &mp : q_nodes_out_right)
            { // out edges:
                const std::vector<int> &q_r_nodes = mp.second;
                int deg_q_r = q_r_nodes.size();
                int u = mp.first;
                auto corres_ptr_l = q_nodes_out_left.find(u);
                int deg_q_l = (corres_ptr_l == q_nodes_out_left.end() ? 0 : (corres_ptr_l->second).size());
                double move_cost =
                    part_cost_func(left_num, deg_q_l, right_num, deg_q_r) -
                    part_cost_func(left_num, deg_q_l + 1, right_num, deg_q_r - 1);
                for (const auto v : q_r_nodes)
                    gain[v] += move_cost;
            }
            for (auto const &mp : q_nodes_in_left)
            { // out edges:
                const std::vector<int> &q_l_nodes = mp.second;
                int deg_q_l = q_l_nodes.size();
                int u = mp.first;
                auto corres_ptr_r = q_nodes_in_right.find(u);
                int deg_q_r = (corres_ptr_r == q_nodes_in_right.end() ? 0 : (corres_ptr_r->second).size());
                double move_cost =
                    part_cost_func(left_num, deg_q_l, right_num, deg_q_r) -
                    part_cost_func(left_num, deg_q_l - 1, right_num, deg_q_r + 1);
                for (const auto v : q_l_nodes)
                    gain[v] += move_cost;
            }
            for (auto const &mp : q_nodes_in_right)
            { // out edges:
                const std::vector<int> &q_r_nodes = mp.second;
                int deg_q_r = q_r_nodes.size();
                int u = mp.first;
                auto corres_ptr_l = q_nodes_in_left.find(u);
                int deg_q_l = (corres_ptr_l == q_nodes_in_left.end() ? 0 : (corres_ptr_l->second).size());
                double move_cost =
                    part_cost_func(left_num, deg_q_l, right_num, deg_q_r) -
                    part_cost_func(left_num, deg_q_l + 1, right_num, deg_q_r - 1);
                for (const auto v : q_r_nodes)
                    gain[v] += move_cost;
            }

            // collect gains:
            for (int i = 0; i < left_num; ++i)
                left_part[i].gain = gain[left_part[i].u];
            for (int i = 0; i < right_num; ++i)
                right_part[i].gain = gain[right_part[i].u];
            std::sort(left_part, left_part + left_num);
            std::sort(right_part, right_part + right_num);
            bool is_converged = true;
            for (int i = 0; i < std::min(left_num, right_num); ++i)
                if (left_part[i].gain + right_part[i].gain > 0)
                {
                    std::swap(left_part[i], right_part[i]);
                    is_converged = false;
                }
                else
                    break;
            if (is_converged)
                break;
        }

        graph_bisection2(left_part, set_label, gain, left_num, cur_label);
        graph_bisection2(right_part, set_label, gain, right_num, cur_label);
    }

    void bfsr_bisection(int *nodes, int tot_num, int *que, int *visited, int &vis_label)
    {
        if (tot_num < 32)
            return;
        ++vis_label;
        // printf("vis_label=%d tot_num=%d\n", vis_label, tot_num);
        for (int i = 0; i < tot_num; ++i)
            visited[nodes[i]] = -vis_label;
        // find the last encountered node.
        int front_idx = 0, back_idx = 0;
        int last = nodes[0];
        visited[nodes[0]] = vis_label;
        que[back_idx++] = nodes[0];
        while (front_idx < back_idx)
        {
            int u = que[front_idx++];
            last = u;
            for (int i = 0; i < graph[u].out_deg; ++i)
            {
                int v = outedge[graph[u].out_start + i];
                if (visited[v] == -vis_label)
                {
                    visited[v] = vis_label;
                    que[back_idx++] = v;
                }
            }
        }

        int last_pos = 0;
        for (int i = 0; i < tot_num; ++i)
            if (nodes[i] == last)
            {
                last_pos = i;
                break;
            }
        std::swap(nodes[0], nodes[last_pos]);

        for (int i = 0; i < tot_num; ++i)
            visited[nodes[i]] = -vis_label;

        // mark the left part by vis_label, then the right part is marked by -vis_label
        front_idx = 0, back_idx = 0;
        // visited[last] = vis_label;
        // que[back_idx++] = last;
        for (int i = 0; i < tot_num && back_idx < tot_num / 4; ++i)
        {
            int s = nodes[i];
            if (visited[s] == -vis_label)
            {
                visited[s] = vis_label;
                que[back_idx++] = s;
                while (front_idx < back_idx && back_idx < tot_num / 2)
                {
                    int u = que[front_idx++];
                    for (int i = 0; i < graph[u].out_deg; ++i)
                    {
                        int v = outedge[graph[u].out_start + i];
                        if (visited[v] == -vis_label)
                        {
                            visited[v] = vis_label;
                            que[back_idx++] = v;
                            if (back_idx * 4 >= tot_num * 3)
                                break;
                        }
                    }
                }
            }
        }

        int left_num = back_idx;
        int right_num = tot_num - left_num;
        int *left_part = nodes;
        int *right_part = nodes + left_num;
        int a = 0, b = 0;
        while (true)
        {
            while (a < left_num && visited[left_part[a]] == vis_label)
                a++;
            while (b < right_num && visited[right_part[b]] == -vis_label)
                b++;
            if (a < left_num && b < right_num)
                std::swap(left_part[a], right_part[b]);
            else
                break;
        }

        // printf("left_num=%d right_num=%d\n", left_num, right_num);
        bfsr_bisection(left_part, left_num, que, visited, vis_label);
        bfsr_bisection(right_part, right_num, que, visited, vis_label);
    }
};

#endif