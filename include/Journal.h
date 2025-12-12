#ifndef JOURNAL_H
#define JOURNAL_H

#include "Inode.h"
#include <cstdint>
#include <vector>
#include <ctime>

namespace FileSystemTool {

// Journal transaction types
enum class JournalOp : uint8_t {
    CREATE_FILE = 1,
    DELETE_FILE = 2,
    WRITE_DATA = 3,
    UPDATE_INODE = 4,
    CREATE_DIR = 5,
    DELETE_DIR = 6
};

// Journal entry structure
constexpr uint32_t JOURNAL_ENTRY_SIZE = 256;  // bytes

struct JournalEntry {
    uint32_t transactionId;         // Unique transaction ID
    JournalOp operation;            // Type of operation
    uint8_t  committed;             // 1 if committed, 0 if in progress
    uint8_t  padding[2];            // Padding
    time_t   timestamp;             // When transaction started
    uint32_t inodeNumber;           // Target inode
    uint32_t parentInodeNumber;     // Parent directory inode (if applicable)
    uint32_t blockCount;            // Number of blocks involved
    uint32_t blocks[32];            // Block numbers involved (max 32)
    char     filename[128];         // Filename (if applicable)
    
    JournalEntry();
    void reset();
    bool isValid() const;
};

class Journal {
public:
    Journal(class VirtualDisk* disk);
    
    // Journal lifecycle
    bool initializeJournal();
    bool openJournal();
    
    // Transaction management
    uint32_t beginTransaction(JournalOp op, uint32_t inodeNum, const std::string& filename = "");
    bool commitTransaction(uint32_t transactionId);
    bool abortTransaction(uint32_t transactionId);
    
    // Add block info to current transaction
    bool addBlockToTransaction(uint32_t transactionId, uint32_t blockNum);
    
    // Recovery operations
    std::vector<JournalEntry> getUncommittedTransactions();
    bool replayTransaction(const JournalEntry& entry);
    bool clearJournal();
    
    // Journal statistics
    uint32_t getTransactionCount() const { return nextTransactionId_; }
    
private:
    VirtualDisk* disk_;
    uint32_t nextTransactionId_;
    uint32_t journalStartBlock_;
    uint32_t journalBlockCount_;
    
    bool readJournalEntry(uint32_t index, JournalEntry& entry);
    bool writeJournalEntry(uint32_t index, const JournalEntry& entry);
    uint32_t findFreeJournalSlot();
};

} // namespace FileSystemTool

#endif // JOURNAL_H
