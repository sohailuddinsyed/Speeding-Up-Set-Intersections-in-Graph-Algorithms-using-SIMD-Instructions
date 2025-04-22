#include <immintrin.h>
#include <sys/time.h>
#include <cstdio>
#include <string>
#include <algorithm>
#include "GraphUtils.hpp"
#include "SIMDMaximalCliqueFinder.hpp"
#include <filesystem>  


using namespace std;
// using namespace simd;
// Timing helpers
typedef timeval TimeStamp;
TimeStamp timeStart, timeEnd;

// Default path, can be overridden by CLI
std::string inputGraphPath = "../data/youtube_cont_GRO.txt";

SIMDMaximalCliqueFinder cliqueEngine;

// Measure time difference in milliseconds
double getElapsedTimeMs(TimeStamp start, TimeStamp end) {
    return (end.tv_sec - start.tv_sec) * 1000.0 + (end.tv_usec - start.tv_usec) / 1000.0;
}

// Graph construction wrapper
void prepareGraph(const EdgeVector& edges) {
    gettimeofday(&timeStart, nullptr);
    cliqueEngine.constructAdjacencyListFromEdges(edges);
    gettimeofday(&timeEnd, nullptr);
    printf("[INFO] Graph loaded in %.3f ms\n", getElapsedTimeMs(timeStart, timeEnd));
}

// Clique detection wrapper
int detectCliques() {
    gettimeofday(&timeStart, nullptr);
    int count = cliqueEngine.runDegeneracyOrderedSearch();
    gettimeofday(&timeEnd, nullptr);
    // printf("[INFO] Maximal cliques listed in %.3f ms\n", getElapsedTimeMs(timeStart, timeEnd));
    return count;
}

// Output wrapper
void handleOutput(int argc, char* argv[]) {
    switch (argc > 2) {
        case true:
            cliqueEngine.writeCliqueResultsToFile(argv[2]);
            break;
        default:
            puts("[INFO] No output file specified. Skipping write.");
    }
}

// Entry point
int main(int argc, char* argv[]) {
    inputGraphPath = (argc > 1) ? argv[1] : inputGraphPath;
    std::string graphFileName = std::filesystem::path(inputGraphPath).filename().string();

    EdgeVector graphEdges;
    // size_t edgeCount = 0;

    // Load edges using while instead of for
    graphEdges = loadEdgeList(inputGraphPath);
    // edgeCount = graphEdges.size();
    puts("[INFO] Edges loaded.");
    

    prepareGraph(graphEdges);

    int cliqueCount = detectCliques();

    // printf("[RESULT] File: %s\n", inputGraphPath.c_str());
    // printf("[RESULT] Maximal Cliques: %d\n", cliqueCount);
    // printf("[RESULT] Comparisons: %llu\n", cmp_cnt);
    puts("============================================");
    puts("🧠 MAXIMAL CLIQUE DETECTION REPORT 🧠");
    puts("============================================");
    printf("📄 Graph File       : %s\n", graphFileName.c_str());

    printf("🔍 Total Cliques    : %d\n", cliqueCount);
    printf("⏱️  Detection Time  : %.3f ms\n", getElapsedTimeMs(timeStart, timeEnd));
    // printf("📊 Comparisons Made : %'llu\n", cmp_cnt);  // using ' for grouped numbers (if supported)
    // std::cout << "🧠 Peak Buffer Usage (MC)  : " << cliqueEngine.trueMaxMemoryUsed
    //       << " / " << PACK_NODE_POOL_SIZE << " slots\n";
    // //   std::cout << "[DEBUG] trueMaxMemoryUsed = " << cliqueEngine.trueMaxMemoryUsed << "\n";

    // std::cout << "📊 Utilization        : " << (100.0 * cliqueEngine.trueMaxMemoryUsed / PACK_NODE_POOL_SIZE) << "%\n";

    puts("============================================");
    
    handleOutput(argc, argv);

    return EXIT_SUCCESS;
}