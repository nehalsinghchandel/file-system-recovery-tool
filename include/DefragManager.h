#ifndef DEFRAGMANAGER_H
#define DEFRAGMANAGER_H

#include "FileSystem.h"
#include <vector>
#include <string>

namespace FileSystemTool {

// Fragmentation statistics
struct FragmentationStats {
    double fragmentationScore;      // 0.0 (no fragmentation) to 1.0 (highly fragmented)
    uint32_t totalFiles;
    uint32_t fragmentedFiles;
    uint32_t totalFragments;
    double averageFragmentsPerFile;
    uint32_t largestContiguousRegion;
    
    FragmentationStats() : fragmentationScore(0.0), totalFiles(0), 
                          fragmentedFiles(0), totalFragments(0), 
                          averageFragmentsPerFile(0.0), largestContiguousRegion(0) {}
};

// Performance benchmark results
struct BenchmarkResults {
    double avgReadTimeMs;
    double avgWriteTimeMs;
    double avgSeekTimeMs;
    uint32_t totalOperations;
    
    BenchmarkResults() : avgReadTimeMs(0), avgWriteTimeMs(0), 
                        avgSeekTimeMs(0), totalOperations(0) {}
};

class DefragManager {
public:
    DefragManager(FileSystem* fs);
    
    // Fragmentation analysis
    FragmentationStats analyzeFragmentation();
    bool isFileFragmented(uint32_t inodeNum);
    uint32_t countFileFragments(const Inode& inode);
    
    // Defragmentation
    bool defragmentFileSystem(bool& cancelled);
    bool defragmentFile(uint32_t inodeNum);
    
    // Performance benchmarking
    BenchmarkResults runBenchmark(uint32_t numFiles = 100);
    
    // Fragmentation simulation (for testing)
    void simulateFragmentation(uint32_t numFiles = 50);
    
    // Progress callback
    using ProgressCallback = std::function<void(int progress, const std::string& message)>;
    void setProgressCallback(ProgressCallback callback) { progressCallback_ = callback; }
    
    // Get statistics
    const FragmentationStats& getLastStats() const { return lastStats_; }
    const BenchmarkResults& getBeforeDefragBenchmark() const { return beforeBenchmark_; }
    const BenchmarkResults& getAfterDefragBenchmark() const { return afterBenchmark_; }
    
private:
    FileSystem* fs_;
    FragmentationStats lastStats_;
    BenchmarkResults beforeBenchmark_;
    BenchmarkResults afterBenchmark_;
    ProgressCallback progressCallback_;
    
    // Helper functions
    bool relocateFileBlocks(Inode& inode, const std::vector<uint32_t>& newBlocks);
    std::vector<uint32_t> findContiguousBlocks(uint32_t count);
    uint32_t findFirstFreeBlock();
    void reportProgress(int progress, const std::string& message);
    
    // Benchmark helpers
    double measureReadLatency(uint32_t inodeNum);
    void createBenchmarkFiles(std::vector<uint32_t>& inodeNumbers);
    void deleteBenchmarkFiles(const std::vector<uint32_t>& inodeNumbers);
};

} // namespace FileSystemTool

#endif // DEFRAGMANAGER_H
