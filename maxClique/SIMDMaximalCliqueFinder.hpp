#pragma once

#include "GraphUtils.hpp"
#include "SIMDSetIntersection.hpp"
#include <fstream>
#include <iostream>
#include <iomanip>


// using namespace simd;
class SIMDMaximalCliqueFinder {
public:
    SIMDMaximalCliqueFinder();
    ~SIMDMaximalCliqueFinder();

    void constructAdjacencyListFromEdges(const EdgeVector& edgeList);
    int runDegeneracyOrderedSearch();
    void writeCliqueResultsToFile(const char* filePath);
    int trueMaxMemoryUsed = 0;


private:
    // Graph Data
    int vertexCount = 0;
    long long edgeCount = 0;
    int packCount = 0;
    EdgeVector edges;
    std::vector<PackedVertexSet> adjacencyList;
    std::vector<int> originalDegrees;

    // Pool Buffers
    int* poolBase = nullptr;
    PackState* poolState = nullptr;
    int* poolMC = nullptr;
    int poolMCIndex = 0;

    // Temporary Set Buffers
    int* setsBase = nullptr;
    PackState* setsState = nullptr;

    // Stats
    int maxSetBufferIndex = 0;
    int maxCliqueSize = 0;
    int cliqueCount = 0;


    // Core Algorithm
    void enumerateCliques(std::vector<int>& currentClique, PackedVertexSet candidates, PackedVertexSet excluded);
    void allocateAllBuffers();
    void releaseTempBuffers();
};

SIMDMaximalCliqueFinder::SIMDMaximalCliqueFinder() {
    allocateAlignedMemory((void**)&poolBase, 32, sizeof(int) * PACK_NODE_POOL_SIZE);
    allocateAlignedMemory((void**)&poolState, 32, sizeof(PackState) * PACK_NODE_POOL_SIZE);
    allocateAlignedMemory((void**)&poolMC, 32, sizeof(int) * PACK_NODE_POOL_SIZE);
    std::cerr << "[ALLOC] poolBase  size: " << PACK_NODE_POOL_SIZE << " × " << sizeof(int)
          << " bytes = " << (PACK_NODE_POOL_SIZE * sizeof(int)) / 1024.0 << " KB\n";
    std::cerr << "[ALLOC] poolState size: " << PACK_NODE_POOL_SIZE << " × " << sizeof(PackState)
            << " bytes = " << (PACK_NODE_POOL_SIZE * sizeof(PackState)) / 1024.0 << " KB\n";
    std::cerr << "[ALLOC] poolMC    size: " << PACK_NODE_POOL_SIZE << " × " << sizeof(int)
            << " bytes = " << (PACK_NODE_POOL_SIZE * sizeof(int)) / 1024.0 << " KB\n";

}

SIMDMaximalCliqueFinder::~SIMDMaximalCliqueFinder() {
    free(poolBase);
    free(poolState);
    free(poolMC);
}

void SIMDMaximalCliqueFinder::constructAdjacencyListFromEdges(const EdgeVector& edgeListInput) {
    edges.reserve(edgeListInput.size());
    for (size_t idx = 0; idx < edgeListInput.size(); ++idx) {
        const auto& edge = edgeListInput[idx];
        if (edge.first == edge.second) continue;
        edges.emplace_back(edge);
    }
    

    std::sort(edges.begin(), edges.end(), [](const Edge& a, const Edge& b) {
        return (a.first != b.first) ? a.first < b.first : a.second < b.second;
    });
    
    auto last = std::unique(edges.begin(), edges.end());
    edges.resize(std::distance(edges.begin(), last));
    
    for (size_t i = 0; i < edges.size(); ++i) {
        const auto& [u, v] = edges[i];
        if (u > vertexCount) vertexCount = u;
        if (v > vertexCount) vertexCount = v;
    }
    
    vertexCount++;
    edgeCount = edges.size();

    packCount = (vertexCount + PACK_WIDTH - 1) / PACK_WIDTH;
    adjacencyList.resize(vertexCount);
    originalDegrees.resize(vertexCount, 0);

    int currentIndex = -1;
    int previousSource = -1;

    for (const auto& [src, dst] : edges) {
        int base = dst >> PACK_SHIFT;
        PackState bit = (PackState(1) << (dst & PACK_MASK));
        originalDegrees[src]++;

        int caseId = (src != previousSource) ? 0 :
             (poolBase[currentIndex] == base ? 1 : 2);

        switch (caseId) {
            case 0: {  // New source vertex
                previousSource = src;
                adjacencyList[src].start = ++currentIndex;
                adjacencyList[src].deg++;
                poolBase[currentIndex] = base;
                poolState[currentIndex] = bit;
                break;
            }
            case 1: {  // Same base — merge bit
                poolState[currentIndex] |= bit;
                break;
            }
            case 2: {  // New base, same source — add new entry
                adjacencyList[src].deg++;
                poolBase[++currentIndex] = base;
                poolState[currentIndex] = bit;
                break;
            }
        }

    }

    printf("Compression Ratio: %d/%lld = %.4f\n", currentIndex + 1, edgeCount, (double)(currentIndex + 1) / edgeCount);
}

