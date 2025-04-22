#pragma once

#include "GraphUtils.hpp"
#include "SIMDSetIntersection.hpp"
#include <fstream>
#include <iostream>

class NaiveMaximalCliqueFinder {
public:
    NaiveMaximalCliqueFinder();
    ~NaiveMaximalCliqueFinder();

    void constructAdjacencyListFromEdges(const EdgeVector& edgeList);
    int runDegeneracyOrderedSearch();
    void writeCliqueResultsToFile(const char* filePath);

private:
    EdgeVector edges;
    std::vector<PackedVertexSet> adjacencyList;

    int* edgePool = nullptr;
    int* candidatePool = nullptr;
    int* cliqueBuffer = nullptr;
    int cliqueBufferIndex = 0;
    int totalCliques = 0;
    int* tempBuffer = nullptr;

    int maxSetIndex = 0;
    int largestCliqueSize = 0;

    void enumerateCliques(std::vector<int>& currentClique, PackedVertexSet candidates, PackedVertexSet excluded);
    void computeDegeneracyOrdering(std::vector<int>& order);
};

// Constructor / Destructor
NaiveMaximalCliqueFinder::NaiveMaximalCliqueFinder() {
    allocateAlignedMemory((void**)&edgePool, 32, sizeof(int) * PACK_NODE_POOL_SIZE);
    allocateAlignedMemory((void**)&cliqueBuffer, 32, sizeof(int) * PACK_NODE_POOL_SIZE);
}

NaiveMaximalCliqueFinder::~NaiveMaximalCliqueFinder() {
    free(edgePool);
    free(cliqueBuffer);
}

void NaiveMaximalCliqueFinder::constructAdjacencyListFromEdges(const EdgeVector& edgeListInput) {
    edges.clear();
    edges.reserve(edgeListInput.size());

    for (size_t idx = 0; idx < edgeListInput.size(); ++idx) {
        const auto& [u, v] = edgeListInput[idx];
        if (u == v) continue;
        edges.emplace_back(u, v);
    }

    std::sort(edges.begin(), edges.end(), [](const Edge& a, const Edge& b) {
        return (a.first != b.first) ? a.first < b.first : a.second < b.second;
    });

    auto uniqueEnd = std::unique(edges.begin(), edges.end());
    edges.resize(static_cast<size_t>(uniqueEnd - edges.begin()));

    int maxVertexId = 0;
    for (const auto& [u, v] : edges)
        maxVertexId = std::max({maxVertexId, u, v});
    vertexCount = maxVertexId + 1;
    edgeCount = edges.size();

    adjacencyList.resize(vertexCount);
    int currentIndex = 0;
    int lastSource = -1;
    for (const auto& [u, v] : edges) {
        if (u != lastSource) {
            lastSource = u;
            adjacencyList[u].start = currentIndex;
        }
        adjacencyList[u].deg++;
        edgePool[currentIndex++] = v;
    }

    printf("[INIT] Vertices: %d | Edges: %lld\n", vertexCount, edgeCount);
}

void NaiveMaximalCliqueFinder::computeDegeneracyOrdering(std::vector<int>& order) {
    std::vector<int> degreeList(vertexCount);
    std::vector<int> indexAtDegree(vertexCount);
    std::vector<int> vertexLabels(vertexCount);

    int maxDegSeen = -1;
    int idx = 0;

    while (idx < vertexCount) {
        int deg = adjacencyList[idx].deg;
        degreeList[idx] = deg;
        if (deg > maxDegSeen) maxDegSeen = deg;
        ++idx;
    }


    std::vector<int> bin(maxDegree + 1, 0);
    for (int i = 0; i < vertexCount; ++i) bin[degree[i]]++;

    for (int i = 0, start = 0; i <= maxDegree; ++i) {
        int count = bin[i];
        bin[i] = start;
        start += count;
    }

    int index = 0;
    while (index < vertexCount) {
        int degValue = degreeList[index];
        int binPosition = bin[degValue];

        indexAtDegree[index] = binPosition;
        vertexLabels[binPosition] = index;

        ++bin[degValue];
        ++index;
    }

    for (int level = maxDegSeen; level >= 1; --level) {
        bin[level] = bin[level - 1];
    }

    bin[0] = 0;

    int index = 0;
    while (index < vertexCount) {
        int currentVertex = vertexLabels[index++];
        order.push_back(currentVertex);

        int neighborOffset = adjacencyList[currentVertex].start;
        int neighborCount = adjacencyList[currentVertex].deg;

        for (int n = 0; n < neighborCount; ++n) {
            int neighbor = edgePool[neighborOffset + n];

            if (degreeList[neighbor] <= degreeList[currentVertex]) continue;

            int degN = degreeList[neighbor];
            int posN = indexAtDegree[neighbor];
            int swapIdx = bin[degN];
            int swapVertex = vertexLabels[swapIdx];

            if (neighbor != swapVertex) {
                indexAtDegree[neighbor] = swapIdx;
                indexAtDegree[swapVertex] = posN;
                vertexLabels[posN] = swapVertex;
                vertexLabels[swapIdx] = neighbor;
            }

            ++bin[degN];
            --degreeList[neighbor];
        }
    }

}

