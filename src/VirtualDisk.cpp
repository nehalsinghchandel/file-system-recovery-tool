#include "VirtualDisk.h"
#include <iostream>
#include <cstring>
#include <algorithm>

namespace FileSystemTool {

VirtualDisk::VirtualDisk(const std::string& diskPath)
    : diskPath_(diskPath) {
    memset(&superblock_, 0, sizeof(Superblock));
}

VirtualDisk::~VirtualDisk() {
    closeDisk();
}

bool VirtualDisk::createDisk(uint32_t sizeInBytes) {
    // Create a new disk file
    diskFile_.open(diskPath_, std::ios::out | std::ios::binary | std::ios::trunc);
    if (!diskFile_.is_open()) {
        std::cerr << "Failed to create disk file: " << diskPath_ << std::endl;
        return false;
    }
    
    // Write zeros to create the file of specified size
    std::vector<uint8_t> zeros(BLOCK_SIZE, 0);
    uint32_t numBlocks = sizeInBytes / BLOCK_SIZE;
    
    for (uint32_t i = 0; i < numBlocks; ++i) {
        diskFile_.write(reinterpret_cast<const char*>(zeros.data()), BLOCK_SIZE);
    }
    
    diskFile_.close();
    
    // Open for read/write (don't validate yet - we haven't written superblock)
    diskFile_.open(diskPath_, std::ios::in | std::ios::out | std::ios::binary);
    if (!diskFile_.is_open()) {
        std::cerr << "Failed to reopen disk file: " << diskPath_ << std::endl;
        return false;
    }
    
    // Initialize the superblock structure in memory
    initializeSuperblock(sizeInBytes);
    
    // Format the disk (writes superblock, bitmap, etc.)
    if (!formatDisk()) {
        diskFile_.close();
        return false;
    }
    
    // Now read bitmap into memory for normal operations
    if (!readBitmap()) {
        std::cerr << "Failed to read bitmap after format" << std::endl;
        diskFile_.close();
        return false;
    }
    
    return true;
}

bool VirtualDisk::openDisk() {
    diskFile_.open(diskPath_, std::ios::in | std::ios::out | std::ios::binary);
    if (!diskFile_.is_open()) {
        std::cerr << "Failed to open disk file: " << diskPath_ << std::endl;
        return false;
    }
    
    // Read superblock
    if (!readSuperblock()) {
        std::cerr << "Failed to read superblock" << std::endl;
        diskFile_.close();
        return false;
    }
    
    // Validate magic number
    if (superblock_.magic != MAGIC_NUMBER) {
        std::cerr << "Invalid magic number in superblock" << std::endl;
        diskFile_.close();
        return false;
    }
    
    // Read bitmap into memory
    if (!readBitmap()) {
        std::cerr << "Failed to read bitmap" << std::endl;
        diskFile_.close();
        return false;
    }
    
    return true;
}

void VirtualDisk::closeDisk() {
    if (diskFile_.is_open()) {
        writeBitmap();
        writeSuperblock();
        diskFile_.close();
    }
}

bool VirtualDisk::formatDisk() {
    // Initialize bitmap (all blocks free except system blocks)
    bitmap_.clear();
    bitmap_.resize(superblock_.totalBlocks, true);  // All free initially
    
    // Mark system blocks as used
    uint32_t systemBlocks = superblock_.dataBlocksStart;
    for (uint32_t i = 0; i < systemBlocks; ++i) {
        bitmap_[i] = false;  // Mark as used
    }
    
    // Update superblock
    superblock_.freeBlocks = superblock_.totalBlocks - systemBlocks;
    superblock_.cleanShutdown = 1;
    
    // Write superblock and bitmap
    if (!writeSuperblock() || !writeBitmap()) {
        return false;
    }
    
    // Initialize inode table (all inodes free)
    std::vector<uint8_t> zeros(BLOCK_SIZE, 0);
    uint32_t inodeBlocks = superblock_.dataBlocksStart - superblock_.inodeTableStart;
    
    for (uint32_t i = 0; i < inodeBlocks; ++i) {
        if (!writeBlock(superblock_.inodeTableStart + i, zeros.data())) {
            return false;
        }
    }
    
    // Initialize journal
    uint32_t journalBlocks = superblock_.journalSize;
    for (uint32_t i = 0; i < journalBlocks; ++i) {
        if (!writeBlock(superblock_.journalStart + i, zeros.data())) {
            return false;
        }
    }
    
    return true;
}

bool VirtualDisk::readBlock(uint32_t blockNum, uint8_t* buffer) {
    if (blockNum >= superblock_.totalBlocks) {
        std::cerr << "Block number out of range: " << blockNum << std::endl;
        return false;
    }
    
    diskFile_.seekg(blockNum * BLOCK_SIZE, std::ios::beg);
    diskFile_.read(reinterpret_cast<char*>(buffer), BLOCK_SIZE);
    
    return diskFile_.good();
}

bool VirtualDisk::writeBlock(uint32_t blockNum, const uint8_t* buffer) {
    if (blockNum >= superblock_.totalBlocks) {
        std::cerr << "Block number out of range: " << blockNum << std::endl;
        return false;
    }
    
    diskFile_.seekp(blockNum * BLOCK_SIZE, std::ios::beg);
    diskFile_.write(reinterpret_cast<const char*>(buffer), BLOCK_SIZE);
    diskFile_.flush();
    
    return diskFile_.good();
}

int32_t VirtualDisk::allocateBlock() {
    // Find first free block in data region
    for (uint32_t i = superblock_.dataBlocksStart; i < superblock_.totalBlocks; ++i) {
        if (bitmap_[i]) {  // Block is free
            bitmap_[i] = false;  // Mark as used
            superblock_.freeBlocks--;
            return static_cast<int32_t>(i);
        }
    }
    
    std::cerr << "No free blocks available" << std::endl;
    return -1;  // No free blocks
}

bool VirtualDisk::freeBlock(uint32_t blockNum) {
    if (blockNum >= superblock_.totalBlocks) {
        return false;
    }
    
    if (blockNum < superblock_.dataBlocksStart) {
        std::cerr << "Cannot free system block: " << blockNum << std::endl;
        return false;
    }
    
    if (!bitmap_[blockNum]) {  // Block is used
        bitmap_[blockNum] = true;  // Mark as free
        superblock_.freeBlocks++;
        
        // Zero out the block for security
        std::vector<uint8_t> zeros(BLOCK_SIZE, 0);
        writeBlock(blockNum, zeros.data());
        
        return true;
    }
    
    return false;  // Block was already free
}

bool VirtualDisk::isBlockFree(uint32_t blockNum) {
    if (blockNum >= superblock_.totalBlocks) {
        return false;
    }
    return bitmap_[blockNum];
}

bool VirtualDisk::readSuperblock() {
    diskFile_.seekg(0, std::ios::beg);
    diskFile_.read(reinterpret_cast<char*>(&superblock_), sizeof(Superblock));
    return diskFile_.good();
}

bool VirtualDisk::writeSuperblock() {
    diskFile_.seekp(0, std::ios::beg);
    diskFile_.write(reinterpret_cast<const char*>(&superblock_), sizeof(Superblock));
    diskFile_.flush();
    return diskFile_.good();
}

bool VirtualDisk::readBitmap() {
    uint32_t bitmapBlocks = calculateBitmapBlocks();
    std::vector<uint8_t> buffer(BLOCK_SIZE);
    
    bitmap_.clear();
    bitmap_.reserve(superblock_.totalBlocks);
    
    for (uint32_t i = 0; i < bitmapBlocks; ++i) {
        if (!readBlock(superblock_.bitmapStart + i, buffer.data())) {
            return false;
        }
        
        // Convert bytes to bits
        for (uint32_t j = 0; j < BLOCK_SIZE && bitmap_.size() < superblock_.totalBlocks; ++j) {
            for (int bit = 0; bit < 8 && bitmap_.size() < superblock_.totalBlocks; ++bit) {
                bitmap_.push_back((buffer[j] & (1 << bit)) != 0);
            }
        }
    }
    
    return true;
}

bool VirtualDisk::writeBitmap() {
    uint32_t bitmapBlocks = calculateBitmapBlocks();
    std::vector<uint8_t> buffer(BLOCK_SIZE, 0);
    
    size_t bitIndex = 0;
    for (uint32_t i = 0; i < bitmapBlocks; ++i) {
        buffer.assign(BLOCK_SIZE, 0);
        
        // Convert bits to bytes
        for (uint32_t j = 0; j < BLOCK_SIZE && bitIndex < bitmap_.size(); ++j) {
            uint8_t byte = 0;
            for (int bit = 0; bit < 8 && bitIndex < bitmap_.size(); ++bit) {
                if (bitmap_[bitIndex]) {
                    byte |= (1 << bit);
                }
                bitIndex++;
            }
            buffer[j] = byte;
        }
        
        if (!writeBlock(superblock_.bitmapStart + i, buffer.data())) {
            return false;
        }
    }
    
    return true;
}

void VirtualDisk::markClean() {
    superblock_.cleanShutdown = 1;
    writeSuperblock();
}

void VirtualDisk::markDirty() {
    superblock_.cleanShutdown = 0;
    writeSuperblock();
}

void VirtualDisk::initializeSuperblock(uint32_t diskSize) {
    superblock_.magic = MAGIC_NUMBER;
    superblock_.totalBlocks = diskSize / BLOCK_SIZE;
    superblock_.blockSize = BLOCK_SIZE;
    superblock_.inodeCount = superblock_.totalBlocks / 8;  // ~12.5% for inodes
    superblock_.freeInodes = superblock_.inodeCount;
    
    // Calculate layout
    superblock_.bitmapStart = 1;  // Block 0 is superblock
    superblock_.inodeTableStart = superblock_.bitmapStart + calculateBitmapBlocks();
    superblock_.journalStart = superblock_.inodeTableStart + calculateInodeBlocks();
    superblock_.journalSize = 64;  // 64 blocks for journal (~256KB)
    superblock_.dataBlocksStart = superblock_.journalStart + superblock_.journalSize;
    
    superblock_.freeBlocks = superblock_.totalBlocks - superblock_.dataBlocksStart;
    superblock_.cleanShutdown = 1;
}

uint32_t VirtualDisk::calculateBitmapBlocks() const {
    // Each block can hold 8 * BLOCK_SIZE bits
    uint32_t bitsPerBlock = BLOCK_SIZE * 8;
    return (superblock_.totalBlocks + bitsPerBlock - 1) / bitsPerBlock;
}

uint32_t VirtualDisk::calculateInodeBlocks() const {
    // Each inode is INODE_SIZE bytes
    uint32_t inodesPerBlock = BLOCK_SIZE / 128;  // INODE_SIZE = 128
    return (superblock_.inodeCount + inodesPerBlock - 1) / inodesPerBlock;
}

} // namespace FileSystemTool
