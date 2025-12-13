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
    double getFragmentationScore();  // Not const - calculates on demand
    std::string getFilenameFromInode(uint32_t inodeNum) const;
    
    // Block ownership tracking (for visualization)
    void setBlockOwner(uint32_t blockNum, uint32_t inodeNum);
    uint32_t getBlockOwner(uint32_t blockNum) const;  // Returns inode num, or UINT32_MAX if unowned
    void clearBlockOwner(uint32_t blockNum);
    void rebuildBlockOwnership();  // Rebuild ownership map from disk state
    
    // Power cut simulation & recovery
    void simulatePowerCut();  // Old method - marks active write as corrupted
    bool simulatePowerCutDuringWrite(const std::string& filename, 
                                      const std::vector<uint8_t>& fullData,
                                      double crashPercent);  // New method - interactive crash
    void setCorruptionState(const std::vector<uint32_t>& corruptedBlocks, uint32_t inodeNum);  // For animated write
    bool hasCorruption() const { return hasCorruption_; }
    const std::vector<uint32_t>& getCorruptedBlocks() const { return corruptedBlocks_; }
    uint32_t getActiveWriteInode() const { return activeWriteInodeNum_; }
    bool runRecovery();  // Fix corruption, return true if successful
    
    // Access to underlying components (for recovery/optimization)
    VirtualDisk* getDisk() { return disk_.get(); }
    InodeManager* getInodeManager() { return inodeMgr_.get(); }
    DirectoryManager* getDirectoryManager() { return dirMgr_.get(); }
    
    // Performance measurement
    struct PerformanceStats { // Renamed to FileStats in the instruction, but keeping original name for consistency with existing code
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
    
    // Corruption tracking for power cut simulation
    bool hasCorruption_;
    std::vector<uint32_t> corruptedBlocks_;
    uint32_t activeWriteInodeNum_;
    
    // Helper functions
    bool allocateFileBlocks(Inode& inode, uint32_t blocksNeeded, uint32_t inodeNum);
    bool readFileData(const Inode& inode, std::vector<uint8_t>& data);
    void updateStats(bool isRead, double timeMs, uint64_t bytes);
};

} // namespace FileSystemTool

#endif // FILESYSTEM_H
