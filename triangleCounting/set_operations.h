#ifndef _SET_OPERATION_H
#define _SET_OPERATION_H

#include <stdint.h>
#include <string.h>
#include <immintrin.h>

typedef struct
{
    uint32_t state[4];
} PackState;

#define cyclic_shift1 _MM_SHUFFLE(0, 3, 2, 1)
#define cyclic_shift2 _MM_SHUFFLE(2, 1, 0, 3)
#define cyclic_shift3 _MM_SHUFFLE(1, 0, 3, 2)

static int *prepare_byte_check_mask_dict()
{
    int *mask = (int *)malloc(65536 * sizeof(int));
    if (!mask)
        return NULL;

    for (int x = 0; x < 65536; ++x)
    {
        int c0 = (x & 0xf), c1 = ((x >> 4) & 0xf);
        int c2 = ((x >> 8) & 0xf), c3 = ((x >> 12) & 0xf);

        int s0, s1, s2, s3;

        /* Transform each nibble */
        s0 = (c0 == 0) ? -1 : (c0 == 1) ? 0
                          : (c0 == 2)   ? 1
                          : (c0 == 4)   ? 2
                          : (c0 == 8)   ? 3
                                        : 4;
        s1 = (c1 == 0) ? -1 : (c1 == 1) ? 0
                          : (c1 == 2)   ? 1
                          : (c1 == 4)   ? 2
                          : (c1 == 8)   ? 3
                                        : 4;
        s2 = (c2 == 0) ? -1 : (c2 == 1) ? 0
                          : (c2 == 2)   ? 1
                          : (c2 == 4)   ? 2
                          : (c2 == 8)   ? 3
                                        : 4;
        s3 = (c3 == 0) ? -1 : (c3 == 1) ? 0
                          : (c3 == 2)   ? 1
                          : (c3 == 4)   ? 2
                          : (c3 == 8)   ? 3
                                        : 4;

        int is_multiple_match = (s0 == 4) || (s1 == 4) || (s2 == 4) || (s3 == 4);
        if (is_multiple_match)
        {
            mask[x] = -1;
            continue;
        }

        int is_no_match = (s0 == -1) && (s1 == -1) && (s2 == -1) && (s3 == -1);
        if (is_no_match)
        {
            mask[x] = -2;
            continue;
        }

        if (s0 == -1)
            s0 = 0;
        if (s1 == -1)
            s1 = 1;
        if (s2 == -1)
            s2 = 2;
        if (s3 == -1)
            s3 = 3;

        mask[x] = (s0) | (s1 << 2) | (s2 << 4) | (s3 << 6);
    }

    return mask;
}

// static const int *byte_check_mask_dict = prepare_byte_check_mask_dict();

static uint8_t *prepare_match_shuffle_dict()
{
    uint8_t *dict = (uint8_t *)malloc(4096 * sizeof(uint8_t));
    if (!dict)
        return NULL;

    for (int x = 0; x < 256; ++x)
    {
        for (int i = 0; i < 4; ++i)
        {
            uint8_t c = (x >> (i << 1)) & 3;
            int pos = x * 16 + i * 4;
            for (uint8_t j = 0; j < 4; ++j)
                dict[pos + j] = c * 4 + j;
        }
    }

    return dict;
}

#define all_zero_si128 _mm_setzero_si128()
#define all_one_si128 _mm_set_epi32(0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff)
static int *byte_check_mask_dict = NULL;
static __m128i *match_shuffle_dict = NULL;
/* Add an initialization function */
static void set_operation_init(void)
{
    if (!byte_check_mask_dict)
    {
        byte_check_mask_dict = prepare_byte_check_mask_dict();
    }
    if (!match_shuffle_dict)
    {
        match_shuffle_dict = (__m128i *)prepare_match_shuffle_dict();
    }
}
static const uint8_t shuffle_pi8_array[256] = {
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    0, 1, 2, 3, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    4, 5, 6, 7, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    0, 1, 2, 3, 4, 5, 6, 7, 255, 255, 255, 255, 255, 255, 255, 255,
    8, 9, 10, 11, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    0, 1, 2, 3, 8, 9, 10, 11, 255, 255, 255, 255, 255, 255, 255, 255,
    4, 5, 6, 7, 8, 9, 10, 11, 255, 255, 255, 255, 255, 255, 255, 255,
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 255, 255, 255, 255,
    12, 13, 14, 15, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    0, 1, 2, 3, 12, 13, 14, 15, 255, 255, 255, 255, 255, 255, 255, 255,
    4, 5, 6, 7, 12, 13, 14, 15, 255, 255, 255, 255, 255, 255, 255, 255,
    0, 1, 2, 3, 4, 5, 6, 7, 12, 13, 14, 15, 255, 255, 255, 255,
    8, 9, 10, 11, 12, 13, 14, 15, 255, 255, 255, 255, 255, 255, 255, 255,
    0, 1, 2, 3, 8, 9, 10, 11, 12, 13, 14, 15, 255, 255, 255, 255,
    4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 255, 255, 255, 255,
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};

