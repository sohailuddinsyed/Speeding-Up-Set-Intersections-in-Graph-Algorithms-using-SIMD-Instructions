#ifndef _M_HEAP_H
#define _M_HEAP_H

#include "../util.hpp"

struct MHNode
{
    int key;
    int label;
    double val;

    MHNode() {};
    MHNode(int _k, int _l, double _v) : key(_k), label(_l), val(_v) {};
};

struct UpdateInfo
{
    int pos; // pos == 0 means the node is not in heap.
    int state;
    double utd_val; // up-to-date values of each key.

    UpdateInfo() {};
    UpdateInfo(int _p, int _s, double _v) : pos(_p), state(_s), utd_val(_v) {};
};

class ModifiedHeap
{
public:
    ModifiedHeap(int max_size)
    {
        align_malloc((void **)&heap, 32, sizeof(MHNode) * (max_size + 1));
        align_malloc((void **)&up_info, 32, sizeof(UpdateInfo) * (max_size + 1));
        align_malloc((void **)&up_list, 32, sizeof(int) * (max_size + 1));
        size = max_size;
        reset_label = 0;
        up_list_cnt = 0;
        for (int i = 0; i < size; ++i)
        {
            heap[i + 1] = MHNode(i, 0, 0.0);
            up_info[i] = UpdateInfo(i + 1, 0, 0.0);
        }
    }

    ~ModifiedHeap()
    {
        free(heap);
        free(up_info);
        free(up_list);
    }

    void inc(int key, double delta = 1.0)
    {
        UpdateInfo &info = up_info[key];
        if ((info.state & RESET_LABEL_MASK) != reset_label)
        {
            info.state = reset_label;
            info.utd_val = delta;
        }
        else
            info.utd_val += delta;

        if ((info.state & UPDATE_LABEL_MASK) == 0)
        {
            up_list[up_list_cnt++] = key;
            info.state |= UPDATE_LABEL_MASK;
        }
    }

    MHNode top()
    {
        adjust();
        return heap[1];
    }

    int pop()
    {
        adjust();
        int key = heap[1].key;
        swap_mhnode(1, size);
        size--;
        up_info[key].pos = 0;
        down(1);
        return key;
    }

    void del(int key)
    {
        adjust();
        int pos = up_info[key].pos;
        if (pos == -1)
            return;
        swap_mhnode(pos, size);
        size--;
        up_info[key].pos = 0;
        if (is_zero(pos))
            down(pos);
        else
            up(pos);
    }

    void reset()
    {
        reset_label++;
        up_list_cnt = 0;
    }

    void adjust()
    {
        for (int i = 0; i < up_list_cnt; ++i)
        {
            int key = up_list[i];
            UpdateInfo &info = up_info[key];
            heap[info.pos].val = info.utd_val;
            heap[info.pos].label = reset_label;
            up(info.pos);
            info.state -= UPDATE_LABEL_MASK;
        }
        up_list_cnt = 0;
    }

    int get_size() { return size; }
    bool in_heap(int key) { return up_info[key].pos > 0; }

private:
    static const int RESET_LABEL_MASK = 0x7fffffff;
    static const int UPDATE_LABEL_MASK = 0x80000000;

    int size = 0, reset_label = 0;
    MHNode *heap;
    UpdateInfo *up_info;
    int *up_list, up_list_cnt;

    bool is_zero(int pos)
    {
        return heap[pos].label != reset_label;
    }

    void up(int pos)
    {
        while (pos >= 2 &&
               (is_zero(pos >> 1) || heap[pos].val > heap[pos >> 1].val))
        {
            swap_mhnode(pos, pos >> 1);
            pos >>= 1;
        }
    }

    void down(int pos)
    {
        if (is_zero(pos))
            heap[pos].val = 0.0;
        int j = (pos << 1);
        while (j <= size)
        {
            if (j < size)
            {
                char zero_state = is_zero(j) | (is_zero(j + 1) << 1);
                if (zero_state == 0)
                    j += (heap[j + 1].val > heap[j].val);
                else if (zero_state == 1)
                    j++;
            }

            if (is_zero(j) || heap[pos].val >= heap[j].val)
                break;
            swap_mhnode(pos, j);
            pos = j;
            j = pos << 1;
        }
    }

    void swap_mhnode(int pos_x, int pos_y)
    {
        std::swap(heap[pos_x], heap[pos_y]);
        up_info[heap[pos_x].key].pos = pos_x;
        up_info[heap[pos_y].key].pos = pos_y;
    }
};

#endif