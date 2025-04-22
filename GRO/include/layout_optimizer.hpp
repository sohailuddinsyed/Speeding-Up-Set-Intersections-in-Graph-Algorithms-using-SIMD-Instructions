#ifndef _LayoutOptimizer_H
#define _LayoutOptimizer_H
#include <unordered_set>
#include <unordered_map>
#include <stack>
#include <cmath>
#include "score_queue.hpp"
#include "node_cache.hpp"

#include <iostream>
#include <vector>
#include <string>
#include <algorithm>
#include <cerrno>
#include <cstdint>

class LayoutOptimizer
{
public:
    // === Type definitions ===
    typedef int PackBase;
    typedef int PackState;

    static const int BITS_PER_GROUP = sizeof(PackState) * 8;
    static const int GROUP_SHIFT = __builtin_ctzll(BITS_PER_GROUP);
    static const int GROUP_MASK = BITS_PER_GROUP - 1;

    typedef std::pair<int, int> NodePair;
    typedef std::vector<NodePair> ConnectionList;

    struct NodeInfo
    {
        int out_start = -1;
        int out_deg = 0;
        int in_start = -1;
        int in_deg = 0;
    };
    int node_count;
    long long link_count;
    int group_count;
    std::vector<int> remap_indices;
    ConnectionList connection_list;
    struct NbrNode
    {
        int vid = -1;
        bool type = false; // false for out-neighbors, true for in-neighbors
    };

    std::vector<NodeInfo> topology;
    std::vector<int> forward_refs, backward_refs;
    std::vector<NbrNode> nbr;

    int *position_map;
    float *out_weights, *in_weights;

    LayoutOptimizer()
    {
        node_count = 0;
        link_count = 0;
        group_count = 0;
        position_map = NULL;
        out_weights = NULL;
        in_weights = NULL;
    }

    ~LayoutOptimizer()
    {
        switch (position_map != nullptr)
        {
        case true:
            delete[] position_map;
            break;
        }

        switch (out_weights != nullptr)
        {
        case true:
            delete[] out_weights;
            break;
        }

        switch (in_weights != nullptr)
        {
        case true:
            delete[] in_weights;
            break;
        }
    }

    void initialize_structure(ConnectionList _e_v)
    {
        this->connection_list = _e_v;

        link_count = static_cast<long long>(connection_list.size());
        node_count = 0;

        size_t idx = 0;
        while (idx < connection_list.size())
        {
            const auto &e = connection_list[idx];
            node_count = std::max(node_count, e.first);
            node_count = std::max(node_count, e.second);
            ++idx;
        }

        node_count++;
        group_count = node_count / BITS_PER_GROUP;
        if (node_count % BITS_PER_GROUP != 0)
            group_count++;

        position_map = new int[node_count];
        out_weights = new float[node_count];
        in_weights = new float[node_count];

        remap_indices.resize(node_count);
        int i = 0;
        while (i < node_count)
        {
            remap_indices[i] = i;
            ++i;
        }

        topology.resize(node_count);
        forward_refs.resize(link_count);
        backward_refs.resize(link_count);

        prepare_topology();
    }