static const __m128i *shuffle_mask = (__m128i *)(shuffle_pi8_array);
unsigned long long inter_cnt = 0, no_match_cnt = 0, cmp_cnt = 0;
unsigned long long multimatch_cnt = 0, skew_cnt = 0, low_select_cnt = 0;
unsigned long long byte_check_cnt[4] = {0, 0, 0, 0};

// static const __m128i *match_shuffle_dict = (__m128i *)prepare_match_shuffle_dict();

static const uint8_t byte_check_group_a_pi8[64] = {
    0, 0, 0, 0, 4, 4, 4, 4, 8, 8, 8, 8, 12, 12, 12, 12,
    1, 1, 1, 1, 5, 5, 5, 5, 9, 9, 9, 9, 13, 13, 13, 13,
    2, 2, 2, 2, 6, 6, 6, 6, 10, 10, 10, 10, 14, 14, 14, 14,
    3, 3, 3, 3, 7, 7, 7, 7, 11, 11, 11, 11, 15, 15, 15, 15};

static const uint8_t byte_check_group_b_pi8[64] = {
    0, 4, 8, 12, 0, 4, 8, 12, 0, 4, 8, 12, 0, 4, 8, 12,
    1, 5, 9, 13, 1, 5, 9, 13, 1, 5, 9, 13, 1, 5, 9, 13,
    2, 6, 10, 14, 2, 6, 10, 14, 2, 6, 10, 14, 2, 6, 10, 14,
    3, 7, 11, 15, 3, 7, 11, 15, 3, 7, 11, 15, 3, 7, 11, 15};

static const __m128i *byte_check_group_a_order = (__m128i *)(byte_check_group_a_pi8);
static const __m128i *byte_check_group_b_order = (__m128i *)(byte_check_group_b_pi8);

