#ifndef FILESYSTEM_H
#define FILESYSTEM_H

#include "VirtualDisk.h"
#include "Inode.h"
#include "Directory.h"
#include <string>
#include <vector>
#include <memory>

namespace FileSystemTool {

// File handle for operations
struct FileHandle {
    uint32_t inodeNumber;
    Inode inode;
    std::string path;
    bool isOpen;
    
    FileHandle() : inodeNumber(0), isOpen(false) {}
};

class FileSystem {
public:
    FileSystem(const std::string& diskPath);
    ~FileSystem();
    
    // File system lifecycle
    bool createFileSystem(uint32_t diskSize = DEFAULT_DISK_SIZE);
    bool mountFileSystem();
    bool unmountFileSystem();
    bool isMounted() const { return mounted_; }
    
    // File operations (CRUD)
    bool createFile(const std::string& path);
    bool deleteFile(const std::string& path);
    bool readFile(const std::string& path, std::vector<uint8_t>& data);
    bool writeFile(const std::string& path, const std::vector<uint8_t>& data);
    bool fileExists(const std::string& path);
    
    // Directory operations
    bool createDir(const std::string& path);
    bool deleteDir(const std::string& path);
    std::vector<DirectoryEntry> listDir(const std::string& path);
    
    // File information
    bool getFileInfo(const std::string& path, Inode& info);
    uint64_t getFileSize(const std::string& path);
    
    // Disk statistics
    uint32_t getTotalBlocks() const;
    uint32_t getFreeBlocks() const;
    uint32_t getUsedBlocks() const;
    double getFragmentationScore();
    
    // Block ownership tracking for per-file visualization
    void setBlockOwner(uint32_t blockNum, uint32_t inodeNum);
    void clearBlockOwner(uint32_t blockNum);
    uint32_t getBlockOwner(uint32_t blockNum) const;  // Returns inode num, or UINT32_MAX if unowned
    std::string getFilenameFromInode(uint32_t inodeNum) const;
    void rebuildBlockOwnership();  // Rebuild ownership map from disk state
    
    // Access to underlying components (for recovery/optimization)
    VirtualDisk* getDisk() { return disk_.get(); }
    InodeManager* getInodeManager() { return inodeMgr_.get(); }
    DirectoryManager* getDirectoryManager() { return dirMgr_.get(); }
    
    // Performance measurement
    struct PerformanceStats {
        double lastReadTimeMs;
        double lastWriteTimeMs;
        uint64_t totalBytesRead;
        uint64_t totalBytesWritten;
        uint32_t totalReads;
        uint32_t totalWrites;
    };
    
    const PerformanceStats& getStats() const { return stats_; }
    void resetStats();
    
private:
    std::string diskPath_;
    std::unique_ptr<VirtualDisk> disk_;
    std::unique_ptr<InodeManager> inodeMgr_;
    std::unique_ptr<DirectoryManager> dirMgr_;
    bool mounted_;
    PerformanceStats stats_;
    std::map<uint32_t, uint32_t> blockOwners_;  // blockNum -> inodeNum mapping
    
    // Helper functions
    bool allocateFileBlocks(uint32_t inodeNum, Inode& inode, const std::vector<uint8_t>& data);
    bool readFileData(const Inode& inode, std::vector<uint8_t>& data);
    void updateStats(bool isRead, double timeMs, uint64_t bytes);
};

} // namespace FileSystemTool

#endif // FILESYSTEM_H
