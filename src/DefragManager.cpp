#include "DefragManager.h"
#include <iostream>
#include <algorithm>
#include <chrono>
#include <random>

namespace FileSystemTool {

DefragManager::DefragManager(FileSystem* fs) : fs_(fs) {}

FragmentationStats DefragManager::analyzeFragmentation() {
    FragmentationStats stats;
    const auto& sb = fs_->getDisk()->getSuperblock();
    
    uint32_t totalFragments = 0;
    
    // Analyze each file
    for (uint32_t i = 0; i < sb.inodeCount; ++i) {
        Inode inode;
        if (!fs_->getInodeManager()->readInode(i, inode)) {
            continue;
        }
        
        if (inode.isValid() && inode.fileType == FileType::REGULAR_FILE) {
            stats.totalFiles++;
            
            uint32_t fragments = countFileFragments(inode);
            if (fragments > 1) {
                stats.fragmentedFiles++;
                totalFragments += fragments;
            }
        }
    }
    
    if (stats.totalFiles > 0) {
        stats.totalFragments = totalFragments;
        stats.averageFragmentsPerFile = static_cast<double>(totalFragments) / stats.totalFiles;
        stats.fragmentationScore = static_cast<double>(stats.fragmentedFiles) / stats.totalFiles;
    }
    
    // Find largest contiguous region
    auto bitmap = fs_->getDisk()->getBitmap();
    uint32_t currentRegion = 0;
    stats.largestContiguousRegion = 0;
    
    for (size_t i = sb.dataBlocksStart; i < bitmap.size(); ++i) {
        if (bitmap[i]) {  // Free block
            currentRegion++;
            stats.largestContiguousRegion = std::max(stats.largestContiguousRegion, currentRegion);
        } else {
            currentRegion = 0;
        }
    }
    
    lastStats_ = stats;
    return stats;
}

bool DefragManager::isFileFragmented(uint32_t inodeNum) {
    Inode inode;
    if (!fs_->getInodeManager()->readInode(inodeNum, inode)) {
        return false;
    }
    
    return countFileFragments(inode) > 1;
}

uint32_t DefragManager::countFileFragments(const Inode& inode) {
    auto blocks = fs_->getInodeManager()->getInodeBlocks(inode);
    
    if (blocks.size() <= 1) {
        return blocks.size();
    }
    
    // Count how many times blocks are non-contiguous
    uint32_t fragments = 1;
    for (size_t i = 1; i < blocks.size(); ++i) {
        if (blocks[i] != blocks[i-1] + 1) {
            fragments++;
        }
    }
    
    return fragments;
}

bool DefragManager::defragmentFileSystem(bool& cancelled) {
    std::cout << "Starting defragmentation..." << std::endl;
    
    // Run benchmark before
    beforeBenchmark_ = runBenchmark(50);
    
    const auto& sb = fs_->getDisk()->getSuperblock();
    uint32_t filesProcessed = 0;
    uint32_t filesDefragged = 0;
    
    // Process each file
    for (uint32_t i = 0; i < sb.inodeCount && !cancelled; ++i) {
        Inode inode;
        if (!fs_->getInodeManager()->readInode(i, inode)) {
            continue;
        }
        
        if (inode.isValid() && inode.fileType == FileType::REGULAR_FILE) {
            filesProcessed++;
            
            if (isFileFragmented(i)) {
                if (defragmentFile(i)) {
                    filesDefragged++;
                }
            }
            
            // Report progress
            int progress = (filesProcessed * 100) / sb.inodeCount;
            reportProgress(progress, "Defragmenting files...");
        }
    }
    
    // CRITICAL: Write bitmap and superblock to disk to persist changes
    fs_->getDisk()->writeBitmap();
    fs_->getDisk()->writeSuperblock();
    
    // Run benchmark after
    if (!cancelled) {
        afterBenchmark_ = runBenchmark(50);
        
        std::cout << "Defragmentation complete!" << std::endl;
        std::cout << "Files defragmented: " << filesDefragged << std::endl;
        std::cout << "Read latency improvement: " 
                  << (beforeBenchmark_.avgReadTimeMs - afterBenchmark_.avgReadTimeMs) << " ms" << std::endl;
    }
    
    return !cancelled;
}

bool DefragManager::defragmentFile(uint32_t inodeNum) {
    Inode inode;
    if (!fs_->getInodeManager()->readInode(inodeNum, inode)) {
        return false;
    }
    
    if (inode.blockCount == 0) {
        return true;
    }
    
    // Read file data
    auto blocks = fs_->getInodeManager()->getInodeBlocks(inode);
    std::vector<uint8_t> fileData;
    std::vector<uint8_t> blockBuffer(BLOCK_SIZE);
    
    for (uint32_t blockNum : blocks) {
        if (!fs_->getDisk()->readBlock(blockNum, blockBuffer.data())) {
            return false;
        }
        
        size_t bytesToCopy = std::min(static_cast<size_t>(BLOCK_SIZE), 
                                      static_cast<size_t>(inode.fileSize - fileData.size()));
        fileData.insert(fileData.end(), blockBuffer.begin(), blockBuffer.begin() + bytesToCopy);
    }
    
    // Free old blocks FIRST - this creates free space at their locations
    for (uint32_t blockNum : blocks) {
        fs_->getDisk()->freeBlock(blockNum);
    }
    
    // Reset inode
    memset(inode.directBlocks, 0, sizeof(inode.directBlocks));
    inode.indirectBlock = 0;
    inode.blockCount = 0;
    
    // Allocate new blocks using COMPACT allocation (from lowest available)
    // This ensures all files get sequential blocks
    std::vector<uint32_t> newBlocks;
    for (uint32_t i = 0; i < blocks.size(); ++i) {
        int32_t blockNum = fs_->getDisk()->allocateBlockCompact();
        if (blockNum < 0) {
            // Allocation failed - restore old blocks
            for (uint32_t oldBlock : blocks) {
                fs_->getDisk()->allocateBlock();  // Re-allocate to mark as used
            }
            return false;
        }
        newBlocks.push_back(static_cast<uint32_t>(blockNum));
    }
    
    // Write data to new sequential blocks
    for (size_t i = 0; i < newBlocks.size(); ++i) {
        size_t offset = i * BLOCK_SIZE;
        size_t copySize = std::min(static_cast<size_t>(BLOCK_SIZE), fileData.size() - offset);
        
        blockBuffer.assign(BLOCK_SIZE, 0);
        std::copy(fileData.begin() + offset, fileData.begin() + offset + copySize, blockBuffer.begin());
        
        if (!fs_->getDisk()->writeBlock(newBlocks[i], blockBuffer.data())) {
            return false;
        }
        
        fs_->getInodeManager()->addBlockToInode(inode, newBlocks[i]);
    }
    
    // Update inode
    return fs_->getInodeManager()->writeInode(inodeNum, inode);
}

BenchmarkResults DefragManager::runBenchmark(uint32_t numFiles) {
    BenchmarkResults results;
    std::vector<uint32_t> testInodes;
    
    // Find existing files to benchmark
    const auto& sb = fs_->getDisk()->getSuperblock();
    for (uint32_t i = 0; i < sb.inodeCount && testInodes.size() < numFiles; ++i) {
        Inode inode;
        if (fs_->getInodeManager()->readInode(i, inode) && 
            inode.isValid() && inode.fileType == FileType::REGULAR_FILE) {
            testInodes.push_back(i);
        }
    }
    
    if (testInodes.empty()) {
        return results;
    }
    
    // Measure read latency
    double totalLatency = 0;
    for (uint32_t inodeNum : testInodes) {
        double latency = measureReadLatency(inodeNum);
        totalLatency += latency;
    }
    
    results.avgReadTimeMs = totalLatency / testInodes.size();
    results.totalOperations = testInodes.size();
    
    return results;
}

void DefragManager::simulateFragmentation(uint32_t numFiles) {
    std::cout << "Simulating fragmentation with " << numFiles << " files..." << std::endl;
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> sizeDist(1024, 16384);  // 1KB to 16KB files
    
    // Create files
    for (uint32_t i = 0; i < numFiles; ++i) {
        std::string filename = "/testfile_" + std::to_string(i) + ".dat";
        fs_->createFile(filename);
        
        // Generate random data
        size_t size = sizeDist(gen);
        std::vector<uint8_t> data(size);
        for (auto& byte : data) {
            byte = static_cast<uint8_t>(gen());
        }
        
        fs_->writeFile(filename, data);
    }
    
    // Delete every other file to create gaps
    for (uint32_t i = 0; i < numFiles; i += 2) {
        std::string filename = "/testfile_" + std::to_string(i) + ".dat";
        fs_->deleteFile(filename);
    }
    
    // Create more files that will fragment
    for (uint32_t i = numFiles; i < numFiles * 1.5; ++i) {
        std::string filename = "/fragmented_" + std::to_string(i) + ".dat";
        fs_->createFile(filename);
        
        size_t size = sizeDist(gen);
        std::vector<uint8_t> data(size);
        for (auto& byte : data) {
            byte = static_cast<uint8_t>(gen());
        }
        
        fs_->writeFile(filename, data);
    }
    
    std::cout << "Fragmentation simulation complete" << std::endl;
}

std::vector<uint32_t> DefragManager::findContiguousBlocks(uint32_t count) {
    auto bitmap = fs_->getDisk()->getBitmap();
    const auto& sb = fs_->getDisk()->getSuperblock();
    
    std::vector<uint32_t> blocks;
    
    for (size_t i = sb.dataBlocksStart; i < bitmap.size(); ++i) {
        if (bitmap[i]) {  // Free block
            blocks.push_back(i);
            
            if (blocks.size() == count) {
                // Allocate all blocks
                for (uint32_t blockNum : blocks) {
                    fs_->getDisk()->allocateBlock();  // Will find these same blocks
                }
                return blocks;
            }
        } else {
            blocks.clear();  // Not contiguous, restart
        }
    }
    
    return {};  // Couldn't find enough contiguous blocks
}

double DefragManager::measureReadLatency(uint32_t inodeNum) {
    auto start = std::chrono::high_resolution_clock::now();
    
    Inode inode;
    if (!fs_->getInodeManager()->readInode(inodeNum, inode)) {
        return 0;
    }
    
    auto blocks = fs_->getInodeManager()->getInodeBlocks(inode);
    std::vector<uint8_t> buffer(BLOCK_SIZE);
    
    for (uint32_t blockNum : blocks) {
        fs_->getDisk()->readBlock(blockNum, buffer.data());
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    return std::chrono::duration<double, std::milli>(end - start).count();
}

void DefragManager::reportProgress(int progress, const std::string& message) {
    if (progressCallback_) {
        progressCallback_(progress, message);
    }
}

} // namespace FileSystemTool