static int intersect_filter_simd4x_count(int *set_a, int size_a, int *set_b, int size_b)
{
    set_operation_init();
    int i = 0, j = 0, res = 0;
    int qs_a = size_a - (size_a & 3);
    int qs_b = size_b - (size_b & 3);

    while (i < qs_a && j < qs_b)
    {
        __m128i v_a = _mm_lddqu_si128((__m128i *)(set_a + i));
        __m128i v_b = _mm_lddqu_si128((__m128i *)(set_b + j));

        int a_max = set_a[i + 3];
        int b_max = set_b[j + 3];

        if (a_max == b_max)
        {
            i += 4;
            j += 4;
            _mm_prefetch((char *)(set_a + i), _MM_HINT_NTA);
            _mm_prefetch((char *)(set_b + j), _MM_HINT_NTA);
        }
        else if (a_max < b_max)
        {
            i += 4;
            _mm_prefetch((char *)(set_a + i), _MM_HINT_NTA);
        }
        else
        {
            j += 4;
            _mm_prefetch((char *)(set_b + j), _MM_HINT_NTA);
        }

        __m128i byte_group_a = _mm_shuffle_epi8(v_a, byte_check_group_a_order[0]);
        __m128i byte_group_b = _mm_shuffle_epi8(v_b, byte_check_group_b_order[0]);
        __m128i byte_check_mask = _mm_cmpeq_epi8(byte_group_a, byte_group_b);
        int bc_mask = _mm_movemask_epi8(byte_check_mask);
        int ms_order = byte_check_mask_dict[bc_mask];

        if (ms_order == -1)
        {
            byte_group_a = _mm_shuffle_epi8(v_a, byte_check_group_a_order[1]);
            byte_group_b = _mm_shuffle_epi8(v_b, byte_check_group_b_order[1]);
            byte_check_mask = _mm_and_si128(byte_check_mask,
                                            _mm_cmpeq_epi8(byte_group_a, byte_group_b));
            bc_mask = _mm_movemask_epi8(byte_check_mask);
            ms_order = byte_check_mask_dict[bc_mask];

            if (ms_order == -1)
            {
                byte_group_a = _mm_shuffle_epi8(v_a, byte_check_group_a_order[2]);
                byte_group_b = _mm_shuffle_epi8(v_b, byte_check_group_b_order[2]);
                byte_check_mask = _mm_and_si128(byte_check_mask,
                                                _mm_cmpeq_epi8(byte_group_a, byte_group_b));
                bc_mask = _mm_movemask_epi8(byte_check_mask);
                ms_order = byte_check_mask_dict[bc_mask];

                if (ms_order == -1)
                {
                    byte_group_a = _mm_shuffle_epi8(v_a, byte_check_group_a_order[3]);
                    byte_group_b = _mm_shuffle_epi8(v_b, byte_check_group_b_order[3]);
                    byte_check_mask = _mm_and_si128(byte_check_mask,
                                                    _mm_cmpeq_epi8(byte_group_a, byte_group_b));
                    bc_mask = _mm_movemask_epi8(byte_check_mask);
                    ms_order = byte_check_mask_dict[bc_mask];
                }
            }
        }

        if (ms_order == -2)
            continue;

        __m128i sf_v_b = _mm_shuffle_epi8(v_b, match_shuffle_dict[ms_order]);
        __m128i cmp_mask = _mm_cmpeq_epi32(v_a, sf_v_b);

        int mask = _mm_movemask_ps((__m128)cmp_mask);
        res += _mm_popcnt_u32(mask);
    }

    while (i < size_a && j < size_b)
    {
        if (set_a[i] == set_b[j])
        {
            res++;
            i++;
            j++;
        }
        else if (set_a[i] < set_b[j])
        {
            i++;
        }
        else
        {
            j++;
        }
    }

    return res;
}

