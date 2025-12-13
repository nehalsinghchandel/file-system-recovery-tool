#ifndef VIRTUALDISK_H
#define VIRTUALDISK_H

#include <string>
#include <cstdint>
#include <vector>
#include <fstream>

namespace FileSystemTool {

// Constants
constexpr uint32_t BLOCK_SIZE = 4096;           // 4KB blocks
constexpr uint32_t DEFAULT_DISK_SIZE = 104857600; // 100MB
constexpr uint32_t MAGIC_NUMBER = 0xF5757357;   // Magic number for validation

// Superblock structure - stores disk metadata
struct Superblock {
    uint32_t magic;              // Magic number for validation
    uint32_t totalBlocks;        // Total number of blocks
    uint32_t freeBlocks;         // Number of free blocks
    uint32_t blockSize;          // Size of each block
    uint32_t inodeCount;         // Total number of inodes
    uint32_t freeInodes;         // Number of free inodes
    uint32_t bitmapStart;        // Block number where bitmap starts
    uint32_t inodeTableStart;    // Block number where inode table starts
    uint32_t dataBlocksStart;    // Block number where data blocks start
    uint32_t journalStart;       // Block number where journal starts
    uint32_t journalSize;        // Number of journal blocks
    uint8_t  cleanShutdown;      // 1 if clean shutdown, 0 if crashed
    uint8_t  padding[43];        // Padding to align to 64 bytes
};

class VirtualDisk {
public:
    VirtualDisk(const std::string& diskPath);
    ~VirtualDisk();

    // Disk lifecycle
    bool createDisk(uint32_t sizeInBytes = DEFAULT_DISK_SIZE);
    bool openDisk();
    void closeDisk();
    bool formatDisk();
    
    // Block operations
    bool readBlock(uint32_t blockNum, uint8_t* buffer);
    bool writeBlock(uint32_t blockNum, const uint8_t* buffer);
    
    // Block allocation
    int32_t allocateBlock();  // First-fit allocation (fast)
    int32_t allocateBlockCompact();  // Allocate from lowest block (for defrag)
    bool freeBlock(uint32_t blockNum);
    bool isBlockFree(uint32_t blockNum);
    
    // Superblock operations
    bool readSuperblock();
    bool writeSuperblock();
    const Superblock& getSuperblock() const { return superblock_; }
    
    // Bitmap operations
    bool readBitmap();
    bool writeBitmap();
    std::vector<bool> getBitmap() const { return bitmap_; }
    
    // Status
    bool isOpen() const { return diskFile_.is_open(); }
    uint32_t getTotalBlocks() const { return superblock_.totalBlocks; }
    uint32_t getFreeBlocks() const { return superblock_.freeBlocks; }
    
    // Mark clean/dirty shutdown
    void markClean();
    void markDirty();
    bool wasCleanShutdown() const { return superblock_.cleanShutdown == 1; }

private:
    std::string diskPath_;
    std::fstream diskFile_;
    Superblock superblock_;
    std::vector<bool> bitmap_;  // In-memory bitmap for performance
    
    void initializeSuperblock(uint32_t diskSize);
    uint32_t calculateBitmapBlocks() const;
    uint32_t calculateInodeBlocks() const;
};

} // namespace FileSystemTool

#endif // VIRTUALDISK_H
