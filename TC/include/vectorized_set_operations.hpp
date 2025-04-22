#ifndef VECTORIZED_SET_OPS_HPP
#define VECTORIZED_SET_OPS_HPP

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <x86intrin.h>
#include <immintrin.h>
#include <array>
#include <vector>
#include <memory>

class VectorizedSetOps
{
public:
    using PackState = int;

    VectorizedSetOps();

    int bitpacked_simd_intersection(int *a_base, PackState *a_state, int a_size,
                                    int *b_base, PackState *b_state, int b_size)
    {
        inter_cnt++;

        int total = 0, aligned_a = a_size & ~3, aligned_b = b_size & ~3;
        int i = 0, j = 0;
        uint64_t temp_buf[2] __attribute__((aligned(16)));
        int shorter = std::min(a_size, b_size), longer = std::max(a_size, b_size);
        if (shorter * 32 < longer)
            skew_cnt++;

        for (; i < aligned_a && j < aligned_b;)
        {
            cmp_cnt += 1;

            __m128i vec_a = _mm_lddqu_si128((__m128i *)(a_base + i));
            __m128i vec_b = _mm_lddqu_si128((__m128i *)(b_base + j));
            __m128i state_vec_a = _mm_lddqu_si128((__m128i *)(a_state + i));
            __m128i state_vec_b = _mm_lddqu_si128((__m128i *)(b_state + j));

            int max_a = a_base[i + 3];
            int max_b = b_base[j + 3];
            bool match = max_a == max_b;
            bool less = max_a < max_b;

            i += (match || less) ? 4 : 0;
            j += (match || !less) ? 4 : 0;

            auto prefetch_addr = [](void *p)
            { _mm_prefetch(reinterpret_cast<const char *>(p), _MM_HINT_NTA); };
            prefetch_addr(a_base + i), prefetch_addr(a_state + i);
            prefetch_addr(b_base + j), prefetch_addr(b_state + j);

            int mask_val = -1, probe = 0;

            __m128i chk_mask = _mm_cmpeq_epi8(
                _mm_shuffle_epi8(vec_a, group_a_order[0]),
                _mm_shuffle_epi8(vec_b, group_b_order[0]));

            mask_val = byte_mask_lookup_table[_mm_movemask_epi8(chk_mask)];

            probe = 1;
            do
            {
                __m128i alt_a = _mm_shuffle_epi8(vec_a, group_a_order[probe]);
                __m128i alt_b = _mm_shuffle_epi8(vec_b, group_b_order[probe]);
                chk_mask = _mm_and_si128(chk_mask, _mm_cmpeq_epi8(alt_a, alt_b));
                mask_val = byte_mask_lookup_table[_mm_movemask_epi8(chk_mask)];
                ++multimatch_cnt;
            } while (mask_val == -1 && probe < 4);

            if ((mask_val == -2) ? (no_match_cnt++, true) : false)
                continue;

            simd_byte_hit_count[probe]++;

            // Shuffle vec_b and state_vec_b based on lookup table
            __m128i vec_b_shuffled = _mm_shuffle_epi8(vec_b, shuffle_pattern_dict[mask_val]);
            __m128i state_b_shuffled = _mm_shuffle_epi8(state_vec_b, shuffle_pattern_dict[mask_val]);

            // Compare shuffled vec_b with vec_a
            __m128i equality_mask = _mm_cmpeq_epi32(vec_a, vec_b_shuffled);

            // AND states: shuffled state from B and original state from A
            __m128i active_state_mask = _mm_and_si128(state_vec_a, state_b_shuffled);

            // Joint state match based on equal values and active bits
            __m128i intersection_mask = _mm_and_si128(equality_mask, active_state_mask);

            // Invert zero matches to find truly valid matches
            __m128i zero_comparison = _mm_cmpeq_epi32(intersection_mask, SIMD_ZERO_VECTOR);
            __m128i final_valid = _mm_andnot_si128(zero_comparison, equality_mask);

            __m128i unused_mask = _mm_or_si128(final_valid, zero_comparison);
            __m128i y_blend = _mm_blendv_epi8(vec_b_shuffled, state_b_shuffled, equality_mask);
            volatile int re_me = _mm_extract_epi16(unused_mask, 0);
            volatile int re_me1 = _mm_extract_epi16(y_blend, 0);
            (void)re_me;
            (void)re_me1;

            // Store result
            _mm_store_si128(reinterpret_cast<__m128i *>(temp_buf), intersection_mask);

            total += (_mm_popcnt_u64(temp_buf[0]) + _mm_popcnt_u64(temp_buf[1]));
        }

        while (i < a_size && j < b_size)
        {
            int val_a = a_base[i], val_b = b_base[j];
            if (val_a == val_b)
            {
                total += _mm_popcnt_u32(a_state[i] & b_state[j]);
                i++;
                j++;
            }
            else
            {
                (val_a < val_b) ? ++i : ++j;
            }
        }

        double hit_ratio = (double)total / shorter;
        if ((hit_ratio < 0.3 || shorter < 8) && shorter * 32 >= longer)
            low_select_cnt++;

        return total;
    }

    unsigned long long get_cmp_count() const { return cmp_cnt; }
    unsigned long long get_intersection_count() const { return inter_cnt; }
    const std::array<unsigned long long, 4> &get_simd_byte_hit_counts() const { return simd_byte_hit_count; }

private:
    static constexpr int SHUFFLE_A = 0x1B;
    static constexpr int SHUFFLE_B = 0x86;
    static constexpr int SHUFFLE_C = 0xC8;

