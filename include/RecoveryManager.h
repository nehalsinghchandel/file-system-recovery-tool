#ifndef RECOVERYMANAGER_H
#define RECOVERYMANAGER_H

#include "FileSystem.h"
#include "Journal.h"
#include <vector>
#include <string>
#include <memory>

namespace FileSystemTool {

// Consistency check results
struct ConsistencyReport {
    bool isConsistent;
    uint32_t orphanBlocks;
    uint32_t invalidInodes;
    uint32_t brokenDirectories;
    std::vector<std::string> errors;
    std::vector<std::string> fixes;
    
    ConsistencyReport() : isConsistent(true), orphanBlocks(0), 
                         invalidInodes(0), brokenDirectories(0) {}
};

class RecoveryManager {
public:
    RecoveryManager(FileSystem* fs);
    ~RecoveryManager();
    
    // Recovery operations
    bool performRecovery();
    ConsistencyReport checkConsistency();
    bool repairFileSystem(const ConsistencyReport& report);
    
    // Journal-based recovery
    bool replayJournal();
    
    // Consistency checks
    bool checkBitmapConsistency(ConsistencyReport& report);
    bool checkInodeConsistency(ConsistencyReport& report);
    bool checkDirectoryConsistency(ConsistencyReport& report);
    
    // Repair operations
    bool fixOrphanBlocks(std::vector<uint32_t>& orphanBlocks);
    bool fixInvalidInodes(std::vector<uint32_t>& invalidInodes);
    
    // Crash simulation
    void simulateCrashDuringWrite(const std::string& filename, 
                                  const std::vector<uint8_t>& data,
                                  double crashAtPercent = 0.5);
    void simulateCrashDuringDelete(const std::string& filename);
    
    // Get last recovery report
    const ConsistencyReport& getLastReport() const { return lastReport_; }
    
private:
    FileSystem* fs_;
    std::unique_ptr<Journal> journal_;
    ConsistencyReport lastReport_;
    
    // Helper functions
    std::vector<uint32_t> findOrphanBlocks();
    std::vector<uint32_t> getAllAllocatedBlocks();
    std::vector<uint32_t> findInvalidInodes();
};

} // namespace FileSystemTool

#endif // RECOVERYMANAGER_H