static int bp_intersect_filter_simd4x_count(int *bases_a, PackState *states_a, int size_a,
                                            int *bases_b, PackState *states_b, int size_b)
{
    set_operation_init();
    inter_cnt++;
    int len_a = (size_a < size_b) ? size_a : size_b;
    int len_b = (size_a > size_b) ? size_a : size_b;
    if (len_a * 32 < len_b)
        skew_cnt++;
    int size_c = 0;

    int i = 0, j = 0, res = 0;
    int qs_a = size_a - (size_a & 3);
    int qs_b = size_b - (size_b & 3);
    uint64_t bits[2] __attribute__((aligned(16)));

    while (i < qs_a && j < qs_b)
    {
        cmp_cnt++;
        __m128i base_a = _mm_lddqu_si128((__m128i *)(bases_a + i));
        __m128i base_b = _mm_lddqu_si128((__m128i *)(bases_b + j));
        __m128i state_a = _mm_lddqu_si128((__m128i *)(states_a + i));
        __m128i state_b = _mm_lddqu_si128((__m128i *)(states_b + j));

        int a_max = bases_a[i + 3];
        int b_max = bases_b[j + 3];
        if (a_max == b_max)
        {
            i += 4;
            j += 4;
            _mm_prefetch((char *)(bases_a + i), _MM_HINT_NTA);
            _mm_prefetch((char *)(states_a + i), _MM_HINT_NTA);
            _mm_prefetch((char *)(bases_b + j), _MM_HINT_NTA);
            _mm_prefetch((char *)(states_b + j), _MM_HINT_NTA);
        }
        else if (a_max < b_max)
        {
            i += 4;
            _mm_prefetch((char *)(bases_a + i), _MM_HINT_NTA);
            _mm_prefetch((char *)(states_a + i), _MM_HINT_NTA);
        }
        else
        {
            j += 4;
            _mm_prefetch((char *)(bases_b + j), _MM_HINT_NTA);
            _mm_prefetch((char *)(states_b + j), _MM_HINT_NTA);
        }

        int bn = 0;
        __m128i byte_group_a = _mm_shuffle_epi8(base_a, byte_check_group_a_order[0]);
        __m128i byte_group_b = _mm_shuffle_epi8(base_b, byte_check_group_b_order[0]);
        __m128i byte_check_mask = _mm_cmpeq_epi8(byte_group_a, byte_group_b);
        int bc_mask = _mm_movemask_epi8(byte_check_mask);
        int ms_order = byte_check_mask_dict[bc_mask];

        if (ms_order == -1)
        {
            multimatch_cnt++;
            byte_group_a = _mm_shuffle_epi8(base_a, byte_check_group_a_order[1]);
            byte_group_b = _mm_shuffle_epi8(base_b, byte_check_group_b_order[1]);
            byte_check_mask = _mm_and_si128(byte_check_mask,
                                            _mm_cmpeq_epi8(byte_group_a, byte_group_b));
            bc_mask = _mm_movemask_epi8(byte_check_mask);
            ms_order = byte_check_mask_dict[bc_mask];
            bn++;

            if (ms_order == -1)
            {
                byte_group_a = _mm_shuffle_epi8(base_a, byte_check_group_a_order[2]);
                byte_group_b = _mm_shuffle_epi8(base_b, byte_check_group_b_order[2]);
                byte_check_mask = _mm_and_si128(byte_check_mask,
                                                _mm_cmpeq_epi8(byte_group_a, byte_group_b));
                bc_mask = _mm_movemask_epi8(byte_check_mask);
                ms_order = byte_check_mask_dict[bc_mask];
                bn++;

                if (ms_order == -1)
                {
                    byte_group_a = _mm_shuffle_epi8(base_a, byte_check_group_a_order[3]);
                    byte_group_b = _mm_shuffle_epi8(base_b, byte_check_group_b_order[3]);
                    byte_check_mask = _mm_and_si128(byte_check_mask,
                                                    _mm_cmpeq_epi8(byte_group_a, byte_group_b));
                    bc_mask = _mm_movemask_epi8(byte_check_mask);
                    ms_order = byte_check_mask_dict[bc_mask];
                    bn++;
                }
            }
        }

        if (ms_order == -2)
        {
            no_match_cnt++;
            continue;
        }
        byte_check_cnt[bn]++;

        __m128i sf_base_b = _mm_shuffle_epi8(base_b, match_shuffle_dict[ms_order]);
        __m128i sf_state_b = _mm_shuffle_epi8(state_b, match_shuffle_dict[ms_order]);
        __m128i cmp_mask = _mm_cmpeq_epi32(base_a, sf_base_b);
        __m128i and_state = _mm_and_si128(cmp_mask, _mm_and_si128(state_a, sf_state_b));

        __m128i state_mask = _mm_cmpeq_epi32(and_state, all_zero_si128);
        cmp_mask = _mm_andnot_si128(state_mask, cmp_mask);
        int mask = _mm_movemask_ps((__m128)cmp_mask);
        size_c += _mm_popcnt_u32(mask);

        _mm_store_si128((__m128i *)bits, and_state);
        res += _mm_popcnt_u64(bits[0]);
        res += _mm_popcnt_u64(bits[1]);
    }

    while (i < size_a && j < size_b)
    {
        if (bases_a[i] == bases_b[j])
        {
            res += _mm_popcnt_u32(states_a[i].state[0] & states_b[j].state[0]) +
                   _mm_popcnt_u32(states_a[i].state[1] & states_b[j].state[1]) +
                   _mm_popcnt_u32(states_a[i].state[2] & states_b[j].state[2]) +
                   _mm_popcnt_u32(states_a[i].state[3] & states_b[j].state[3]);
            i++;
            j++;
        }
        else if (bases_a[i] < bases_b[j])
        {
            i++;
        }
        else
        {
            j++;
        }
    }

    double selectivity = (double)size_c / len_a;
    if ((selectivity < 0.3 || len_a < 8) && len_a * 32 >= len_b)
        low_select_cnt++;

    return res;
}

static int merge(int *set_a, int size_a, int *set_b, int size_b, int *set_c)
{
    set_operation_init();
    int i = 0, j = 0, size_c = 0;
    while (i < size_a && j < size_b)
    {
        if (set_a[i] < set_b[j])
        {
            set_c[size_c++] = set_a[i++];
        }
        else
        {
            set_c[size_c++] = set_b[j++];
        }
    }
    memcpy(set_c + size_c, set_a + i, (size_a - i) * sizeof(int));
    size_c += (size_a - i);
    memcpy(set_c + size_c, set_b + j, (size_b - j) * sizeof(int));
    size_c += (size_b - j);

    return size_c;
}

#endif