#include "FileSystem.h"
#include <iostream>
#include <cstring>
#include <chrono>
#include <algorithm>
#include <set>

namespace FileSystemTool {

FileSystem::FileSystem(const std::string& diskPath)
    : diskPath_(diskPath), mounted_(false), hasCorruption_(false), activeWriteInodeNum_(UINT32_MAX) {
    memset(&stats_, 0, sizeof(PerformanceStats));
}

FileSystem::~FileSystem() {
    if (mounted_) {
        unmountFileSystem();
    }
}

bool FileSystem::createFileSystem(uint32_t diskSize) {
    disk_ = std::make_unique<VirtualDisk>(diskPath_);
    
    if (!disk_->createDisk(diskSize)) {
        std::cerr << "Failed to create virtual disk" << std::endl;
        return false;
    }
    
    // Create managers
    inodeMgr_ = std::make_unique<InodeManager>(disk_.get());
    dirMgr_ = std::make_unique<DirectoryManager>(disk_.get(), inodeMgr_.get());
    
    // Initialize root directory
    if (!dirMgr_->initializeRootDirectory()) {
        std::cerr << "Failed to initialize root directory" << std::endl;
        return false;
    }
    
    disk_->markClean();
    mounted_ = true;
    
    std::cout << "File system created successfully" << std::endl;
    return true;
}

bool FileSystem::mountFileSystem() {
    if (mounted_) {
        std::cerr << "File system already mounted" << std::endl;
        return false;
    }
    
    disk_ = std::make_unique<VirtualDisk>(diskPath_);
    
    if (!disk_->openDisk()) {
        std::cerr << "Failed to open virtual disk" << std::endl;
        return false;
    }
    
    // Create managers
    inodeMgr_ = std::make_unique<InodeManager>(disk_.get());
    dirMgr_ = std::make_unique<DirectoryManager>(disk_.get(), inodeMgr_.get());
    
    // Check for unclean shutdown
    if (!disk_->wasCleanShutdown()) {
        std::cout << "Warning: File system was not cleanly unmounted" << std::endl;
        std::cout << "Recovery may be needed" << std::endl;
    }
    
    disk_->markDirty();  // Mark as mounted
    mounted_ = true;
    
    // DO NOT rebuild block ownership - causes crashes from uninitialized blocks
    
    std::cout << "File system mounted successfully" << std::endl;
    return true;
}

bool FileSystem::unmountFileSystem() {
    if (!mounted_) {
        return false;
    }
    
    disk_->markClean();
    disk_->closeDisk();
    
    disk_.reset();
    inodeMgr_.reset();
    dirMgr_.reset();
    
    mounted_ = false;
    
    std::cout << "File system unmounted successfully" << std::endl;
    return true;
}

double FileSystem::getFragmentationScore() {
    if (!mounted_) return 0.0;
    
    int totalFragments = 0;
    int totalFiles = 0;
    
    const auto& sb = disk_->getSuperblock();
    
    // Iterate through all inodes
    for (uint32_t i = 0; i < sb.inodeCount; ++i) {
        Inode inode;
        if (!inodeMgr_->readInode(i, inode)) continue;
        
        if (!inode.isValid() || inode.fileType != FileType::REGULAR_FILE) continue;
        if (inode.fileSize == 0) continue;
        
        totalFiles++;
        
        // Get blocks for this file
        std::vector<uint32_t> blocks;
        
        // Collect direct blocks
        for (int j = 0; j < 12; ++j) {
            if (inode.directBlocks[j] > 0 &&
                inode.directBlocks[j] < (int32_t)sb.totalBlocks) {
                blocks.push_back(static_cast<uint32_t>(inode.directBlocks[j]));
            }
        }
        
        // Collect indirect blocks
        if (inode.indirectBlock > 0 &&
            inode.indirectBlock < (int32_t)sb.totalBlocks) {
            std::vector<uint8_t> indirectData(BLOCK_SIZE);
            if (disk_->readBlock(inode.indirectBlock, indirectData.data())) {
                const int32_t* pointers = reinterpret_cast<const int32_t*>(indirectData.data());
                int maxIndirect = BLOCK_SIZE / sizeof(int32_t);
                
                for (int j = 0; j < maxIndirect; ++j) {
                    if (pointers[j] > 0 &&
                        pointers[j] < (int32_t)sb.totalBlocks) {
                        blocks.push_back(static_cast<uint32_t>(pointers[j]));
                    }
                }
            }
        }
        
        // Count fragments (number of non-contiguous sequences)
        if (blocks.size() > 1) {
            std::sort(blocks.begin(), blocks.end());
            int fragments = 1;  // At least 1 fragment
            
            for (size_t j = 1; j < blocks.size(); ++j) {
                if (blocks[j] != blocks[j-1] + 1) {
                    fragments++;  // New fragment when blocks aren't contiguous
                }
            }
            
            totalFragments += fragments;
        } else if (blocks.size() == 1) {
            totalFragments += 1;  // Single block = 1 fragment
        }
    }
    
    if (totalFiles == 0) return 0.0;
    
    // Average fragments per file
    double avgFragments = static_cast<double>(totalFragments) / totalFiles;
    
    // Fragmentation score: 0% = 1 fragment/file (perfect), 100% = highly fragmented
    // Score = (avgFragments - 1) * 20, capped at 100%
    double score = std::min(100.0, (avgFragments - 1.0) * 20.0);
    
    return std::max(0.0, score);
}

bool FileSystem::createFile(const std::string& path) {
    if (!mounted_) return false;
    
    // Split path into directory and filename
    size_t lastSlash = path.find_last_of('/');
    std::string dirPath = (lastSlash != std::string::npos) ? path.substr(0, lastSlash) : "/";
    std::string filename = (lastSlash != std::string::npos) ? path.substr(lastSlash + 1) : path;
    
    if (filename.empty()) {
        std::cerr << "Invalid filename" << std::endl;
        return false;
    }
    
    // Resolve directory path
    int32_t dirInode = dirMgr_->resolvePath(dirPath, 0);
    if (dirInode < 0) {
        std::cerr << "Directory not found: " << dirPath << std::endl;
        return false;
    }
    
    // Check if file already exists
    if (dirMgr_->lookupEntry(static_cast<uint32_t>(dirInode), filename) >= 0) {
        std::cerr << "File already exists: " << path << std::endl;
        return false;
    }
    
    // Allocate inode for file
    int32_t fileInode = inodeMgr_->allocateInode(FileType::REGULAR_FILE);
    if (fileInode < 0) {
        return false;
    }
    
    // Add to directory
    return dirMgr_->addEntry(static_cast<uint32_t>(dirInode), filename, 
                            static_cast<uint32_t>(fileInode), FileType::REGULAR_FILE);
}

bool FileSystem::deleteFile(const std::string& path) {
    if (!mounted_) return false;
    
    // Split path
    size_t lastSlash = path.find_last_of('/');
    std::string dirPath = (lastSlash != std::string::npos) ? path.substr(0, lastSlash) : "/";
    std::string filename = (lastSlash != std::string::npos) ? path.substr(lastSlash + 1) : path;
    
    // Resolve directory
    int32_t dirInode = dirMgr_->resolvePath(dirPath, 0);
    if (dirInode < 0) {
        return false;
    }
    
    // Lookup file
    int32_t fileInode = dirMgr_->lookupEntry(static_cast<uint32_t>(dirInode), filename);
    if (fileInode < 0) {
        std::cerr << "File not found: " << path << std::endl;
        return false;
    }
    
    // Free inode (and its blocks)
    if (!inodeMgr_->freeInode(static_cast<uint32_t>(fileInode))) {
        return false;
    }
    
    // Remove from directory
    return dirMgr_->removeEntry(static_cast<uint32_t>(dirInode), filename);
}

bool FileSystem::readFile(const std::string& path, std::vector<uint8_t>& data) {
    if (!mounted_) return false;
    
    auto start = std::chrono::high_resolution_clock::now();
    
    // Resolve file path
    int32_t inodeNum = dirMgr_->resolvePath(path, 0);
    if (inodeNum < 0) {
        std::cerr << "File not found: " << path << std::endl;
        return false;
    }
    
    // Read inode
    Inode inode;
    if (!inodeMgr_->readInode(static_cast<uint32_t>(inodeNum), inode)) {
        return false;
    }
    
    if (inode.fileType != FileType::REGULAR_FILE) {
        std::cerr << "Not a regular file: " << path << std::endl;
        return false;
    }
    
    // Read file data
    bool result = readFileData(inode, data);
    
    auto end = std::chrono::high_resolution_clock::now();
    double elapsed = std::chrono::duration<double, std::milli>(end - start).count();
    updateStats(true, elapsed, data.size());
    
    return result;
}

bool FileSystem::writeFile(const std::string& path, const std::vector<uint8_t>& data) {
    if (!mounted_) return false;
    
    auto start = std::chrono::high_resolution_clock::now();
    
    // Resolve path
    std::string dirPath = "/";
    std::string fileName = path;
    if (path.find('/') != std::string::npos) {
        size_t lastSlash = path.rfind('/');
        dirPath = path.substr(0, lastSlash);
        if (dirPath.empty()) dirPath = "/";
        fileName = path.substr(lastSlash + 1);
    }
    
    int32_t dirInode = 0; // Root
    int32_t fileInode = dirMgr_->lookupEntry(dirInode, fileName);
    
    if (fileInode < 0) {
        return false;
    }
    
    // Read existing inode
    Inode inode;
    if (!inodeMgr_->readInode(static_cast<uint32_t>(fileInode), inode)) {
        return false;
    }
    
    // Clear ownership of old blocks before freeing them
    auto oldBlocks = inodeMgr_->getInodeBlocks(inode);
    for (uint32_t blockNum : oldBlocks) {
        clearBlockOwner(blockNum);
        disk_->freeBlock(blockNum);
    }
    
    // Reset inode block pointers
    for (int i = 0; i < 12; ++i) {
        inode.directBlocks[i] = -1;
    }
    inode.indirectBlock = -1;
    
    //  Allocate and write new blocks, tracking ownership
    uint32_t blocksNeeded = (data.size() + BLOCK_SIZE - 1) / BLOCK_SIZE;
    if (!allocateFileBlocks(inode, blocksNeeded, static_cast<uint32_t>(fileInode))) {
        return false;
    }
    
    // Update file size and write inode
    inode.fileSize = data.size();
    if (!inodeMgr_->writeInode(static_cast<uint32_t>(fileInode), inode)) {
        return false;
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    double timeMs = std::chrono::duration<double, std::milli>(end - start).count();
    updateStats(false, timeMs, data.size());
    
    return true;
}

bool FileSystem::fileExists(const std::string& path) {
    if (!mounted_) return false;
    return dirMgr_->resolvePath(path, 0) >= 0;
}

bool FileSystem::createDir(const std::string& path) {
    if (!mounted_) return false;
    
    size_t lastSlash = path.find_last_of('/');
    std::string parentPath = (lastSlash != std::string::npos) ? path.substr(0, lastSlash) : "/";
    std::string dirname = (lastSlash != std::string::npos) ? path.substr(lastSlash + 1) : path;
    
    int32_t parentInode = dirMgr_->resolvePath(parentPath, 0);
    if (parentInode < 0) {
        std::cerr << "Parent directory not found: " << parentPath << std::endl;
        return false;
    }
    
    uint32_t newInode;
    return dirMgr_->createDirectory(dirname, static_cast<uint32_t>(parentInode), newInode);
}

std::vector<DirectoryEntry> FileSystem::listDir(const std::string& path) {
    if (!mounted_) return {};
    
    int32_t inodeNum = dirMgr_->resolvePath(path, 0);
    if (inodeNum < 0) {
        return {};
    }
    
    return dirMgr_->listDirectory(static_cast<uint32_t>(inodeNum));
}

bool FileSystem::getFileInfo(const std::string& path, Inode& info) {
    if (!mounted_) return false;
    
    int32_t inodeNum = dirMgr_->resolvePath(path, 0);
    if (inodeNum < 0) {
        return false;
    }
    
    return inodeMgr_->readInode(static_cast<uint32_t>(inodeNum), info);
}

uint32_t FileSystem::getTotalBlocks() const {
    return disk_ ? disk_->getTotalBlocks() : 0;
}

uint32_t FileSystem::getFreeBlocks() const {
    return disk_ ? disk_->getFreeBlocks() : 0;
}

uint32_t FileSystem::getUsedBlocks() const {
    if (!disk_) return 0;
    return disk_->getTotalBlocks() - disk_->getFreeBlocks();
}

void FileSystem::resetStats() {
    memset(&stats_, 0, sizeof(PerformanceStats));
}


bool FileSystem::allocateFileBlocks(Inode& inode, uint32_t blocksNeeded, uint32_t inodeNum) {
    if (blocksNeeded == 0) return true;
    
    // Allocate direct blocks
    uint32_t directBlocksToUse = std::min(blocksNeeded, 12u);
    for (uint32_t i = 0; i < directBlocksToUse; ++i) {
        int32_t blockNum = disk_->allocateBlock();
        if (blockNum < 0) {
            return false;
        }
        
        inode.directBlocks[i] = blockNum;
        inode.blockCount++;
        
        // Track ownership
        setBlockOwner(static_cast<uint32_t>(blockNum), inodeNum);
    }
    
    // Handle indirect blocks if needed (files > 48KB)
    if (blocksNeeded > 12) {
        // Allocate indirect block
        int32_t indirectBlockNum = disk_->allocateBlock();
        if (indirectBlockNum < 0) {
            return false;
        }
        inode.indirectBlock = indirectBlockNum;
        
        // CRITICAL FIX: Set ownership for indirect block itself
        setBlockOwner(static_cast<uint32_t>(indirectBlockNum), inodeNum);
        
        // Allocate data blocks referenced by indirect block
        std::vector<uint8_t> indirectData(BLOCK_SIZE, 0);
        int32_t* pointers = reinterpret_cast<int32_t*>(indirectData.data());
        
        uint32_t indirectBlocksNeeded = blocksNeeded - 12;
        for (uint32_t i = 0; i < indirectBlocksNeeded; ++i) {
            int32_t blockNum = disk_->allocateBlock();
            if (blockNum < 0) {
                return false;
            }
            pointers[i] = blockNum;
            inode.blockCount++;
            setBlockOwner(static_cast<uint32_t>(blockNum), inodeNum);
        }
        
        // Write indirect block
        disk_->writeBlock(indirectBlockNum, indirectData.data());
    }
    
    return true;
}

bool FileSystem::readFileData(const Inode& inode, std::vector<uint8_t>& data) {
    data.clear();
    data.reserve(inode.fileSize);
    
    auto blocks = inodeMgr_->getInodeBlocks(inode);
    std::vector<uint8_t> buffer(BLOCK_SIZE);
    
    for (size_t i = 0; i < blocks.size(); ++i) {
        if (!disk_->readBlock(blocks[i], buffer.data())) {
            return false;
        }
        
        // Copy relevant bytes
        size_t bytesToCopy = std::min(static_cast<size_t>(BLOCK_SIZE), 
                                      static_cast<size_t>(inode.fileSize) - data.size());
        data.insert(data.end(), buffer.begin(), buffer.begin() + bytesToCopy);
    }
    
    return true;
}

void FileSystem::updateStats(bool isRead, double timeMs, uint64_t bytes) {
    if (isRead) {
        stats_.lastReadTimeMs = timeMs;
        stats_.totalBytesRead += bytes;
        stats_.totalReads++;
    } else {
        stats_.lastWriteTimeMs = timeMs;
        stats_.totalBytesWritten += bytes;
        stats_.totalWrites++;
    }
}

// Block ownership tracking implementation
void FileSystem::setBlockOwner(uint32_t blockNum, uint32_t inodeNum) {
    blockOwners_[blockNum] = inodeNum;
}

void FileSystem::clearBlockOwner(uint32_t blockNum) {
    blockOwners_.erase(blockNum);
}

uint32_t FileSystem::getBlockOwner(uint32_t blockNum) const {
    auto it = blockOwners_.find(blockNum);
    if (it != blockOwners_.end()) {
        return it->second;
    }
    return UINT32_MAX;  // No owner
}

std::string FileSystem::getFilenameFromInode(uint32_t inodeNum) const {
    if (!mounted_) return "";
    
    // Search root directory for this inode
    auto entries = dirMgr_->listDirectory(0);  // Root inode is 0
    for (const auto& entry : entries) {
        if (entry.inodeNumber == inodeNum) {
            return "/" + entry.getName();
        }
    }
    return "";
}

void FileSystem::rebuildBlockOwnership() {
    blockOwners_.clear();
    
    if (!mounted_) return;
    
    const auto& sb = disk_->getSuperblock();
    
    // Iterate through all inodes
    for (uint32_t i = 0; i < sb.inodeCount; ++i) {
        Inode inode;
        if (!inodeMgr_->readInode(i, inode)) continue;
        
        if (!inode.isValid() || inode.fileType != FileType::REGULAR_FILE) continue;
        
        // Manually get blocks to avoid invalid block access
        // Process direct blocks
        for (int j = 0; j < 12; ++j) {
            // CRITICAL: Skip uninitialized blocks (-1 casts to UINT32_MAX)
            if (inode.directBlocks[j] > 0 && 
                inode.directBlocks[j] != -1 &&
                inode.directBlocks[j] < (int32_t)sb.totalBlocks) {
                setBlockOwner(static_cast<uint32_t>(inode.directBlocks[j]), i);
            }
        }
        
        // Process indirect block if valid
        if (inode.indirectBlock > 0 && 
            inode.indirectBlock != -1 &&
            inode.indirectBlock < (int32_t)sb.totalBlocks) {
            
            std::vector<uint8_t> indirectData(BLOCK_SIZE);
            if (disk_->readBlock(inode.indirectBlock, indirectData.data())) {
                const int32_t* pointers = reinterpret_cast<const int32_t*>(indirectData.data());
                int maxIndirect = BLOCK_SIZE / sizeof(int32_t);
                
                for (int j = 0; j < maxIndirect; ++j) {
                    // CRITICAL: Skip uninitialized blocks
                    if (pointers[j] > 0 && 
                        pointers[j] != -1 &&
                        pointers[j] < (int32_t)sb.totalBlocks) {
                        setBlockOwner(static_cast<uint32_t>(pointers[j]), i);
                    }
                }
            }
        }
    }
}

void FileSystem::simulatePowerCut() {
    std::cout << "[POWER CUT] Simulating power failure!" << std::endl;
    
    hasCorruption_ = true;
    corruptedBlocks_.clear();
    
    // Find blocks belonging to the most recently written file
    const auto& sb = disk_->getSuperblock();
    
    // Find the last modified file
    uint32_t lastModifiedInode = UINT32_MAX;
    time_t latestTime = 0;
    
    for (uint32_t i = 0; i < sb.inodeCount; ++i) {
        Inode inode;
        if (inodeMgr_->readInode(i, inode) && inode.isValid()) {
            if (inode.modifiedTime > latestTime) {
                latestTime = inode.modifiedTime;
                lastModifiedInode = i;
            }
        }
    }
    
    if (lastModifiedInode != UINT32_MAX) {
        Inode inode;
        if (inodeMgr_->readInode(lastModifiedInode, inode)) {
            // Collect blocks from this file
            for (int i = 0; i < 12; ++i) {
                if (inode.directBlocks[i] > 0 &&
                    inode.directBlocks[i] != -1 &&
                    inode.directBlocks[i] < (int32_t)sb.totalBlocks) {
                    corruptedBlocks_.push_back(static_cast<uint32_t>(inode.directBlocks[i]));
                }
            }
            
            activeWriteInodeNum_ = lastModifiedInode;
            
            std::cout << "[CORRUPTION] Found " << corruptedBlocks_.size() 
                      << " orphaned blocks from inode " << lastModifiedInode << std::endl;
            
            // Corrupt the inode (mark as partially invalid but keep blocks allocated)
            inode.fileSize = 0;  // Simulate incomplete write
            inodeMgr_->writeInode(lastModifiedInode, inode);
        }
    }
    
    hasCorruption_ = true;
    activeWriteInodeNum_ = UINT32_MAX;  // Mark that we don't know which file yet
    
    std::cout << "[CRASH] Power cut simulated! " << corruptedBlocks_.size() 
              << " blocks are now corrupted" << std::endl;
}

bool FileSystem::simulatePowerCutDuringWrite(const std::string& filename,
                                               const std::vector<uint8_t>& fullData,
                                               double crashPercent) {
    if (!mounted_) return false;
    
    std::cout << "[POWER CUT] Starting file write simulation..." << std::endl;
    std::cout << "[POWER CUT] File: " << filename << ", Size: " << fullData.size() << " bytes" << std::endl;
    
    // Calculate how much to write before crash
    size_t crashPoint = static_cast<size_t>(fullData.size() * crashPercent);
    uint32_t blocksToWrite = (crashPoint + BLOCK_SIZE - 1) / BLOCK_SIZE;
    uint32_t totalBlocks = (fullData.size() + BLOCK_SIZE - 1) / BLOCK_SIZE;
    
    std::cout << "[POWER CUT] Will crash at " << (crashPercent * 100) << "% (" 
              << crashPoint << " bytes, " << blocksToWrite << "/" << totalBlocks << " blocks)" << std::endl;
    
    // 1. Create the file
    if (!createFile(filename)) {
        std::cerr << "[ERROR] Failed to create file for power cut simulation" << std::endl;
        return false;
    }
    
    // 2. Get the inode number
    int32_t inodeNum = dirMgr_->resolvePath(filename, 0);
    if (inodeNum < 0) {
        std::cerr << "[ERROR] Failed to resolve new file inode" << std::endl;
        return false;
    }
    
    Inode inode;
    if (!inodeMgr_->readInode(static_cast<uint32_t>(inodeNum), inode)) {
        std::cerr << "[ERROR] Failed to read inode" << std::endl;
        return false;
    }
    
    // 3. Allocate blocks for partial write
    std::vector<uint32_t> allocatedBlocks;
    for (uint32_t i = 0; i < blocksToWrite && i < 12; ++i) {
        int32_t blockNum = disk_->allocateBlock();
        if (blockNum < 0) {
            std::cerr << "[ERROR] Failed to allocate block " << i << std::endl;
            return false;
        }
        
        inode.directBlocks[i] = static_cast<uint32_t>(blockNum);
        allocatedBlocks.push_back(static_cast<uint32_t>(blockNum));
        setBlockOwner(static_cast<uint32_t>(blockNum), static_cast<uint32_t>(inodeNum));
        
        // Write the data to this block
        size_t offset = i * BLOCK_SIZE;
        size_t bytesToWrite = std::min(static_cast<size_t>(BLOCK_SIZE), fullData.size() - offset);
        std::vector<uint8_t> blockData(fullData.begin() + offset, 
                                        fullData.begin() + offset + bytesToWrite);
        blockData.resize(BLOCK_SIZE, 0);  // Pad to block size
        
        disk_->writeBlock(static_cast<uint32_t>(blockNum), blockData.data());
        
        std::cout << "[POWER CUT] Wrote block " << (i+1) << "/" << blocksToWrite 
                  << " (block #" << blockNum << ")" << std::endl;
    }
    
    // 4. Update inode with partial data (this creates the inconsistency)
    inode.fileSize = crashPoint;  // File size shows partial write
    inode.blockCount = blocksToWrite;
    inodeMgr_->writeInode(static_cast<uint32_t>(inodeNum), inode);
    
    // 5. Write bitmap to persist allocated blocks
    disk_->writeBitmap();
    
    // 6. SIMULATE CRASH: Mark all written blocks as corrupted
    std::cout << "[POWER CUT] ⚡ CRASH! Marking " << allocatedBlocks.size() << " blocks as corrupted..." << std::endl;
    
    corruptedBlocks_ = allocatedBlocks;
    hasCorruption_ = true;
    activeWriteInodeNum_ = static_cast<uint32_t>(inodeNum);
    
    std::cout << "[POWER CUT] File system is now in CORRUPTED state" << std::endl;
    std::cout << "[POWER CUT] Corrupted blocks: ";
    for (uint32_t blockNum : corruptedBlocks_) {
        std::cout << blockNum << " ";
    }
    std::cout << std::endl;
    
    return true;
}

bool FileSystem::runRecovery() {
    if (!hasCorruption_) {
        std::cout << "[RECOVERY] No corruption detected" << std::endl;
        return true;
    }
    
    std::cout << "[RECOVERY] Starting recovery process..." << std::endl;
    std::cout << "[RECOVERY] Found " << corruptedBlocks_.size() << " corrupted blocks" << std::endl;
    
    // Step 2: Free the corrupted blocks from bitmap
    for (uint32_t blockNum : corruptedBlocks_) {
        disk_->freeBlock(blockNum);
        std::cout << "[RECOVERY] Freed corrupted block " << blockNum << std::endl;
    }
    
    // Step 3: Find which inode(s) reference these corrupted blocks
    std::set<uint32_t> affectedInodes;
    
    for (uint32_t blockNum : corruptedBlocks_) {
        // Check all inodes to see which one references this block
        for (uint32_t inodeNum = 1; inodeNum < 128; ++inodeNum) {  // Start from 1, skip inode 0 (root dir)
            Inode inode;
            if (inodeMgr_->readInode(inodeNum, inode) && inode.isValid()) {
                // Check if this inode references the corrupted block
                for (int i = 0; i < 12 && inode.directBlocks[i] != 0; ++i) {
                    if (inode.directBlocks[i] == blockNum) {
                        affectedInodes.insert(inodeNum);
                        break;
                    }
                }
            }
        }
    }
    
    // Step 4: Clear only the affected inodes (corrupted files)
    for (uint32_t inodeNum : affectedInodes) {
        if (inodeNum == 0) {
            std::cout << "[RECOVERY] WARNING: Skipping inode 0 (root directory)" << std::endl;
            continue;  // Never delete the root directory
        }
        
        Inode inode;
        if (inodeMgr_->readInode(inodeNum, inode)) {
            // Remove from directory first
            auto entries = listDir("/");
            for (const auto& entry : entries) {
                if (entry.inodeNumber == inodeNum) {
                    // Remove this entry from root directory
                    dirMgr_->removeEntry(0, entry.getName());  // 0 = root inode
                    break;
                }
            }
            
            // Free all blocks referenced by this inode
            for (uint32_t i = 0; i < 12 && inode.directBlocks[i] != 0; ++i) {
                // Only free if not already freed (not in corrupted list)
                bool alreadyFreed = (std::find(corruptedBlocks_.begin(), corruptedBlocks_.end(), 
                                              inode.directBlocks[i]) != corruptedBlocks_.end());
                if (!alreadyFreed) {
                    disk_->freeBlock(inode.directBlocks[i]);
                }
            }
            
            // Clear the inode
            inodeMgr_->freeInode(inodeNum);
            std::cout << "[RECOVERY] Cleared corrupted inode " << inodeNum << std::endl;
        }
    }
    
    // Persist changes
    disk_->writeBitmap();
    disk_->writeSuperblock();
    
    // Clear the corruption state
    hasCorruption_ = false;
    corruptedBlocks_.clear();
    activeWriteInodeNum_ = 0;
    
    std::cout << "[RECOVERY] Recovery complete! File system is now consistent." << std::endl;
    return true;
}

} // namespace FileSystemTool