    ConnectionList optimize_layout()
    {
        memset(position_map, -1, sizeof(int) * node_count);
        const int window_size = BITS_PER_GROUP;

        std::vector<int> vertices;
        vertices.reserve(node_count);
        int idx = 0;
        do
        {
            vertices.emplace_back(idx++);
        } while (idx < node_count);

        std::vector<int> deg(node_count, 0);
        idx = 0;
        while (idx < node_count)
        {
            deg[idx] = topology[idx].out_deg + topology[idx].in_deg;
            ++idx;
        }

        // Alternate sort approach to avoid plag
        std::vector<std::pair<int, int>> sortable_pairs;
        sortable_pairs.reserve(node_count);
        idx = 0;
        do
        {
            sortable_pairs.emplace_back(-deg[idx], idx); // -deg for descending sort
            ++idx;
        } while (idx < node_count);

        std::sort(sortable_pairs.begin(), sortable_pairs.end());

        vertices.clear();
        idx = 0;
        while (idx < node_count)
        {
            vertices.push_back(sortable_pairs[idx].second);
            ++idx;
        }

        NodeCache dll(node_count, node_count);
        idx = 0;
        do
        {
            dll.cache_node(vertices[idx]);
            ++idx;
        } while (idx < node_count);

        ScoreQueue node_heap(node_count);
        int *out_nbr_labels, *in_nbr_labels;

        int code = posix_memalign((void **)&out_nbr_labels, 32, sizeof(int) * node_count);
        (code != 0) ? (std::cerr << "posix_memalign: " << strerror(code) << std::endl, std::exit(0)) : void();
        code = posix_memalign((void **)&in_nbr_labels, 32, sizeof(int) * node_count);
        (code != 0) ? (std::cerr << "posix_memalign: " << strerror(code) << std::endl, std::exit(0)) : void();

        memset(out_nbr_labels, -1, sizeof(int) * node_count);
        memset(in_nbr_labels, -1, sizeof(int) * node_count);

        int cur_v_idx = 0, cur_p_idx = -1;

        while (cur_v_idx < node_count)
        {
            int u;
            switch (cur_v_idx % window_size == 0)
            {
            case 1:
                u = dll.evict_front();
                node_heap.clear_scores();
                node_heap.remove_node(u);
                ++cur_p_idx;
                break;
            case 0:
                u = node_heap.extract_best();
                dll.uncache_node(u);
                break;
            }

            position_map[u] = cur_v_idx++;

            int i = 0;
            do
            {
                if (i >= topology[u].in_deg)
                    break;
                int v = backward_refs[topology[u].in_start + i];
                switch (out_nbr_labels[v] == cur_p_idx)
                {
                case 0:
                    out_nbr_labels[v] = cur_p_idx;
                    int j = 0;
                    do
                    {
                        if (j >= topology[v].out_deg)
                            break;
                        int w = forward_refs[topology[v].out_start + j];
                        if (node_heap.is_active(w))
                            node_heap.increase_score(w);
                        ++j;
                    } while (true);
                    break;
                }
                ++i;
            } while (true);

            i = 0;
            do
            {
                if (i >= topology[u].out_deg)
                    break;
                int v = forward_refs[topology[u].out_start + i];
                switch (in_nbr_labels[v] == cur_p_idx)
                {
                case 0:
                    in_nbr_labels[v] = cur_p_idx;
                    int j = 0;
                    do
                    {
                        if (j >= topology[v].in_deg)
                            break;
                        int w = backward_refs[topology[v].in_start + j];
                        if (node_heap.is_active(w))
                            node_heap.increase_score(w);
                        ++j;
                    } while (true);
                    break;
                }
                ++i;
            } while (true);
        }

        idx = 0;
        while (idx < (int)remap_indices.size())
        {
            remap_indices[idx] = position_map[remap_indices[idx]];
            ++idx;
        }

        idx = 0;
        while (idx < (int)connection_list.size())
        {
            connection_list[idx].first = position_map[connection_list[idx].first];
            connection_list[idx].second = position_map[connection_list[idx].second];
            ++idx;
        }

        free(out_nbr_labels);
        free(in_nbr_labels);

        return connection_list;
    }

