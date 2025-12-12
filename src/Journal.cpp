#include "Journal.h"
#include "VirtualDisk.h"
#include <cstring>
#include <iostream>

namespace FileSystemTool {

JournalEntry::JournalEntry() {
    reset();
}

void JournalEntry::reset() {
    transactionId = 0;
    operation = JournalOp::CREATE_FILE;
    committed = 0;
    timestamp = 0;
    inodeNumber = 0;
    parentInodeNumber = 0;
    blockCount = 0;
    memset(padding, 0, sizeof(padding));
    memset(blocks, 0, sizeof(blocks));
    memset(filename, 0, sizeof(filename));
}

bool JournalEntry::isValid() const {
    return transactionId != 0;
}

Journal::Journal(VirtualDisk* disk) : disk_(disk), nextTransactionId_(1) {
    const auto& sb = disk_->getSuperblock();
    journalStartBlock_ = sb.journalStart;
    journalBlockCount_ = sb.journalSize;
}

bool Journal::initializeJournal() {
    // Clear journal
    std::vector<uint8_t> zeros(BLOCK_SIZE, 0);
    for (uint32_t i = 0; i < journalBlockCount_; ++i) {
        if (!disk_->writeBlock(journalStartBlock_ + i, zeros.data())) {
            return false;
        }
    }
    nextTransactionId_ = 1;
    return true;
}

bool Journal::openJournal() {
    // Scan for highest transaction ID
    JournalEntry entry;
    nextTransactionId_ = 1;
    
    uint32_t entriesPerBlock = BLOCK_SIZE / JOURNAL_ENTRY_SIZE;
    uint32_t maxEntries = journalBlockCount_ * entriesPerBlock;
    
    for (uint32_t i = 0; i < maxEntries; ++i) {
        if (readJournalEntry(i, entry) && entry.isValid()) {
            if (entry.transactionId >= nextTransactionId_) {
                nextTransactionId_ = entry.transactionId + 1;
            }
        }
    }
    
    return true;
}

uint32_t Journal::beginTransaction(JournalOp op, uint32_t inodeNum, const std::string& filename) {
    uint32_t slot = findFreeJournalSlot();
    if (slot == UINT32_MAX) {
        std::cerr << "Journal full" << std::endl;
        return 0;
    }
    
    JournalEntry entry;
    entry.transactionId = nextTransactionId_++;
    entry.operation = op;
    entry.committed = 0;
    entry.timestamp = time(nullptr);
    entry.inodeNumber = inodeNum;
    entry.blockCount = 0;
    
    if (!filename.empty()) {
        strncpy(entry.filename, filename.c_str(), sizeof(entry.filename) - 1);
    }
    
    if (!writeJournalEntry(slot, entry)) {
        return 0;
    }
    
    return entry.transactionId;
}

bool Journal::commitTransaction(uint32_t transactionId) {
    // Find transaction
    JournalEntry entry;
    uint32_t entriesPerBlock = BLOCK_SIZE / JOURNAL_ENTRY_SIZE;
    uint32_t maxEntries = journalBlockCount_ * entriesPerBlock;
    
    for (uint32_t i = 0; i < maxEntries; ++i) {
        if (readJournalEntry(i, entry) && entry.transactionId == transactionId) {
            entry.committed = 1;
            return writeJournalEntry(i, entry);
        }
    }
    
    return false;
}

bool Journal::abortTransaction(uint32_t transactionId) {
    // Mark transaction as invalid
    JournalEntry entry;
    uint32_t entriesPerBlock = BLOCK_SIZE / JOURNAL_ENTRY_SIZE;
    uint32_t maxEntries = journalBlockCount_ * entriesPerBlock;
    
    for (uint32_t i = 0; i < maxEntries; ++i) {
        if (readJournalEntry(i, entry) && entry.transactionId == transactionId) {
            entry.reset();
            return writeJournalEntry(i, entry);
        }
    }
    
    return false;
}

std::vector<JournalEntry> Journal::getUncommittedTransactions() {
    std::vector<JournalEntry> uncommitted;
    JournalEntry entry;
    
    uint32_t entriesPerBlock = BLOCK_SIZE / JOURNAL_ENTRY_SIZE;
    uint32_t maxEntries = journalBlockCount_ * entriesPerBlock;
    
    for (uint32_t i = 0; i < maxEntries; ++i) {
        if (readJournalEntry(i, entry) && entry.isValid() && entry.committed == 0) {
            uncommitted.push_back(entry);
        }
    }
    
    return uncommitted;
}

bool Journal::clearJournal() {
    return initializeJournal();
}

bool Journal::readJournalEntry(uint32_t index, JournalEntry& entry) {
    uint32_t entriesPerBlock = BLOCK_SIZE / JOURNAL_ENTRY_SIZE;
    uint32_t blockNum = journalStartBlock_ + (index / entriesPerBlock);
    uint32_t offset = (index % entriesPerBlock) * JOURNAL_ENTRY_SIZE;
    
    if (blockNum >= journalStartBlock_ + journalBlockCount_) {
        return false;
    }
    
    std::vector<uint8_t> buffer(BLOCK_SIZE);
    if (!disk_->readBlock(blockNum, buffer.data())) {
        return false;
    }
    
    memcpy(&entry, buffer.data() + offset, sizeof(JournalEntry));
    return true;
}

bool Journal::writeJournalEntry(uint32_t index, const JournalEntry& entry) {
    uint32_t entriesPerBlock = BLOCK_SIZE / JOURNAL_ENTRY_SIZE;
    uint32_t blockNum = journalStartBlock_ + (index / entriesPerBlock);
    uint32_t offset = (index % entriesPerBlock) * JOURNAL_ENTRY_SIZE;
    
    if (blockNum >= journalStartBlock_ + journalBlockCount_) {
        return false;
    }
    
    // Read-modify-write
    std::vector<uint8_t> buffer(BLOCK_SIZE);
    if (!disk_->readBlock(blockNum, buffer.data())) {
        return false;
    }
    
    memcpy(buffer.data() + offset, &entry, sizeof(JournalEntry));
    return disk_->writeBlock(blockNum, buffer.data());
}

uint32_t Journal::findFreeJournalSlot() {
    JournalEntry entry;
    uint32_t entriesPerBlock = BLOCK_SIZE / JOURNAL_ENTRY_SIZE;
    uint32_t maxEntries = journalBlockCount_ * entriesPerBlock;
    
    for (uint32_t i = 0; i < maxEntries; ++i) {
        if (readJournalEntry(i, entry) && !entry.isValid()) {
            return i;
        }
    }
    
    return UINT32_MAX;
}

} // namespace FileSystemTool
