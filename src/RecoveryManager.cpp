#include "RecoveryManager.h"
#include <iostream>
#include <algorithm>
#include <set>
#include <cstdlib>

namespace FileSystemTool {

RecoveryManager::RecoveryManager(FileSystem* fs) : fs_(fs) {
    journal_ = std::make_unique<Journal>(fs_->getDisk());
    journal_->openJournal();
}

RecoveryManager::~RecoveryManager() {}

bool RecoveryManager::performRecovery() {
    std::cout << "Starting file system recovery..." << std::endl;
    
    // First, replay journal
    if (!replayJournal()) {
        std::cerr << "Journal replay failed" << std::endl;
    }
    
    // Then check consistency
    auto report = checkConsistency();
    lastReport_ = report;
    
    if (!report.isConsistent) {
        std::cout << "Inconsistencies found. Attempting repair..." << std::endl;
        return repairFileSystem(report);
    }
    
    std::cout << "File system is consistent" << std::endl;
    return true;
}

ConsistencyReport RecoveryManager::checkConsistency() {
    ConsistencyReport report;
    
    checkBitmapConsistency(report);
    checkInodeConsistency(report);
    checkDirectoryConsistency(report);
    
    report.isConsistent = (report.orphanBlocks == 0 && 
                          report.invalidInodes == 0 && 
                          report.brokenDirectories == 0);
    
    return report;
}

bool RecoveryManager::repairFileSystem(const ConsistencyReport& report) {
    bool success = true;
    
    // Fix orphan blocks
    auto orphans = findOrphanBlocks();
    if (!orphans.empty()) {
        if (!fixOrphanBlocks(orphans)) {
            std::cerr << "Failed to fix orphan blocks" << std::endl;
            success = false;
        }
    }
    
    // Fix invalid inodes
    auto invalidInodes = findInvalidInodes();
    if (!invalidInodes.empty()) {
        if (!fixInvalidInodes(invalidInodes)) {
            std::cerr << "Failed to fix invalid inodes" << std::endl;
            success = false;
        }
    }
    
    std::cout << "Repair completed" << std::endl;
    return success;
}

bool RecoveryManager::replayJournal() {
    auto uncommitted = journal_->getUncommittedTransactions();
    
    if (uncommitted.empty()) {
        std::cout << "No uncommitted transactions found" << std::endl;
        return true;
    }
    
    std::cout << "Found " <<  uncommitted.size() << " uncommitted transactions" << std::endl;
    
    for (const auto& entry : uncommitted) {
        std::cout << "Rolling back transaction " << entry.transactionId << std::endl;
        // For simplicity, we abort all uncommitted transactions
        // In a real implementation, you might try to complete them
        journal_->abortTransaction(entry.transactionId);
    }
    
    journal_->clearJournal();
    return true;
}

bool RecoveryManager::checkBitmapConsistency(ConsistencyReport& report) {
    auto allocated = getAllAllocatedBlocks();
    auto bitmap = fs_->getDisk()->getBitmap();
    
    uint32_t orphans = 0;
    const auto& sb = fs_->getDisk()->getSuperblock();
    
    // Check for blocks marked used but not referenced
    for (uint32_t i = sb.dataBlocksStart; i < bitmap.size(); ++i) {
        bool inBitmap = !bitmap[i];  // false = used
        bool inInodes = std::find(allocated.begin(), allocated.end(), i) != allocated.end();
        
        if (inBitmap && !inInodes) {
            orphans++;
        }
    }
    
    report.orphanBlocks = orphans;
    if (orphans > 0) {
        report.errors.push_back("Found " + std::to_string(orphans) + " orphan blocks");
    }
    
    return orphans == 0;
}

bool RecoveryManager::checkInodeConsistency(ConsistencyReport& report) {
    const auto& sb = fs_->getDisk()->getSuperblock();
    uint32_t invalidCount = 0;
    
    for (uint32_t i = 0; i < sb.inodeCount; ++i) {
        Inode inode;
        if (!fs_->getInodeManager()->readInode(i, inode)) {
            continue;
        }
        
        if (inode.isValid()) {
            // Check if file size matches block count
            uint32_t expectedBlocks = (inode.fileSize + BLOCK_SIZE - 1) / BLOCK_SIZE;
            if (inode.blockCount != expectedBlocks && inode.fileType == FileType::REGULAR_FILE) {
                invalidCount++;
            }
        }
    }
    
    report.invalidInodes = invalidCount;
    if (invalidCount > 0) {
        report.errors.push_back("Found " + std::to_string(invalidCount) + " invalid inodes");
    }
    
    return invalidCount == 0;
}

bool RecoveryManager::checkDirectoryConsistency(ConsistencyReport& report) {
    // Basic check: ensure root directory exists
    Inode rootInode;
    if (!fs_->getInodeManager()->readInode(0, rootInode) || 
        rootInode.fileType != FileType::DIRECTORY) {
        report.brokenDirectories = 1;
        report.errors.push_back("Root directory is corrupted");
        return false;
    }
    
    return true;
}

bool RecoveryManager::fixOrphanBlocks(std::vector<uint32_t>& orphanBlocks) {
    std::cout << "Freeing " << orphanBlocks.size() << " orphan blocks..." << std::endl;
    
    for (uint32_t blockNum : orphanBlocks) {
        fs_->getDisk()->freeBlock(blockNum);
    }
    
    lastReport_.fixes.push_back("Freed " + std::to_string(orphanBlocks.size()) + " orphan blocks");
    return true;
}

bool RecoveryManager::fixInvalidInodes(std::vector<uint32_t>& invalidInodes) {
    std::cout << "Fixing " << invalidInodes.size() << " invalid inodes..." << std::endl;
    
    for (uint32_t inodeNum : invalidInodes) {
        fs_->getInodeManager()->freeInode(inodeNum);
    }
    
    lastReport_.fixes.push_back("Freed " + std::to_string(invalidInodes.size()) + " invalid inodes");
    return true;
}

void RecoveryManager::simulateCrashDuringWrite(const std::string& filename, 
                                                const std::vector<uint8_t>& data,
                                                double crashAtPercent) {
    std::cout << "Simulating crash during write to " << filename << std::endl;
    
    // Create partial data
    size_t crashPoint = static_cast<size_t>(data.size() * crashAtPercent);
    std::vector<uint8_t> partialData(data.begin(), data.begin() + crashPoint);
    
    // Write partial data
    fs_->writeFile(filename, partialData);
    
    // Mark disk as dirty (simulating crash)
    fs_->getDisk()->markDirty();
    
    std::cout << "Crash simulated! Wrote " << crashPoint << "/" << data.size() << " bytes" << std::endl;
}

void RecoveryManager::simulateCrashDuringDelete(const std::string& filename) {
    std::cout << "Simulating crash during delete of " << filename << std::endl;
    
    // Partially delete (remove from directory but don't free blocks)
    // This creates orphan blocks
    
    fs_->deleteFile(filename);
    fs_->getDisk()->markDirty();
    
    std::cout << "Crash simulated during delete!" << std::endl;
}

std::vector<uint32_t> RecoveryManager::findOrphanBlocks() {
    auto allocated = getAllAllocatedBlocks();
    auto bitmap = fs_->getDisk()->getBitmap();
    const auto& sb = fs_->getDisk()->getSuperblock();
    
    std::vector<uint32_t> orphans;
    
    for (uint32_t i = sb.dataBlocksStart; i < bitmap.size(); ++i) {
        bool inBitmap = !bitmap[i];  // false = used
        bool inInodes = std::find(allocated.begin(), allocated.end(), i) != allocated.end();
        
        if (inBitmap && !inInodes) {
            orphans.push_back(i);
        }
    }
    
    return orphans;
}

std::vector<uint32_t> RecoveryManager::getAllAllocatedBlocks() {
    std::set<uint32_t> blocks;
    const auto& sb = fs_->getDisk()->getSuperblock();
    
    // Scan all inodes
    for (uint32_t i = 0; i < sb.inodeCount; ++i) {
        Inode inode;
        if (fs_->getInodeManager()->readInode(i, inode) && inode.isValid()) {
            auto inodeBlocks = fs_->getInodeManager()->getInodeBlocks(inode);
            blocks.insert(inodeBlocks.begin(), inodeBlocks.end());
            
            // Also include indirect block itself
            if (inode.indirectBlock != 0) {
                blocks.insert(inode.indirectBlock);
            }
        }
    }
    
    return std::vector<uint32_t>(blocks.begin(), blocks.end());
}

std::vector<uint32_t> RecoveryManager::findInvalidInodes() {
    std::vector<uint32_t> invalid;
    const auto& sb = fs_->getDisk()->getSuperblock();
    
    for (uint32_t i = 0; i < sb.inodeCount; ++i) {
        Inode inode;
        if (!fs_->getInodeManager()->readInode(i, inode)) {
            continue;
        }
        
        if (inode.isValid()) {
            uint32_t expectedBlocks = (inode.fileSize + BLOCK_SIZE - 1) / BLOCK_SIZE;
            if (inode.blockCount != expectedBlocks && inode.fileType == FileType::REGULAR_FILE) {
                invalid.push_back(i);
            }
        }
    }
    
    return invalid;
}

} // namespace FileSystemTool
