#include "util.hpp"
#include "set_operation.hpp"

class BPTriangleCount
{
public:
    int v_num;
    long long e_num;

    BPTriangleCount();
    ~BPTriangleCount();
    void build(const EdgeVector &_e_v);
    int triangle_count();
    int triangle_count_mt(int thread_num); // multi-threading using Intel TBB.
    EdgeVector edge_vec;
    std::vector<UVertex> graph;
    int *pool_base = NULL;
    PackState *pool_state = NULL;
};

BPTriangleCount::BPTriangleCount()
{
    v_num = 0;
    e_num = 0;
    align_malloc((void **)&pool_base, 32, sizeof(int) * PACK_NODE_POOL_SIZE);
    align_malloc((void **)&pool_state, 32, sizeof(PackState) * PACK_NODE_POOL_SIZE);
}

BPTriangleCount::~BPTriangleCount()
{
    free(pool_state);
    free(pool_base);
}

void BPTriangleCount::build(const EdgeVector &_e_v)
{
    EdgeVector rev_edge_vec;
    rev_edge_vec.reserve(_e_v.size() / 2);
    edge_vec.reserve(_e_v.size() / 2);

    for (const auto &e : _e_v)
    {
        if (e.first < e.second)
        {
            v_num = std::max(v_num, e.second);
            edge_vec.push_back(e);
        }
        else if (e.first > e.second)
        {
            rev_edge_vec.push_back(e);
        }
    }
    v_num++;

    std::sort(edge_vec.begin(), edge_vec.end(), edge_idpair_cmp);
    edge_vec.erase(std::unique(edge_vec.begin(), edge_vec.end()), edge_vec.end());
    std::sort(rev_edge_vec.begin(), rev_edge_vec.end(), edge_idpair_cmp);
    rev_edge_vec.erase(std::unique(rev_edge_vec.begin(), rev_edge_vec.end()), rev_edge_vec.end());

    e_num = (long long)edge_vec.size();

    graph.resize(v_num);

    int cur_packnode_idx = -1;
    int prev_u = -1;
    for (auto &e : rev_edge_vec)
    {
        PackBase v_base = (e.second >> PACK_SHIFT);
        PackState v_bit = ((PackState)1 << (e.second & PACK_MASK));
        if (e.first != prev_u)
        {
            prev_u = e.first;
            graph[e.first].start = ++cur_packnode_idx;
            graph[e.first].deg++;
            pool_base[cur_packnode_idx] = v_base;
            pool_state[cur_packnode_idx] = v_bit;
        }
        else
        {
            if (pool_base[cur_packnode_idx] == v_base)
            {
                pool_state[cur_packnode_idx] |= v_bit;
            }
            else
            {
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

int BPTriangleCount::triangle_count()
{
    int res = 0;
    for (const auto &e : edge_vec)
    {
        const UVertex &u = graph[e.first];
        const UVertex &v = graph[e.second];

#if SIMD_STATE == 2
        res += bp_intersect_scalar2x_count(pool_base + u.start, pool_state + u.start, u.deg,
                                           pool_base + v.start, pool_state + v.start, v.deg);
#elif SIMD_STATE == 4
#if SIMD_MODE == 0
        res += bp_intersect_simd4x_count(pool_base + u.start, pool_state + u.start, u.deg,
                                         pool_base + v.start, pool_state + v.start, v.deg);
#else
        res += bp_intersect_filter_simd4x_count(pool_base + u.start, pool_state + u.start, u.deg,
                                                pool_base + v.start, pool_state + v.start, v.deg);
#endif
#else
        res += bp_intersect_count(pool_base + u.start, pool_state + u.start, u.deg,
                                  pool_base + v.start, pool_state + v.start, v.deg);
#endif
    }
    return res;
}

int con_thread_num = 1;
int *con_res = NULL;
BPTriangleCount *bptc;

void *con_calc_triangle(void *con)
{
    long id;
    id = (unsigned long long)(con);
    long long e_idx_l = bptc->edge_vec.size() / con_thread_num * id;
    long long e_idx_r = bptc->edge_vec.size() / con_thread_num * (id + 1) - 1;
    if (id + 1 == con_thread_num)
        e_idx_r = bptc->edge_vec.size() - 1;

    for (long long i = e_idx_l; i < e_idx_r; ++i)
    {
        const auto &e = bptc->edge_vec[i];
        const UVertex &u = bptc->graph[e.first];
        const UVertex &v = bptc->graph[e.second];

#if SIMD_STATE == 2
        con_res[id] += bp_intersect_scalar2x_count(bptc->pool_base + u.start, bptc->pool_state + u.start, u.deg,
                                                   bptc->pool_base + v.start, bptc->pool_state + v.start, v.deg);
#elif SIMD_STATE == 4
#if SIMD_MODE == 0
        con_res[id] += bp_intersect_simd4x_count(bptc->pool_base + u.start, bptc->pool_state + u.start, u.deg,
                                                 bptc->pool_base + v.start, bptc->pool_state + v.start, v.deg);
#else
        con_res[id] += bp_intersect_filter_simd4x_count(bptc->pool_base + u.start, bptc->pool_state + u.start, u.deg,
                                                        bptc->pool_base + v.start, bptc->pool_state + v.start, v.deg);
#endif
#else
        con_res[id] += bp_intersect_count(bptc->pool_base + u.start, bptc->pool_state + u.start, u.deg,
                                          bptc->pool_base + v.start, bptc->pool_state + v.start, v.deg);
#endif
    }
    pthread_exit(NULL);
}

int BPTriangleCount::triangle_count_mt(int thread_num)
{
    con_thread_num = thread_num;
    align_malloc((void **)&con_res, 32, sizeof(int) * thread_num);
    printf("thread_num=%d\n", thread_num);
    memset(con_res, 0, sizeof(int) * thread_num);
    bptc = this;

    pthread_t *pt = (pthread_t *)malloc(con_thread_num * sizeof(pthread_t));
    struct timeval time_start;
    struct timeval time_end;
    gettimeofday(&time_start, NULL);
    for (long a = 0; a < con_thread_num; a++)
        pthread_create(&pt[a], NULL, con_calc_triangle, (void *)a);
    for (long a = 0; a < con_thread_num; a++)
        pthread_join(pt[a], NULL);
    gettimeofday(&time_end, NULL);
    double tc_mt_time = (time_end.tv_sec - time_start.tv_sec) * 1000.0 + (time_end.tv_usec - time_start.tv_usec) / 1000.0;
    printf("tc_mt_time=%.3fms\n", tc_mt_time);
    free(pt);
    int tot_res = 0;
    for (int i = 0; i < thread_num; ++i)
        tot_res += con_res[i];
    return tot_res;
}
