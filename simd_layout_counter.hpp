#include "graph_data_utils.hpp"
#include "vectorized_set_operations.hpp"

class SIMDTriangleAnalyzer;

struct WorkerInfo
{
    SIMDTriangleAnalyzer *self;
    long thread_id;
};

class SIMDTriangleAnalyzer
{
public:
    int vertex_count;
    long long edge_count;
    EdgeListContainer forward_edges;
    std::vector<CompressedVertexInfo> vertex_table;
    int *compressed_base = NULL;
    PackState *compressed_state = NULL;
    int worker_count = 1;
    int *partial_results = NULL;

    SIMDTriangleAnalyzer()
    {
        vertex_count = 0;
        edge_count = 0;
        allocate_memory_aligned((void **)&compressed_base, 32, sizeof(int) * CONST_NODE_POOL_SIZE);
        allocate_memory_aligned((void **)&compressed_state, 32, sizeof(PackState) * CONST_NODE_POOL_SIZE);
    }

    ~SIMDTriangleAnalyzer()
    {
        free(compressed_state);
        free(compressed_base);
        if (partial_results)
            free(partial_results);
    }

    int count_all_triangles()
    {
        int res = 0;
        for (const auto &e : forward_edges)
        {
            const CompressedVertexInfo &u = vertex_table[e.first];
            const CompressedVertexInfo &v = vertex_table[e.second];

            res += bitpacked_simd_intersection(compressed_base + u.base_index, compressed_state + u.base_index, u.degree,
                                               compressed_base + v.base_index, compressed_state + v.base_index, v.degree);
        }
        return res;
    }

    static void *execute_partial_count(void *arg)
    {
        WorkerInfo *data = (WorkerInfo *)arg;
        SIMDTriangleAnalyzer *self = data->self;
        long id = data->thread_id;

        long long e_idx_l = self->forward_edges.size() / self->worker_count * id;
        long long e_idx_r = (id + 1 == self->worker_count) ? self->forward_edges.size() - 1
                                                           : self->forward_edges.size() / self->worker_count * (id + 1) - 1;

        for (long long i = e_idx_l; i <= e_idx_r; ++i)
        {
            const auto &e = self->forward_edges[i];
            const CompressedVertexInfo &u = self->vertex_table[e.first];
            const CompressedVertexInfo &v = self->vertex_table[e.second];

            self->partial_results[id] += bitpacked_simd_intersection(self->compressed_base + u.base_index, self->compressed_state + u.base_index, u.degree,
                                                                     self->compressed_base + v.base_index, self->compressed_state + v.base_index, v.degree);
        }

        pthread_exit(NULL);
    }

    int count_triangles_parallel(int worker_count)
    {
        worker_count = worker_count;
        allocate_memory_aligned((void **)&partial_results, 32, sizeof(int) * worker_count);
        memset(partial_results, 0, sizeof(int) * worker_count);

        pthread_t *pt = (pthread_t *)malloc(worker_count * sizeof(pthread_t));
        WorkerInfo *args = new WorkerInfo[worker_count];

        struct timeval time_start;
        struct timeval time_end;
        gettimeofday(&time_start, NULL);

        for (long a = 0; a < worker_count; ++a)
        {
            args[a] = {this, a};
            pthread_create(&pt[a], NULL, SIMDTriangleAnalyzer::execute_partial_count, (void *)&args[a]);
        }

        for (long a = 0; a < worker_count; ++a)
        {
            pthread_join(pt[a], NULL);
        }

        gettimeofday(&time_end, NULL);
        double tc_mt_time = (time_end.tv_sec - time_start.tv_sec) * 1000.0 + (time_end.tv_usec - time_start.tv_usec) / 1000.0;
        printf("tc_mt_time=%.3fms\n", tc_mt_time);

        free(pt);
        delete[] args;

        int tot_res = 0;
        for (int i = 0; i < worker_count; ++i)
            tot_res += partial_results[i];

        return tot_res;
    }

    void initialize_vertex_table_data(const EdgeListContainer &input_edges)
    {
        EdgeListContainer rev_edge_vec;
        rev_edge_vec.reserve(input_edges.size() / 2);
        forward_edges.reserve(input_edges.size() / 2);

        for (const auto &e : input_edges)
        {
            if (e.first < e.second)
            {
                vertex_count = std::max(vertex_count, e.second);
                forward_edges.push_back(e);
            }
            else if (e.first > e.second)
            {
                rev_edge_vec.push_back(e);
            }
        }
        vertex_count++;

        std::sort(forward_edges.begin(), forward_edges.end(), compare_vertex_edges);
        forward_edges.erase(std::unique(forward_edges.begin(), forward_edges.end()), forward_edges.end());
        std::sort(rev_edge_vec.begin(), rev_edge_vec.end(), compare_vertex_edges);
        rev_edge_vec.erase(std::unique(rev_edge_vec.begin(), rev_edge_vec.end()), rev_edge_vec.end());

        edge_count = (long long)forward_edges.size();
        vertex_table.resize(vertex_count);

        int cur_packnode_idx = -1;
        int prev_u = -1;
        for (auto &e : rev_edge_vec)
        {
            PackBase v_base = (e.second >> CONST_PACK_SHIFT);
            PackState v_bit = ((PackState)1 << (e.second & CONST_PACK_MASK));
            if (e.first != prev_u)
            {
                prev_u = e.first;
                vertex_table[e.first].base_index = ++cur_packnode_idx;
                vertex_table[e.first].degree++;
                compressed_base[cur_packnode_idx] = v_base;
                compressed_state[cur_packnode_idx] = v_bit;
            }
            else
            {
                if (compressed_base[cur_packnode_idx] == v_base)
                {
                    compressed_state[cur_packnode_idx] |= v_bit;
                }
                else
                {
                    vertex_table[e.first].degree++;
                    compressed_base[++cur_packnode_idx] = v_base;
                    compressed_state[cur_packnode_idx] = v_bit;
                }
            }
        }

        cur_packnode_idx++;
        double comp_ratio = (double)cur_packnode_idx / edge_count;
        printf("comp_ratio=%d/%lld, %.4f\n", cur_packnode_idx, edge_count, comp_ratio);
    }
};
