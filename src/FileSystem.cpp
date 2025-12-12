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
    
    // Free existing blocks
    auto oldBlocks = inodeMgr_->getInodeBlocks(inode);
    for (uint32_t blockNum : oldBlocks) {
        disk_->freeBlock(blockNum);
    }
    
    // Reset inode block pointers
    memset(inode.directBlocks, 0, sizeof(inode.directBlocks));
    inode.indirectBlock = 0;
    inode.blockCount = 0;
    inode.fileSize = 0;
    
    // Allocate new blocks and write data
    bool result = allocateFileBlocks(inode, data);
    
    if (result) {
        inode.modifiedTime = time(nullptr);
        inodeMgr_->writeInode(static_cast<uint32_t>(inodeNum), inode);
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    double elapsed = std::chrono::duration<double, std::milli>(end - start).count();
    updateStats(false, elapsed, data.size());
    
    return result;
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

bool FileSystem::allocateFileBlocks(Inode& inode, const std::vector<uint8_t>& data) {
    inode.fileSize = data.size();
    
    if (data.empty()) {
        return true;
    }
    
    uint32_t blocksNeeded = (data.size() + BLOCK_SIZE - 1) / BLOCK_SIZE;
    std::vector<uint8_t> buffer(BLOCK_SIZE, 0);
    
    for (uint32_t i = 0; i < blocksNeeded; ++i) {
        // Allocate block
        int32_t blockNum = disk_->allocateBlock();
        if (blockNum < 0) {
            std::cerr << "Failed to allocate block" << std::endl;
            return false;
        }
        
        // Copy data to buffer
        size_t offset = i * BLOCK_SIZE;
        size_t copySize = std::min(static_cast<size_t>(BLOCK_SIZE), data.size() - offset);
        buffer.assign(BLOCK_SIZE, 0);
        std::copy(data.begin() + offset, data.begin() + offset + copySize, buffer.begin());
        
        // Write block
        if (!disk_->writeBlock(static_cast<uint32_t>(blockNum), buffer.data())) {
            return false;
        }
        
        // Add block to inode
        if (!inodeMgr_->addBlockToInode(inode, static_cast<uint32_t>(blockNum))) {
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

} // namespace FileSystemTool
