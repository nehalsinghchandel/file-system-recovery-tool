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
    const auto& sb = fs_->getDisk()->getSuperblock();
    std::vector<uint32_t> blocks;
    
    // Collect VALID direct blocks only (skip -1, 0, out of range)
    for (int i = 0; i < 12; ++i) {
        if (inode.directBlocks[i] > 0 &&
            inode.directBlocks[i] != -1 &&
            inode.directBlocks[i] < (int32_t)sb.totalBlocks) {
            blocks.push_back(static_cast<uint32_t>(inode.directBlocks[i]));
        }
    }
    
    // TODO: Add indirect blocks if needed (with validation)
    
    if (blocks.size() <= 1) {
        return blocks.size();
    }
    
    // Sort to find contiguous sequences
    std::sort(blocks.begin(), blocks.end());
    
    // Count fragments (non-contiguous sequences)
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
    
    // STEP 1: Collect ALL files and their data
    struct FileData {
        uint32_t inodeNum;
        Inode inode;
        std::vector<uint8_t> data;
        std::vector<uint32_t> oldBlocks;
    };
    
    std::vector<FileData> allFiles;
    
    for (uint32_t i = 0; i < sb.inodeCount && !cancelled; ++i) {
        Inode inode;
        if (!fs_->getInodeManager()->readInode(i, inode)) {
            continue;
        }
        
        if (inode.isValid() && inode.fileType == FileType::REGULAR_FILE && inode.fileSize > 0) {
            FileData fd;
            fd.inodeNum = i;
            fd.inode = inode;
            fd.data.resize(inode.fileSize);
            
            // Collect old blocks
            for (int j = 0; j < 12; ++j) {
                if (inode.directBlocks[j] > 0 &&
                    inode.directBlocks[j] != -1 &&
                    inode.directBlocks[j] < (int32_t)sb.totalBlocks) {
                    fd.oldBlocks.push_back(static_cast<uint32_t>(inode.directBlocks[j]));
                }
            }
            
            // Read file data from old blocks
            size_t bytesRead = 0;
            for (uint32_t blockNum : fd.oldBlocks) {
                std::vector<uint8_t> blockData(BLOCK_SIZE);
                if (fs_->getDisk()->readBlock(blockNum, blockData.data())) {
                    size_t bytesToCopy = std::min((size_t)BLOCK_SIZE, fd.data.size() - bytesRead);
                    std::copy(blockData.begin(), blockData.begin() + bytesToCopy,
                             fd.data.begin() + bytesRead);
                    bytesRead += bytesToCopy;
                }
            }
            
            allFiles.push_back(fd);
        }
    }
    
    // STEP 2: FREE ALL old blocks
    for (auto& fd : allFiles) {
        for (uint32_t blockNum : fd.oldBlocks) {
            fs_->getDisk()->freeBlock(blockNum);
            fs_->clearBlockOwner(blockNum);
        }
        
        // Clear inode blocks
        for (int i = 0; i < 12; ++i) {
            fd.inode.directBlocks[i] = -1;
        }
        fd.inode.indirectBlock = -1;
        fd.inode.blockCount = 0;
    }
    
    // STEP 3: Reallocate ALL files from scratch (guarantees contiguous allocation)
    uint32_t filesDefragged = 0;
    for (auto& fd : allFiles) {
        uint32_t blocksNeeded = (fd.data.size() + BLOCK_SIZE - 1) / BLOCK_SIZE;
        std::vector<uint32_t> newBlocks;
        
        // Allocate blocks - should be contiguous since we freed everything
        for (uint32_t i = 0; i < blocksNeeded; ++i) {
            int32_t blockNum = fs_->getDisk()->allocateBlockCompact();
            if (blockNum < 0) {
                std::cerr << "ERROR: Failed to allocate block during defrag!" << std::endl;
                break;
            }
            newBlocks.push_back(static_cast<uint32_t>(blockNum));
            fs_->setBlockOwner(static_cast<uint32_t>(blockNum), fd.inodeNum);
        }
        
        // Write data to new blocks
        for (size_t i = 0; i < newBlocks.size(); ++i) {
            size_t offset = i * BLOCK_SIZE;
            size_t copySize = std::min((size_t)BLOCK_SIZE, fd.data.size() - offset);
            
            std::vector<uint8_t> blockBuffer(BLOCK_SIZE, 0);
            std::copy(fd.data.begin() + offset, fd.data.begin() + offset + copySize, blockBuffer.begin());
            
            fs_->getDisk()->writeBlock(newBlocks[i], blockBuffer.data());
        }
        
        // Update inode
        for (size_t i = 0; i < newBlocks.size() && i < 12; ++i) {
            fd.inode.directBlocks[i] = newBlocks[i];
            fd.inode.blockCount++;
        }
        
        fs_->getInodeManager()->writeInode(fd.inodeNum, fd.inode);
        filesDefragged++;
        
        std::cout << "Defragmented file inode " << fd.inodeNum 
                  << " (" << newBlocks.size() << " blocks)" << std::endl;
    }
    
    // STEP 4: Write changes to disk
    fs_->getDisk()->writeBitmap();
    fs_->getDisk()->writeSuperblock();
    
    // Run benchmark after
    afterBenchmark_ = runBenchmark(50);
    
    std::cout << "Defragmentation complete!" << std::endl;
    std::cout << "Files defragmented: " << filesDefragged << std::endl;
    std::cout << "Read latency improvement: " 
              << (beforeBenchmark_.avgReadTimeMs - afterBenchmark_.avgReadTimeMs) << " ms" << std::endl;
    
    return true;
}

