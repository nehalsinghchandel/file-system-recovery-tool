#ifndef INODE_H
#define INODE_H

#include <cstdint>
#include <ctime>
#include <string>
#include <vector>

namespace FileSystemTool {

// File type enumeration
enum class FileType : uint8_t {
    REGULAR_FILE = 1,
    DIRECTORY = 2,
    FREE = 0
};

// Inode structure - stores file metadata
constexpr uint32_t DIRECT_BLOCKS = 12;
constexpr uint32_t INODE_SIZE = 128;  // bytes

struct Inode {
    uint32_t inodeNumber;           // Inode number
    FileType fileType;              // File type
    uint8_t  permissions;           // Simple permission bits (rwx)
    uint16_t linkCount;             // Number of hard links
    uint32_t fileSize;              // File size in bytes
    uint32_t blockCount;            // Number of blocks allocated
    time_t   createdTime;           // Creation timestamp
    time_t   modifiedTime;          // Last modification timestamp
    time_t   accessedTime;          // Last access timestamp
    uint32_t directBlocks[DIRECT_BLOCKS];  // Direct block pointers
    uint32_t indirectBlock;         // Single indirect block pointer
    uint8_t  padding[20];           // Padding to make 128 bytes
    
    Inode();
    void reset();
    bool isValid() const;
    bool isFree() const;
};

// Inode Manager class
class InodeManager {
public:
    InodeManager(class VirtualDisk* disk);
    
    // Inode operations
    int32_t allocateInode(FileType type);
    bool freeInode(uint32_t inodeNum);
    bool readInode(uint32_t inodeNum, Inode& inode);
    bool writeInode(uint32_t inodeNum, const Inode& inode);
    
    // Block pointer management
    bool addBlockToInode(Inode& inode, uint32_t blockNum);
    bool removeBlockFromInode(Inode& inode, uint32_t blockIndex);
    std::vector<uint32_t> getInodeBlocks(const Inode& inode);
    
    // Utilities
    uint32_t getMaxInodes() const;
    uint32_t calculateInodeBlockNum(uint32_t inodeNum) const;
    
private:
    VirtualDisk* disk_;
    
    bool readIndirectBlock(uint32_t blockNum, std::vector<uint32_t>& pointers);
    bool writeIndirectBlock(uint32_t blockNum, const std::vector<uint32_t>& pointers);
};

} // namespace FileSystemTool

#endif // INODE_H
