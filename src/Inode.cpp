#include "Inode.h"
#include "VirtualDisk.h"
#include <cstring>
#include <iostream>

namespace FileSystemTool {

Inode::Inode() {
    reset();
}

void Inode::reset() {
    inodeNumber = 0;
    fileType = FileType::FREE;
    permissions = 0;
    linkCount = 0;
    fileSize = 0;
    blockCount = 0;
    createdTime = 0;
    modifiedTime = 0;
    accessedTime = 0;
    indirectBlock = 0;
    memset(directBlocks, 0, sizeof(directBlocks));
    memset(padding, 0, sizeof(padding));
}

bool Inode::isValid() const {
    return fileType != FileType::FREE;
}

bool Inode::isFree() const {
    return fileType == FileType::FREE;
}

InodeManager::InodeManager(VirtualDisk* disk) : disk_(disk) {}

int32_t InodeManager::allocateInode(FileType type) {
    const auto& sb = disk_->getSuperblock();
    
    // Find first free inode
    for (uint32_t i = 0; i < sb.inodeCount; ++i) {
        Inode inode;
        if (readInode(i, inode) && inode.isFree()) {
            // Initialize inode
            inode.inodeNumber = i;
            inode.fileType = type;
            inode.permissions = 0x1A4;  // rw-r--r-- (644 octal)
            inode.linkCount = 1;
            inode.fileSize = 0;
            inode.blockCount = 0;
            inode.createdTime = time(nullptr);
            inode.modifiedTime = inode.createdTime;
            inode.accessedTime = inode.createdTime;
            
            if (writeInode(i, inode)) {
                return static_cast<int32_t>(i);
            }
        }
    }
    
    std::cerr << "No free inodes available" << std::endl;
    return -1;
}

bool InodeManager::freeInode(uint32_t inodeNum) {
    Inode inode;
    if (!readInode(inodeNum, inode)) {
        return false;
    }
    
    // Free all blocks used by this inode
    auto blocks = getInodeBlocks(inode);
    for (uint32_t blockNum : blocks) {
        disk_->freeBlock(blockNum);
    }
    
    // Reset inode
    inode.reset();
    return writeInode(inodeNum, inode);
}

bool InodeManager::readInode(uint32_t inodeNum, Inode& inode) {
    const auto& sb = disk_->getSuperblock();
    if (inodeNum >= sb.inodeCount) {
        return false;
    }
    
    uint32_t inodesPerBlock = BLOCK_SIZE / INODE_SIZE;
    uint32_t blockNum = sb.inodeTableStart + (inodeNum / inodesPerBlock);
    uint32_t offset = (inodeNum % inodesPerBlock) * INODE_SIZE;
    
    std::vector<uint8_t> buffer(BLOCK_SIZE);
    if (!disk_->readBlock(blockNum, buffer.data())) {
        return false;
    }
    
    memcpy(&inode, buffer.data() + offset, sizeof(Inode));
    return true;
}

bool InodeManager::writeInode(uint32_t inodeNum, const Inode& inode) {
    const auto& sb = disk_->getSuperblock();
    if (inodeNum >= sb.inodeCount) {
        return false;
    }
    
    uint32_t inodesPerBlock = BLOCK_SIZE / INODE_SIZE;
    uint32_t blockNum = sb.inodeTableStart + (inodeNum / inodesPerBlock);
    uint32_t offset = (inodeNum % inodesPerBlock) * INODE_SIZE;
    
    // Read block, modify inode, write back
    std::vector<uint8_t> buffer(BLOCK_SIZE);
    if (!disk_->readBlock(blockNum, buffer.data())) {
        return false;
    }
    
    memcpy(buffer.data() + offset, &inode, sizeof(Inode));
    return disk_->writeBlock(blockNum, buffer.data());
}

bool InodeManager::addBlockToInode(Inode& inode, uint32_t blockNum) {
    // Try direct blocks first
    for (uint32_t i = 0; i < DIRECT_BLOCKS; ++i) {
        if (inode.directBlocks[i] == 0) {
            inode.directBlocks[i] = blockNum;
            inode.blockCount++;
            return true;
        }
    }
    
    // Use indirect block
    std::vector<uint32_t> indirectPointers;
    if (inode.indirectBlock == 0) {
        // Allocate indirect block
        int32_t indBlock = disk_->allocateBlock();
        if (indBlock < 0) return false;
        inode.indirectBlock = static_cast<uint32_t>(indBlock);
    } else {
        readIndirectBlock(inode.indirectBlock, indirectPointers);
    }
    
    indirectPointers.push_back(blockNum);
    inode.blockCount++;
    return writeIndirectBlock(inode.indirectBlock, indirectPointers);
}

std::vector<uint32_t> InodeManager::getInodeBlocks(const Inode& inode) {
    std::vector<uint32_t> blocks;
    const auto& sb = disk_->getSuperblock();
    
    // CRITICAL FIX: Only add VALID direct blocks (skip -1, 0, out of range)
    for (int i = 0; i < 12; ++i) {
        if (inode.directBlocks[i] > 0 &&
            inode.directBlocks[i] != -1 &&
            inode.directBlocks[i] < (int32_t)sb.totalBlocks) {
            blocks.push_back(static_cast<uint32_t>(inode.directBlocks[i]));
        }
    }
    
    // Handle indirect block if valid
    if (inode.indirectBlock > 0 &&
        inode.indirectBlock != -1 &&
        inode.indirectBlock < (int32_t)sb.totalBlocks) {
        
        std::vector<uint8_t> indirectData(BLOCK_SIZE);
        if (disk_->readBlock(inode.indirectBlock, indirectData.data())) {
            const int32_t* pointers = reinterpret_cast<const int32_t*>(indirectData.data());
            int maxIndirect = BLOCK_SIZE / sizeof(int32_t);
            
            for (int i = 0; i < maxIndirect; ++i) {
                // CRITICAL: Skip invalid indirect pointers
                if (pointers[i] > 0 &&
                    pointers[i] != -1 &&
                    pointers[i] < (int32_t)sb.totalBlocks) {
                    blocks.push_back(static_cast<uint32_t>(pointers[i]));
                }
            }
        }
    }
    
    return blocks;
}

bool InodeManager::readIndirectBlock(uint32_t blockNum, std::vector<uint32_t>& pointers) {
    std::vector<uint8_t> buffer(BLOCK_SIZE);
    if (!disk_->readBlock(blockNum, buffer.data())) {
        return false;
    }
    
    pointers.clear();
    uint32_t* ptr = reinterpret_cast<uint32_t*>(buffer.data());
    uint32_t maxPointers = BLOCK_SIZE / sizeof(uint32_t);
    
    for (uint32_t i = 0; i < maxPointers && ptr[i] != 0; ++i) {
        pointers.push_back(ptr[i]);
    }
    
    return true;
}

bool InodeManager::writeIndirectBlock(uint32_t blockNum, const std::vector<uint32_t>& pointers) {
    std::vector<uint8_t> buffer(BLOCK_SIZE, 0);
    uint32_t* ptr = reinterpret_cast<uint32_t*>(buffer.data());
    
    for (size_t i = 0; i < pointers.size(); ++i) {
        ptr[i] = pointers[i];
    }
    
    return disk_->writeBlock(blockNum, buffer.data());
}

uint32_t InodeManager::getMaxInodes() const {
    return disk_->getSuperblock().inodeCount;
}

} // namespace FileSystemTool