bool DefragManager::defragmentFile(uint32_t inodeNum) {
    Inode inode;
    if (!fs_->getInodeManager()->readInode(inodeNum, inode)) {
        return false;
    }
    
    // Read file data first
    std::vector<uint8_t> fileData(inode.fileSize);
    const auto& sb = fs_->getDisk()->getSuperblock();
    
    // Collect valid blocks and read data
    std::vector<uint32_t> oldBlocks;
    for (int i = 0; i < 12; ++i) {
        if (inode.directBlocks[i] > 0 &&
            inode.directBlocks[i] != -1 &&
            inode.directBlocks[i] < (int32_t)sb.totalBlocks) {
            oldBlocks.push_back(static_cast<uint32_t>(inode.directBlocks[i]));
        }
    }
    
    // Read  file data from old blocks
    size_t bytesRead = 0;
    for (uint32_t blockNum : oldBlocks) {
        std::vector<uint8_t> blockData(BLOCK_SIZE);
        if (fs_->getDisk()->readBlock(blockNum, blockData.data())) {
            size_t bytesToCopy = std::min((size_t)BLOCK_SIZE, fileData.size() - bytesRead);
            std::copy(blockData.begin(), blockData.begin() + bytesToCopy,
                     fileData.begin() + bytesRead);
            bytesRead += bytesToCopy;
        }
    }
    
    // Free old blocks
    for (uint32_t blockNum : oldBlocks) {
        fs_->getDisk()->freeBlock(blockNum);
        fs_->clearBlockOwner(blockNum);
    }
    
    // Clear inode blocks
    for (int i = 0; i < 12; ++i) {
        inode.directBlocks[i] = -1;
    }
    inode.indirectBlock = -1;

    inode.blockCount = 0;
    
    // Allocate new CONTIGUOUS blocks starting from lowest available
    std::vector<uint32_t> newBlocks;
    uint32_t blocksNeeded = oldBlocks.size();
    
    for (uint32_t i = 0; i < blocksNeeded; ++i) {
        int32_t blockNum = fs_->getDisk()->allocateBlockCompact();
        if (blockNum < 0) {
            // Allocation failed - skip this file
            return false;
        }
        newBlocks.push_back(static_cast<uint32_t>(blockNum));
        fs_->setBlockOwner(static_cast<uint32_t>(blockNum), inodeNum);
    }
    
    // VERIFY blocks are contiguous
    bool isContiguous = true;
    for (size_t i = 1; i < newBlocks.size(); ++i) {
        if (newBlocks[i] != newBlocks[i-1] + 1) {
            isContiguous = false;
            std::cerr << "WARNING: Allocated blocks not contiguous for inode " << inodeNum << std::endl;
            break;
        }
    }
    
    // Write data to new sequential blocks
    std::vector<uint8_t> blockBuffer(BLOCK_SIZE);
    for (size_t i = 0; i < newBlocks.size(); ++i) {
        size_t offset = i * BLOCK_SIZE;
        size_t copySize = std::min(static_cast<size_t>(BLOCK_SIZE), fileData.size() - offset);
        
        blockBuffer.assign(BLOCK_SIZE, 0);
        std::copy(fileData.begin() + offset, fileData.begin() + offset + copySize, blockBuffer.begin());
        
        if (!fs_->getDisk()->writeBlock(newBlocks[i], blockBuffer.data())) {
            return false;
        }
    }
    
    // Update inode with new CONTIGUOUS blocks
    for (size_t i = 0; i < newBlocks.size() && i < 12; ++i) {
        inode.directBlocks[i] = newBlocks[i];
        inode.blockCount++;
    }
    
    // Handle indirect blocks if file is large (>12 blocks)
    if (newBlocks.size() > 12) {
        // TODO: Handle indirect blocks
        std::cerr << "WARNING: File larger than 12 blocks not fully supported in defrag" << std::endl;
    }
    
    // Write updated inode
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