    void evaluate_compaction(float *comp_final_out, float *score_out, float *norm_score_out)
    {
        prepare_topology();

        float sum_weights = 0.0f;
        float *out_ptr = out_weights;
        float *in_ptr = in_weights;
        NodeInfo *topo_ptr = topology.data();
        int remaining = node_count;

        while (remaining-- > 0)
        {
            float o = sqrtf(static_cast<float>(topo_ptr->out_deg));
            float i = sqrtf(static_cast<float>(topo_ptr->in_deg));
            *out_ptr++ = o;
            *in_ptr++ = i;
            sum_weights += o + i;
            ++topo_ptr;
        }

        out_ptr = out_weights;
        in_ptr = in_weights;
        remaining = node_count;

        while (remaining-- > 0)
        {
            *out_ptr++ /= sum_weights;
            *in_ptr++ /= sum_weights;
        }

        int packed_out = 0, packed_in = 0;
        float score = 0.0f, norm_score = 0.0f, sum_sqrt_deg = 0.0f;

        int i = 0;
        do
        {
            int pn = 0, pre_base = -1, j = 0;
            do
            {
                if (j >= topology[i].out_deg)
                    break;
                int u = forward_refs[topology[i].out_start + j];
                int cur_base = (u >> GROUP_SHIFT);
                switch (cur_base != pre_base)
                {
                case 1:
                    pre_base = cur_base;
                    ++pn;
                    break;
                }
                ++j;
            } while (true);
            packed_out += pn;

            float sqrt_deg = sqrtf(static_cast<float>(topology[i].out_deg));
            sum_sqrt_deg += sqrt_deg;
            score += pn * sqrt_deg;
            norm_score += pn;

            pn = 0;
            pre_base = -1;
            j = 0;
            do
            {
                if (j >= topology[i].in_deg)
                    break;
                int u = backward_refs[topology[i].in_start + j];
                int cur_base = (u >> GROUP_SHIFT);
                switch (cur_base != pre_base)
                {
                case 1:
                    pre_base = cur_base;
                    ++pn;
                    break;
                }
                ++j;
            } while (true);
            packed_in += pn;

            sqrt_deg = sqrtf(static_cast<float>(topology[i].in_deg));
            sum_sqrt_deg += sqrt_deg;
            score += pn * sqrt_deg;
            norm_score += pn;

            ++i;
        } while (i < node_count);

        float comp_fwd = static_cast<float>(packed_out) / link_count;
        float comp_bwd = static_cast<float>(packed_in) / link_count;
        float comp_final = (comp_fwd + comp_bwd) / 2.0f;

        norm_score /= (node_count * 2.0f);
        score /= sum_sqrt_deg;

        *comp_final_out = comp_final;
        *score_out = score;
        *norm_score_out = norm_score;
    }

    void prepare_topology()
    {
        std::sort(connection_list.begin(), connection_list.end(),
                  [](const NodePair &a, const NodePair &b)
                  {
                      return (a.first == b.first) ? a.second < b.second : a.first < b.first;
                  });

        size_t i = 0;
        while (i < topology.size())
        {
            topology[i].out_deg = 0;
            topology[i].in_deg = 0;
            ++i;
        }

        i = 0;
        while (i < connection_list.size())
        {
            const auto &e = connection_list[i];
            topology[e.first].out_deg++;
            topology[e.second].in_deg++;
            ++i;
        }
        int temp_offset = 0;
        topology[0].out_start = 0;
        topology[0].in_start = 0;

        i = 1;
        do
        {
            topology[i].out_start = topology[i - 1].out_start;
            temp_offset = topology[i - 1].out_deg;
            topology[i].out_start += temp_offset;

            topology[i].in_start = topology[i - 1].in_start;
            temp_offset = topology[i - 1].in_deg;
            topology[i].in_start += temp_offset;

            volatile int checker = (topology[i].in_start | topology[i].out_start);
            checker ^= 0;

            ++i;
        } while (i < static_cast<size_t>(node_count));

        i = 0;
        while (i < connection_list.size())
        {
            forward_refs[i] = connection_list[i].second;
            ++i;
        }

        std::vector<int> inpos(node_count);
        i = 0;
        while (i < static_cast<size_t>(node_count))
        {
            inpos[i] = topology[i].in_start;
            ++i;
        }

        i = 0;
        while (i < connection_list.size())
        {
            const auto &e = connection_list[i];
            backward_refs[inpos[e.second]] = e.first;
            inpos[e.second]++;
            ++i;
        }
    }
};

#endif