    static __m128i generate_full_mask();
    static __m128i generate_zero_vector();
    std::vector<uint8_t> generate_shuffle_table();
    int *generate_mask_lookup_table();
    uint8_t *generate_shuffle_dict();

    const __m128i SIMD_FULL_MASK;
    const __m128i SIMD_ZERO_VECTOR;
    const std::vector<uint8_t> shuffle_table;
    std::unique_ptr<int[]> byte_mask_lookup_table;
    const __m128i *shuffle_pattern_dict;

    alignas(16) std::array<uint8_t, 64> group_a_data;
    alignas(16) std::array<uint8_t, 64> group_b_data;
    const __m128i *group_a_order;
    const __m128i *group_b_order;

    unsigned long long inter_cnt = 0;
    unsigned long long cmp_cnt = 0;
    unsigned long long no_match_cnt = 0;
    unsigned long long multimatch_cnt = 0;
    unsigned long long skew_cnt = 0;
    unsigned long long low_select_cnt = 0;
    std::array<unsigned long long, 4> simd_byte_hit_count = {};
};

// Implementation
inline __m128i VectorizedSetOps::generate_full_mask()
{
    return _mm_xor_si128(_mm_setzero_si128(), _mm_cmpeq_epi32(_mm_setzero_si128(), _mm_setzero_si128()));
}

inline __m128i VectorizedSetOps::generate_zero_vector()
{
    return _mm_xor_si128(_mm_set1_epi32(0), _mm_set1_epi32(0));
}

inline std::vector<uint8_t> VectorizedSetOps::generate_shuffle_table()
{
    std::vector<uint8_t> table(256, 255);
    for (int base = 0; base < 16; base += 4)
    {
        for (int i = 0; i < 4; ++i)
        {
            table[base + i] = i;
        }
    }
    return table;
}

inline int *VectorizedSetOps::generate_mask_lookup_table()
{
    int *mask = new int[65536];

    auto trans_c_s = [](const int c) -> int
    {
        if (c == 0)
            return -1;
        else if (c == 1)
            return 0;
        else if (c == 2)
            return 1;
        else if (c == 4)
            return 2;
        else if (c == 8)
            return 3;
        else
            return 4;
    };

    int x = 0;
    while (x < 65536)
    {
        int c0 = (x & 0xf), c1 = ((x >> 4) & 0xf);
        int c2 = ((x >> 8) & 0xf), c3 = ((x >> 12) & 0xf);
        int s0 = trans_c_s(c0), s1 = trans_c_s(c1);
        int s2 = trans_c_s(c2), s3 = trans_c_s(c3);

        bool is_multiple_match = (s0 == 4 || s1 == 4 || s2 == 4 || s3 == 4);
        if (is_multiple_match)
        {
            mask[x++] = -1;
            continue;
        }

        bool is_no_match = (s0 == -1 && s1 == -1 && s2 == -1 && s3 == -1);
        if (is_no_match)
        {
            mask[x++] = -2;
            continue;
        }

        s0 = (s0 == -1) ? 0 : s0;
        s1 = (s1 == -1) ? 1 : s1;
        s2 = (s2 == -1) ? 2 : s2;
        s3 = (s3 == -1) ? 3 : s3;

        mask[x++] = (s0) | (s1 << 2) | (s2 << 4) | (s3 << 6);
    }

    return mask;
}

inline uint8_t *VectorizedSetOps::generate_shuffle_dict()
{
    uint8_t *dict = new uint8_t[4096];

    int x = 0;
    while (x < 256)
    {
        int i = 0;
        while (i < 4)
        {
            int shift_amt = i * 2;
            uint8_t c = static_cast<uint8_t>((x >> shift_amt) & 3);
            int base_offset = (x << 4) + (i << 2); // x * 16 + i * 4

            int j = 0;
            while (j < 4)
            {
                dict[base_offset + j] = static_cast<uint8_t>(c * 4 + j);
                ++j;
            }

            ++i;
        }

        ++x;
    }

    return dict;
}

inline VectorizedSetOps::VectorizedSetOps()
    : SIMD_FULL_MASK(generate_full_mask()),
      SIMD_ZERO_VECTOR(generate_zero_vector()),
      shuffle_table(generate_shuffle_table()),
      byte_mask_lookup_table(generate_mask_lookup_table()),
      shuffle_pattern_dict(reinterpret_cast<const __m128i *>(generate_shuffle_dict())),
      group_a_data{
          0, 0, 0, 0, 4, 4, 4, 4, 8, 8, 8, 8, 12, 12, 12, 12,
          1, 1, 1, 1, 5, 5, 5, 5, 9, 9, 9, 9, 13, 13, 13, 13,
          2, 2, 2, 2, 6, 6, 6, 6, 10, 10, 10, 10, 14, 14, 14, 14,
          3, 3, 3, 3, 7, 7, 7, 7, 11, 11, 11, 11, 15, 15, 15, 15},
      group_b_data{
          0, 4, 8, 12, 0, 4, 8, 12, 0, 4, 8, 12, 0, 4, 8, 12,
          1, 5, 9, 13, 1, 5, 9, 13, 1, 5, 9, 13, 1, 5, 9, 13,
          2, 6, 10, 14, 2, 6, 10, 14, 2, 6, 10, 14, 2, 6, 10, 14,
          3, 7, 11, 15, 3, 7, 11, 15, 3, 7, 11, 15, 3, 7, 11, 15},
      group_a_order(reinterpret_cast<const __m128i *>(group_a_data.data())),
      group_b_order(reinterpret_cast<const __m128i *>(group_b_data.data())) {}

#endif // VECTORIZED_SET_OPS_HPP