void SIMDMaximalCliqueFinder::allocateAllBuffers() {
    allocateAlignedMemory((void**)&setsBase, 32, sizeof(int) * PACK_NODE_POOL_SIZE);
    allocateAlignedMemory((void**)&setsState, 32, sizeof(PackState) * PACK_NODE_POOL_SIZE);
    std::cerr << "[ALLOC] setsBase  size: " << PACK_NODE_POOL_SIZE << " × " << sizeof(int)
          << " bytes = " << (PACK_NODE_POOL_SIZE * sizeof(int)) / 1024.0 << " KB\n";
    std::cerr << "[ALLOC] setsState size: " << PACK_NODE_POOL_SIZE << " × " << sizeof(PackState)
            << " bytes = " << (PACK_NODE_POOL_SIZE * sizeof(PackState)) / 1024.0 << " KB\n";

}

void SIMDMaximalCliqueFinder::releaseTempBuffers() {
    free(setsBase);
    free(setsState);
}

int SIMDMaximalCliqueFinder::runDegeneracyOrderedSearch() {
    allocateAllBuffers();

    maxSetBufferIndex = 0;
    maxCliqueSize = 0;
    poolMCIndex = 0;
    cliqueCount = 0;

    auto* visitedState = new PackState[packCount]();

    std::vector<int> currentClique;
    currentClique.reserve(2048);
    currentClique.push_back(-1);

    int vertexIndex = 0;
    while (vertexIndex < vertexCount) { 
        currentClique[0] = vertexIndex;

        // Compute current vertex bit position
        int vBase = vertexIndex >> PACK_SHIFT;
        PackState vBit = static_cast<PackState>(1) << (vertexIndex & PACK_MASK);

        // Load adjacency metadata
        const int adjStart = adjacencyList[vertexIndex].start;
        const int adjDeg = adjacencyList[vertexIndex].deg;

        // Build candidate set
        PackedVertexSet candidateSet(0, 0);
        candidateSet.deg = subtractVisitedSIMD(
            poolBase + adjStart,
            poolState + adjStart,
            adjDeg,
            visitedState,
            setsBase,
            setsState
        );

        // Build exclusion set (right after candidates)
        PackedVertexSet exclusionSet(candidateSet.deg, 0);
        exclusionSet.deg = subtractUnvisitedSIMD(
            poolBase + adjStart,
            poolState + adjStart,
            adjDeg,
            visitedState,
            setsBase + exclusionSet.start,
            setsState + exclusionSet.start
        );

        // Explore cliques
        enumerateCliques(currentClique, candidateSet, exclusionSet);

        // Mark vertex as visited
        visitedState[vBase] |= vBit;

        ++vertexIndex;
    }


    delete[] visitedState;
    releaseTempBuffers();

    // printf("Max Set Buffer Index Used: %d\n", maxSetBufferIndex);
    // printf("Largest Clique Size: %d\n", maxCliqueSize);
    // int totalBufferCapacity = PACK_NODE_POOL_SIZE;
    // int usedBufferSlots = maxSetBufferIndex;
    // double usagePercent = (static_cast<double>(usedBufferSlots) / totalBufferCapacity) * 100.0;

    // std::cout << "[INFO] Set Buffer Allocated : " << totalBufferCapacity << " slots\n";
    // std::cout << "[INFO] Set Buffer Used      : " << usedBufferSlots << " slots\n";
    // std::cout << "[INFO] Utilization          : " << std::fixed << std::setprecision(2) << usagePercent << "%\n";

    size_t totalBytesPerBuffer = PACK_NODE_POOL_SIZE * sizeof(int);           // poolBase, poolMC, setsBase
    size_t totalBytesPerState  = PACK_NODE_POOL_SIZE * sizeof(PackState);     // poolState, setsState

    size_t totalAllocated = 3 * totalBytesPerBuffer + 2 * totalBytesPerState;

    std::cout << "💾 Total Working Memory Allocated : "
            << (totalAllocated / 1024.0 / 1024.0) << " MB\n";

    // std::cout << "🧠 Peak Buffer Usage               : "
    //         << trueMaxMemoryUsed << " / " << PACK_NODE_POOL_SIZE << " slots\n";

    // std::cout << "📊 Buffer Utilization              : "
    //         << std::fixed << std::setprecision(4)
    //         << (100.0 * trueMaxMemoryUsed / PACK_NODE_POOL_SIZE) << " %\n";


    return cliqueCount;
}

