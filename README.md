
# SIMD-Accelerated Triangle Counting & Graph Reordering

This repository contains two core components:
1. **TC** - A SIMD-optimized triangle counting module.
2. **GRO** - A graph reordering optimizer to improve compression and cache locality.

---

## 🔧 Project Structure

```plaintext
.
├── Dataset/                        # Input datasets (edge list format)
│   ├── amazon_cont.txt
│   ├── dblp.sym.txt
│   └── youtube_cont.txt
│
├── TC/                            # Triangle Counting project
│   ├── triangle_exec_driver.cpp   # Main program
│   ├── include/
│   │   ├── simd_layout_counter.hpp
│   │   └── vectorized_set_operations.hpp
│   └── Makefile
│
├── GRO/                           # Graph Reordering Optimizer
│   ├── gro_driver.cpp             # Main optimizer code
│   ├── include/
│   │   ├── layout_optimizer.hpp
│   │   ├── node_cache.hpp
│   │   └── score_queue.hpp
│   └── Makefile
│
├── MC/                            # Maximal Clique Enumeration project
│   ├── MainCliqueDetection.cpp    # Main detection engine
│   ├── include/
│   │   ├── SIMDMaximalCliqueFinder.hpp
│   │   ├── NaiveMaximalCliqueFinder.hpp
│   │   ├── SIMDSetIntersection.hpp
│   │   └── GraphUtils.hpp
│   └── Makefile

---

## ⚙️ Requirements

- C++11 or higher
- AVX2 and SSSE3 CPU support
- GNU Make
- g++

---

## 🚀 How to Build

### 1. Build Triangle Counting

```bash
cd TC
make
./triangle_exec_driver ../Dataset/youtube_cont.txt
```

### 2. Build Graph Reordering

```bash
cd GRO
make
./layoutopt ../Dataset/youtube_cont.txt
```

### 2. Build Maximal Clique Detection

```bash
cd MC
make
./MainCliqueDetection ../Dataset/youtube_cont.txt
```
---

## 📊 Output Format

### Triangle Counting Output

```
=========== Triangle Counting Summary ===========
Input Graph Path              : <file>
Vertex Table Size             : <int>
Edge Count (Forward Edges)    : <int>
Compression Ratio             : <float>
Graph Build Time (ms)         : <float>
Triangle Count                : <int>
Triangle Count Time (ms)      : <float>
Total SIMD Comparisons        : <int>
==================================================
```

### Graph Reordering Output

```
=== Layout Optimization Report ===
Metric                   Value
---------------------------------------------
Input File               <name>
Initialization Time (ms) <float>
Optimization Time        <float>
Compaction Ratio         <float>
Layout Score             <float>
Normalized Score         <float>
Save Time (ms)           <float>
---------------------------------------------
```

### Maximal Clique Detection Output

```
============================================
🧠 MAXIMAL CLIQUE DETECTION REPORT 🧠
============================================
📊 Individual Buffer Size(s)    : <float> KB
📊 Compression Ratio         : <float>
⏱️ Graph Loading Time       : <float> ms
🧠 Working Memory Allocated  : <float> MB
📄 Graph File                : <graph_file>
🔍 Total Cliques             : <int>
⏱️ Detection Time           : <float> ms
============================================
```
---

## 🧠 Algorithm Notes

- Uses SIMD intrinsics (`<immintrin.h>`) for set intersections in triangle counting.
- Graph reordering uses greedy heuristics with score-based priority queues for layout optimization.

---

## 🖥️ System Info (AWS)

| Spec        | Value                                  |
|-------------|----------------------------------------|
| CPU         | Intel(R) Xeon(R) E5-2686 v4 @ 2.30GHz  |
| RAM         | 957 MiB (Swap: 9 GiB)                  |
| Storage     | 28G (Used: 22G, Available: 6.1G)       |
| OS          | Ubuntu (Xen Hypervisor)                |

---

## 📊 Sample Results

### ▶️ Layout Optimization Output

```
$ ./layoutopt ../Dataset/dblp.sym.txt
....................

=== Layout Optimization Report ===
Metric                   Value               
---------------------------------------------
Input File               ../Dataset/dblp.sym
Initialization Time (ms) 136.323
Optimization Time        1537.430ms
Compaction Ratio         0.996
Layout Score             11.719
Normalized Score         4.912
Save Time (ms)           226.077
---------------------------------------------
```

### ▶️ Triangle Counting Before and After Layout Optimization

```
$ ./triangle_exec_driver ../Dataset/dblp.sym.txt

=========== Triangle Counting Summary ===========
Input Graph Path              : ../Dataset/dblp.sym.txt
Vertex Table Size             : 950991
Edge Count (Forward Edges)    : 966380
Compression Ratio             : 0.9841
Graph Build Time (ms)         : 92.000
Triangle Count                : 1790126
Triangle Count Time (ms)      : 55.000
Total SIMD Comparisons        : 1075818
==================================================
```

```
$ ./triangle_exec_driver ../Dataset/dblp.sym_layout.txt

=========== Triangle Counting Summary ===========
Input Graph Path              : ../Dataset/dblp.sym_layout.txt
Vertex Table Size             : 1048729
Edge Count (Forward Edges)    : 940171
Compression Ratio             : 1.1155
Graph Build Time (ms)         : 215.000
Triangle Count                : 1590974
Triangle Count Time (ms)      : 60.000
Total SIMD Comparisons        : 1062230
==================================================
```
