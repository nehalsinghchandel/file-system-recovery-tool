#include "FileSystem.h"
#include <iostream>
#include <chrono>
#include <algorithm>

namespace FileSystemTool {

FileSystem::FileSystem(const std::string& diskPath)
    : diskPath_(diskPath), mounted_(false) {
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
    
    // Allocate and write new blocks, tracking ownership
    if (!allocateFileBlocks(static_cast<uint32_t>(fileInode), inode, data)) {
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

bool FileSystem::allocateFileBlocks(uint32_t inodeNum, Inode& inode, const std::vector<uint8_t>& data) {
    uint32_t blocksNeeded = (data.size() + BLOCK_SIZE - 1) / BLOCK_SIZE;
    
    if (blocksNeeded == 0) return true;
    
    // Allocate direct blocks
    uint32_t directBlocksToUse = std::min(blocksNeeded, 12u);
    for (uint32_t i = 0; i < directBlocksToUse; ++i) {
        int32_t blockNum = disk_->allocateBlock();
        if (blockNum < 0) {
            return false;
        }
        
        inode.directBlocks[i] = blockNum;
        
        // Track ownership
        setBlockOwner(static_cast<uint32_t>(blockNum), inodeNum);
        
        // Write data to block
        uint32_t offset = i * BLOCK_SIZE;
        uint32_t bytesToWrite = std::min(BLOCK_SIZE, static_cast<uint32_t>(data.size() - offset));
        
        std::vector<uint8_t> blockData(BLOCK_SIZE, 0);
        std::copy(data.begin() + offset, 
                 data.begin() + offset + bytesToWrite,
                 blockData.begin());
        
        if (!disk_->writeBlock(blockNum, blockData.data())) {
            return false;
        }
    }
    
    // Handle indirect blocks if needed
    if (blocksNeeded > 12) {
        int32_t indirectBlock = disk_->allocateBlock();
        if (indirectBlock < 0) return false;
        
        inode.indirectBlock = indirectBlock;
        std::vector<int32_t> indirectPointers(BLOCK_SIZE / sizeof(int32_t), -1);
        
        uint32_t indirectBlocksNeeded = blocksNeeded - 12;
        for (uint32_t i = 0; i < indirectBlocksNeeded; ++i) {
            int32_t blockNum = disk_->allocateBlock();
            if (blockNum < 0) return false;
            
            indirectPointers[i] = blockNum;
            
            // Track ownership
            setBlockOwner(static_cast<uint32_t>(blockNum), inodeNum);
            
            // Write data
            uint32_t offset = (12 + i) * BLOCK_SIZE;
            uint32_t bytesToWrite = std::min(BLOCK_SIZE, static_cast<uint32_t>(data.size() - offset));
            
            std::vector<uint8_t> blockData(BLOCK_SIZE, 0);
            std::copy(data.begin() + offset,
                     data.begin() + offset + bytesToWrite,
                     blockData.begin());
            
            if (!disk_->writeBlock(blockNum, blockData.data())) {
                return false;
            }
        }
        
        if (!disk_->writeBlock(indirectBlock, 
                              reinterpret_cast<const uint8_t*>(indirectPointers.data()))) {
            return false;
        }
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
            if (inode.directBlocks[j] > 0 && 
                inode.directBlocks[j] < (int32_t)sb.totalBlocks) {
                setBlockOwner(static_cast<uint32_t>(inode.directBlocks[j]), i);
            }
        }
        
        // Process indirect block if valid
        if (inode.indirectBlock > 0 && 
            inode.indirectBlock < (int32_t)sb.totalBlocks) {
            
            std::vector<uint8_t> indirectData(BLOCK_SIZE);
            if (disk_->readBlock(inode.indirectBlock, indirectData.data())) {
                const int32_t* pointers = reinterpret_cast<const int32_t*>(indirectData.data());
                int maxIndirect = BLOCK_SIZE / sizeof(int32_t);
                
                for (int j = 0; j < maxIndirect; ++j) {
                    if (pointers[j] > 0 && 
                        pointers[j] < (int32_t)sb.totalBlocks) {
                        setBlockOwner(static_cast<uint32_t>(pointers[j]), i);
                    }
                }
            }
        }
    }
}

} // namespace FileSystemTool
