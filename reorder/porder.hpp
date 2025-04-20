#ifndef _PORDER_H
#define _PORDER_H
#include <unordered_set>
#include <unordered_map>
#include <stack>
#include <cmath>
#include "linkedlist_heap.hpp"
#include "doublelinked_list.hpp"
#include "util.hpp"

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

    EdgeVector greedy_mheap()
    {
        memset(new_id, -1, sizeof(int) * v_num);
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
        LinkedListHeap node_heap(v_num);
        int *out_nbr_labels, *in_nbr_labels;
        align_malloc((void **)&out_nbr_labels, 32, sizeof(int) * v_num);
        align_malloc((void **)&in_nbr_labels, 32, sizeof(int) * v_num);
        memset(out_nbr_labels, -1, sizeof(int) * v_num);
        memset(in_nbr_labels, -1, sizeof(int) * v_num);
        const int window_size = PACK_WIDTH;
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

            for (int i = 0; i < graph[u].in_deg; ++i)
            {
                int v = inedge[graph[u].in_start + i];
                if (out_nbr_labels[v] == cur_p_idx)
                    continue;
                out_nbr_labels[v] = cur_p_idx;
                for (int j = 0; j < graph[v].out_deg; ++j)
                {
                    int w = outedge[graph[v].out_start + j];
                    if (node_heap.in_heap(w))
                        node_heap.inc(w);
                }
            }
            for (int i = 0; i < graph[u].out_deg; ++i)
            {
                int v = outedge[graph[u].out_start + i];
                if (in_nbr_labels[v] == cur_p_idx)
                    continue;
                in_nbr_labels[v] = cur_p_idx;
                for (int j = 0; j < graph[v].in_deg; ++j)
                {
                    int w = inedge[graph[v].in_start + j];
                    if (node_heap.in_heap(w))
                        node_heap.inc(w);
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

        free(out_nbr_labels);
        free(in_nbr_labels);

        return edge_vec;
    }
    void set_alpha_by_deg()
    {
        double sum = 0.0;
        for (int i = 0; i < v_num; ++i)
        {
            alpha_out[i] = sqrt(graph[i].out_deg);
            alpha_in[i] = sqrt(graph[i].in_deg);
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
};

#endif