void SIMDMaximalCliqueFinder::enumerateCliques(std::vector<int>& currentClique, PackedVertexSet candidates, PackedVertexSet excluded) {
    constexpr int MAX_CLIQUE_DEPTH = 9;
    if (static_cast<int>(currentClique.size()) >= MAX_CLIQUE_DEPTH)
    return;

    bool isTerminalClique = (candidates.deg | excluded.deg) == 0;

    int cliqueSize = static_cast<int>(currentClique.size());
    int bufferWriteIndex = poolMCIndex;

    isTerminalClique
        ? (
            std::memcpy(poolMC + bufferWriteIndex, currentClique.data(), cliqueSize * sizeof(int)),
            poolMCIndex += cliqueSize,
            poolMC[poolMCIndex++] = -1,
            ++cliqueCount,
            maxSetBufferIndex = std::max(maxSetBufferIndex, excluded.start + excluded.deg),
            maxCliqueSize = std::max(maxCliqueSize, cliqueSize),
            void()
        )
        : void();

    if (isTerminalClique) return;


    int pivot = (excluded.deg > 0)
        ? (setsBase[excluded.start] << PACK_SHIFT) | __builtin_ctz(setsState[excluded.start])
        : (setsBase[candidates.start] << PACK_SHIFT) | __builtin_ctz(setsState[candidates.start]);

    int neighborStart = adjacencyList[pivot].start;
    int neighborEnd = neighborStart + adjacencyList[pivot].deg;

    currentClique.push_back(-1);
    int index = 0;
    int baseOffset = candidates.start;

    while (index < candidates.deg) {
        const int base = setsBase[baseOffset + index];
        int state = setsState[baseOffset + index];
        const int baseShifted = base << PACK_SHIFT;

        while (neighborStart < neighborEnd && poolBase[neighborStart] < base)
            ++neighborStart;

        if (neighborStart < neighborEnd && poolBase[neighborStart] == base) {
            state &= ~poolState[neighborStart];
            ++neighborStart;
        }

        while (state != 0) {
            const int vertex = baseShifted | __builtin_ctz(state);
            state &= (state - 1);

            currentClique.back() = vertex;

            const int startP = excluded.start + excluded.deg;
            const int degP = intersectSetsSIMD(
                setsBase + baseOffset, setsState + baseOffset, candidates.deg,
                poolBase + adjacencyList[vertex].start, poolState + adjacencyList[vertex].start, adjacencyList[vertex].deg,
                setsBase + startP, setsState + startP
            );

            const int startX = startP + degP;
            int* xBaseIn = setsBase + excluded.start;
            PackState* xStateIn = setsState + excluded.start;
            int xDegIn = excluded.deg;

            int* xBaseAdj = poolBase + adjacencyList[vertex].start;
            PackState* xStateAdj = poolState + adjacencyList[vertex].start;
            int xDegAdj = adjacencyList[vertex].deg;

            int* xBaseOut = setsBase + startX;
            PackState* xStateOut = setsState + startX;

            // if (startX + adjacencyList[vertex].deg >= PACK_NODE_POOL_SIZE) {
            //     std::cerr << "[ERROR] Buffer overflow risk BEFORE intersectSetsSIMD at X-set.\n";
            //     std::exit(EXIT_FAILURE);
            // }
            

            const int degX = intersectSetsSIMD(
                xBaseIn, xStateIn, xDegIn,
                xBaseAdj, xStateAdj, xDegAdj,
                xBaseOut, xStateOut
            );

            trueMaxMemoryUsed = std::max(trueMaxMemoryUsed, startX + degX);
            
            // std::cout << "[DEBUG] Updated trueMaxMemoryUsed = " << trueMaxMemoryUsed << "\n";
            constexpr double WARNING_THRESHOLD = 0.90;
            int usage = startX + degX;

            if (usage >= static_cast<int>(WARNING_THRESHOLD * PACK_NODE_POOL_SIZE)) {
                std::cerr << "[⚠️ WARNING] Buffer usage has reached "
                        << (100.0 * usage / PACK_NODE_POOL_SIZE) << "% of PACK_NODE_POOL_SIZE (" 
                        << usage << " / " << PACK_NODE_POOL_SIZE << ").\n"
                        << "Consider increasing PACK_NODE_POOL_SIZE to avoid overflow.\n";
            }


            PackedVertexSet newP(startP, degP);
            PackedVertexSet newX(startX, degX);
            enumerateCliques(currentClique, newP, newX);

            PackState bitMask = static_cast<PackState>(1) << (vertex & PACK_MASK);
            setsState[baseOffset + index] &= ~bitMask;
            excluded.deg = (bitMask != 0)
    ? mergeVertexToSet(setsBase + excluded.start, setsState + excluded.start, excluded.deg, base, bitMask)
    : excluded.deg;

        }

        ++index;
    }

    currentClique.pop_back();
}

void SIMDMaximalCliqueFinder::writeCliqueResultsToFile(const char* filePath) {
    std::ofstream outFile(filePath);
    if (!outFile) {
        std::cerr << "Failed to open file for writing: " << filePath << "\n";
        return;
    }
    for (int i = 0; i < poolMCIndex; ++i)
        outFile << (poolMC[i] == -1 ? "\n" : std::to_string(poolMC[i]) + " ");
    outFile.close();
}
