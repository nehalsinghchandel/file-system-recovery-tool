#include "Directory.h"
#include "VirtualDisk.h"
#include <cstring>
#include <iostream>
#include <sstream>

namespace FileSystemTool {

DirectoryEntry::DirectoryEntry() {
    reset();
}

DirectoryEntry::DirectoryEntry(uint32_t inode, const std::string& name, FileType type) {
    reset();
    inodeNumber = inode;
    nameLength = std::min(static_cast<size_t>(MAX_FILENAME_LENGTH - 1), name.length());
    fileType = static_cast<uint8_t>(type);
    strncpy(filename, name.c_str(), nameLength);
    filename[nameLength] = '\0';
}

void DirectoryEntry::reset() {
    inodeNumber = 0;
    nameLength = 0;
    fileType = static_cast<uint8_t>(FileType::FREE);
    memset(padding, 0, sizeof(padding));
    memset(filename, 0, sizeof(filename));
}

bool DirectoryEntry::isValid() const {
    return inodeNumber != 0 && nameLength > 0;
}

std::string DirectoryEntry::getName() const {
    return std::string(filename, nameLength);
}

DirectoryManager::DirectoryManager(VirtualDisk* disk, InodeManager* inodeMgr)
    : disk_(disk), inodeMgr_(inodeMgr) {}

bool DirectoryManager::createDirectory(const std::string& name, uint32_t parentInodeNum, uint32_t& newInodeNum) {
    // Allocate inode for new directory
    int32_t inodeNum = inodeMgr_->allocateInode(FileType::DIRECTORY);
    if (inodeNum < 0) {
        return false;
    }
    
    newInodeNum = static_cast<uint32_t>(inodeNum);
    
    // Create . and .. entries
    std::vector<DirectoryEntry> entries;
    entries.push_back(DirectoryEntry(newInodeNum, ".", FileType::DIRECTORY));
    entries.push_back(DirectoryEntry(parentInodeNum, "..", FileType::DIRECTORY));
    
    // Write directory entries
    Inode dirInode;
    if (!inodeMgr_->readInode(newInodeNum, dirInode)) {
        return false;
    }
    
    if (!writeDirectoryEntries(dirInode, entries)) {
        inodeMgr_->freeInode(newInodeNum);
        return false;
    }
    
    // Update parent directory to include this new directory
    return addEntry(parentInodeNum, name, newInodeNum, FileType::DIRECTORY);
}

bool DirectoryManager::addEntry(uint32_t dirInodeNum, const std::string& name, 
                                 uint32_t entryInodeNum, FileType type) {
    Inode dirInode;
    if (!inodeMgr_->readInode(dirInodeNum, dirInode)) {
        return false;
    }
    
    if (dirInode.fileType != FileType::DIRECTORY) {
        std::cerr << "Inode is not a directory" << std::endl;
        return false;
    }
    
    // Read existing entries
    std::vector<DirectoryEntry> entries;
    readDirectoryEntries(dirInode, entries);
    
    // Check if entry already exists
    for (const auto& entry : entries) {
        if (entry.getName() == name) {
            std::cerr << "Entry already exists: " << name << std::endl;
            return false;
        }
    }
    
    // Add new entry
    entries.push_back(DirectoryEntry(entryInodeNum, name, type));
    
    // Write back
    return writeDirectoryEntries(dirInode, entries);
}

bool DirectoryManager::removeEntry(uint32_t dirInodeNum, const std::string& name) {
    Inode dirInode;
    if (!inodeMgr_->readInode(dirInodeNum, dirInode)) {
        return false;
    }
    
    std::vector<DirectoryEntry> entries;
    readDirectoryEntries(dirInode, entries);
    
    // Find and remove entry
    auto it = std::remove_if(entries.begin(), entries.end(),
        [&name](const DirectoryEntry& e) { return e.getName() == name; });
    
    if (it == entries.end()) {
        return false;  // Entry not found
    }
    
    entries.erase(it, entries.end());
    return writeDirectoryEntries(dirInode, entries);
}

int32_t DirectoryManager::lookupEntry(uint32_t dirInodeNum, const std::string& name) {
    Inode dirInode;
    if (!inodeMgr_->readInode(dirInodeNum, dirInode)) {
        return -1;
    }
    
    std::vector<DirectoryEntry> entries;
    readDirectoryEntries(dirInode, entries);
    
    for (const auto& entry : entries) {
        if (entry.getName() == name) {
            return static_cast<int32_t>(entry.inodeNumber);
        }
    }
    
    return -1;  // Not found
}

std::vector<DirectoryEntry> DirectoryManager::listDirectory(uint32_t dirInodeNum) {
    Inode dirInode;
    std::vector<DirectoryEntry> entries;
    
    if (inodeMgr_->readInode(dirInodeNum, dirInode)) {
        readDirectoryEntries(dirInode, entries);
    }
    
    return entries;
}

int32_t DirectoryManager::resolvePath(const std::string& path, uint32_t startInodeNum) {
    if (path.empty() || path == "/") {
        return 0;  // Root inode
    }
    
    auto components = splitPath(path);
    uint32_t currentInode = (path[0] == '/') ? 0 : startInodeNum;
    
    for (const auto& component : components) {
        if (component.empty()) continue;
        
        int32_t nextInode = lookupEntry(currentInode, component);
        if (nextInode < 0) {
            return -1;  // Path not found
        }
        currentInode = static_cast<uint32_t>(nextInode);
    }
    
    return static_cast<int32_t>(currentInode);
}

std::vector<std::string> DirectoryManager::splitPath(const std::string& path) {
    std::vector<std::string> components;
    std::istringstream iss(path);
    std::string component;
    
    while (std::getline(iss, component, '/')) {
        if (!component.empty()) {
            components.push_back(component);
        }
    }
    
    return components;
}

bool DirectoryManager::initializeRootDirectory() {
    // Check if root already exists
    Inode rootInode;
    if (inodeMgr_->readInode(0, rootInode) && rootInode.isValid()) {
        return true;  // Already initialized
    }
    
    // Allocate inode 0 for root
    rootInode.inodeNumber = 0;
    rootInode.fileType = FileType::DIRECTORY;
    rootInode.permissions = 0x1ED;  // rwxr-xr-x (755 octal)
    rootInode.linkCount = 2;  // . and parent
    rootInode.createdTime = time(nullptr);
    rootInode.modifiedTime = rootInode.createdTime;
    rootInode.accessedTime = rootInode.createdTime;
    
    if (!inodeMgr_->writeInode(0, rootInode)) {
        return false;
    }
    
    // Create . and .. entries (both point to root)
    std::vector<DirectoryEntry> entries;
    entries.push_back(DirectoryEntry(0, ".", FileType::DIRECTORY));
    entries.push_back(DirectoryEntry(0, "..", FileType::DIRECTORY));
    
    return writeDirectoryEntries(rootInode, entries);
}

bool DirectoryManager::readDirectoryEntries(const Inode& dirInode, std::vector<DirectoryEntry>& entries) {
    entries.clear();
    
    auto blocks = inodeMgr_->getInodeBlocks(dirInode);
    std::vector<uint8_t> buffer(BLOCK_SIZE);
    
    for (uint32_t blockNum : blocks) {
        if (!disk_->readBlock(blockNum, buffer.data())) {
            return false;
        }
        
        // Parse directory entries from block
        uint32_t entriesPerBlock = BLOCK_SIZE / DIR_ENTRY_SIZE;
        for (uint32_t i = 0; i < entriesPerBlock; ++i) {
            DirectoryEntry entry;
            memcpy(&entry, buffer.data() + (i * DIR_ENTRY_SIZE), sizeof(DirectoryEntry));
            
            if (entry.isValid()) {
                entries.push_back(entry);
            }
        }
    }
    
    return true;
}

bool DirectoryManager::writeDirectoryEntries(Inode& dirInode, const std::vector<DirectoryEntry>& entries) {
    uint32_t entriesPerBlock = BLOCK_SIZE / DIR_ENTRY_SIZE;
    uint32_t blocksNeeded = entries.empty() ? 1 : (entries.size() + entriesPerBlock - 1) / entriesPerBlock;
    
    // Get current blocks
    auto blocks = inodeMgr_->getInodeBlocks(dirInode);
    
    // Allocate more blocks if needed
    while (blocks.size() < blocksNeeded) {
        int32_t newBlock = disk_->allocateBlock();
        if (newBlock < 0) {
            return false;
        }
        
        if (!inodeMgr_->addBlockToInode(dirInode, static_cast<uint32_t>(newBlock))) {
            disk_->freeBlock(static_cast<uint32_t>(newBlock));
            return false;
        }
        
        blocks.push_back(static_cast<uint32_t>(newBlock));
    }
    
    // Write entries to blocks
    std::vector<uint8_t> buffer(BLOCK_SIZE, 0);
    size_t entryIndex = 0;
    
    // Write blocks with entries
    for (size_t blockIdx = 0; blockIdx < blocksNeeded && blockIdx < blocks.size(); ++blockIdx) {
        buffer.assign(BLOCK_SIZE, 0);  // Zero the entire block
        
        for (uint32_t i = 0; i < entriesPerBlock && entryIndex < entries.size(); ++i) {
            memcpy(buffer.data() + (i * DIR_ENTRY_SIZE), &entries[entryIndex], sizeof(DirectoryEntry));
            entryIndex++;
        }
        
        if (!disk_->writeBlock(blocks[blockIdx], buffer.data())) {
            return false;
        }
    }
    
    // IMPORTANT: Zero out any remaining blocks that are no longer needed
    // This fixes the bug where deleted entries persist on refresh
    for (size_t blockIdx = blocksNeeded; blockIdx < blocks.size(); ++blockIdx) {
        buffer.assign(BLOCK_SIZE, 0);
        if (!disk_->writeBlock(blocks[blockIdx], buffer.data())) {
            return false;
        }
    }
    
    // Update inode
    dirInode.fileSize = entries.size() * DIR_ENTRY_SIZE;
    dirInode.modifiedTime = time(nullptr);
    return inodeMgr_->writeInode(dirInode.inodeNumber, dirInode);
}

} // namespace FileSystemTool