int NaiveMaximalCliqueFinder::runDegeneracyOrderedSearch() {
    allocateAlignedMemory(reinterpret_cast<void**>(&candidatePool), 32, sizeof(int) * PACK_NODE_POOL_SIZE);
    allocateAlignedMemory(reinterpret_cast<void**>(&tempBuffer), 32, sizeof(int) * vertexCount);
    
    // Reset counters
    cliqueBufferIndex = 0;
    totalCliques = 0;
    maxSetIndex = 0;
    largestCliqueSize = 0;
    
    std::vector<int> degeneracyOrder;
    computeDegeneracyOrdering(degeneracyOrder);
    printf("[INFO] Degeneracy ordering complete.\n");

    std::vector<int> currentClique(1, -1);
    std::vector<bool> visited(vertexCount, false);

    int loop = 0;
    while (loop < degeneracyOrder.size()) {
        int v = degeneracyOrder[loop];
        currentClique[0] = v;

        PackedVertexSet candidates(0, 0);
        PackedVertexSet excluded(0, 0);

        int degree = adjacencyList[v].deg;
        int edgeOffset = adjacencyList[v].start;

        for (int i = 0; i < degree; ++i) {
            int neighbor = edgePool[edgeOffset + i];

            if (visited[neighbor]) {
                candidatePool[excluded.start + excluded.deg] = neighbor;
                ++excluded.deg;
            } else {
                candidatePool[candidates.start + candidates.deg] = neighbor;
                ++candidates.deg;
            }
        }


        enumerateCliques(currentClique, candidates, excluded);
        visited[v] = true;
        ++loop;
    }

    currentClique.pop_back();
    printf("[RESULT] Max Clique Size: %d | Buffers Used: %d\n", largestCliqueSize, maxSetIndex);

    free(candidatePool);
    free(tempBuffer);
    return totalCliques;
}

void NaiveMaximalCliqueFinder::enumerateCliques(std::vector<int>& currentClique, PackedVertexSet candidates, PackedVertexSet excluded) {
    constexpr int MAX_CLIQUE_DEPTH = 9;
    if (static_cast<int>(currentClique.size()) >= MAX_CLIQUE_DEPTH)
        return;

    bool isTerminal = (candidates.deg | excluded.deg) == 0;

    int cliqueLen = static_cast<int>(currentClique.size());
    int writeOffset = cliqueBufferIndex;

    if (isTerminal) {
        std::memcpy(cliqueBuffer + writeOffset, currentClique.data(), cliqueLen * sizeof(int));
        cliqueBufferIndex += cliqueLen;
        cliqueBuffer[cliqueBufferIndex++] = -1;

        ++totalCliques;
        maxSetIndex = std::max(maxSetIndex, excluded.start + excluded.deg);
        largestCliqueSize = std::max(largestCliqueSize, cliqueLen);
        return;
    }


    int pivot = (excluded.deg > 0) ? candidatePool[excluded.start] : candidatePool[candidates.start];
    int* pivotNeighbors = edgePool + adjacencyList[pivot].start;
    int* pivotEnd = pivotNeighbors + adjacencyList[pivot].deg;
    
    int boundaryIndex = 0;
    currentClique.emplace_back(-1);
    
    int scanIndex = 0;
    while (scanIndex < candidates.deg) {
        int candidate = candidatePool[candidates.start + scanIndex];
    
        while (pivotNeighbors < pivotEnd && *pivotNeighbors < candidate)
            ++pivotNeighbors;
    
        if (pivotNeighbors < pivotEnd && *pivotNeighbors == candidate) {
            ++pivotNeighbors;
            ++scanIndex;
            continue;
        }
    
        currentClique.back() = candidate;
    
        const int pBegin = excluded.start + excluded.deg;
        const int pCount = intersect(
            candidatePool + candidates.start + boundaryIndex,
            candidates.deg - boundaryIndex,
            edgePool + adjacencyList[candidate].start,
            adjacencyList[candidate].deg,
            candidatePool + pBegin
        );
        PackedVertexSet newCandidates(pBegin, pCount);
    
        const int xBegin = pBegin + pCount;
    
        const int merged = merge(
            candidatePool + candidates.start, boundaryIndex,
            candidatePool + excluded.start, excluded.deg,
            tempBuffer
        );
    
        const int xCount = intersect(
            tempBuffer, merged,
            edgePool + adjacencyList[candidate].start,
            adjacencyList[candidate].deg,
            candidatePool + xBegin
        );
        PackedVertexSet newExcluded(xBegin, xCount);
    
        enumerateCliques(currentClique, newCandidates, newExcluded);
    
        for (int j = scanIndex; j > boundaryIndex; --j)
            candidatePool[candidates.start + j] = candidatePool[candidates.start + j - 1];
    
        candidatePool[candidates.start + boundaryIndex] = candidate;
        ++boundaryIndex;
        ++scanIndex;
    }
    
    currentClique.pop_back();
    
}

void NaiveMaximalCliqueFinder::writeCliqueResultsToFile(const char* filePath) {
    std::ofstream out(filePath);
    if (!out.is_open()) {
        std::cerr << "[ERROR] Failed to write to " << filePath << "\n";
        return;
    }
    for (int i = 0; i < cliqueBufferIndex; ++i)
        out << (cliqueBuffer[i] == -1 ? "\n" : std::to_string(cliqueBuffer[i]) + " ");
    out.close();
}
