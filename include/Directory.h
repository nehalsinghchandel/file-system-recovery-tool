#ifndef DIRECTORY_H
#define DIRECTORY_H

#include "Inode.h"
#include <string>
#include <vector>
#include <map>

namespace FileSystemTool {

// Directory entry structure
constexpr uint32_t MAX_FILENAME_LENGTH = 56;
constexpr uint32_t DIR_ENTRY_SIZE = 64;  // bytes

struct DirectoryEntry {
    uint32_t inodeNumber;                       // Inode number
    uint8_t  nameLength;                        // Length of filename
    uint8_t  fileType;                          // File type (for quick lookup)
    uint8_t  padding[6];                        // Padding
    char     filename[MAX_FILENAME_LENGTH];     // Filename
    
    DirectoryEntry();
    DirectoryEntry(uint32_t inode, const std::string& name, FileType type);
    void reset();
    bool isValid() const;
    std::string getName() const;
};

class DirectoryManager {
public:
    DirectoryManager(class VirtualDisk* disk, class InodeManager* inodeMgr);
    
    // Directory operations
    bool createDirectory(const std::string& name, uint32_t parentInodeNum, uint32_t& newInodeNum);
    bool deleteDirectory(uint32_t inodeNum);
    
    // Entry management
    bool addEntry(uint32_t dirInodeNum, const std::string& name, uint32_t entryInodeNum, FileType type);
    bool removeEntry(uint32_t dirInodeNum, const std::string& name);
    int32_t lookupEntry(uint32_t dirInodeNum, const std::string& name);
    
    // Directory listing
    std::vector<DirectoryEntry> listDirectory(uint32_t dirInodeNum);
    
    // Path resolution
    int32_t resolvePath(const std::string& path, uint32_t startInodeNum);
    std::vector<std::string> splitPath(const std::string& path);
    
    // Initialize root directory
    bool initializeRootDirectory();
    
private:
    VirtualDisk* disk_;
    InodeManager* inodeMgr_;
    
    bool readDirectoryEntries(const Inode& dirInode, std::vector<DirectoryEntry>& entries);
    bool writeDirectoryEntries(Inode& dirInode, const std::vector<DirectoryEntry>& entries);
    bool expandDirectory(Inode& dirInode);
};

} // namespace FileSystemTool

#endif // DIRECTORY_H
