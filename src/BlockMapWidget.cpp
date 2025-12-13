#include "BlockMapWidget.h"
#include "FileSystem.h"
#include <QPainter>
#include <QMouseEvent>
#include <QToolTip>
#include <cmath>

namespace FileSystemTool {

BlockMapWidget::BlockMapWidget(QWidget *parent) 
    : QWidget(parent), fileSystem_(nullptr), totalBlocks_(0), 
      hoveredBlock_(UINT32_MAX), blockDisplaySize_(10), zoomLevel_(1.0), blocksPerRow_(0),
      blockSpacing_(1) {
    setMouseTracking(true);
    setMinimumSize(600, 400);
}

void BlockMapWidget::setFileSystem(FileSystem* fs) {
    fileSystem_ = fs;
    refresh();
}

void BlockMapWidget::refresh() {
    if (!fileSystem_ || !fileSystem_->isMounted()) {
        blockStates_.clear();
        totalBlocks_ = 0;
        update();
        return;
    }
    
    updateBlockStates();
    update();
}

void BlockMapWidget::setZoomLevel(double zoom) {
    zoomLevel_ = std::max(0.5, std::min(3.0, zoom));
    blockDisplaySize_ = static_cast<int>(10 * zoomLevel_);
    update();
}

QSize BlockMapWidget::sizeHint() const {
    return QSize(800, 600);
}

void BlockMapWidget::paintEvent(QPaintEvent *event) {
    QPainter painter(this);
    painter.fillRect(rect(), QColor(30, 30, 30));
    
    if (blockStates_.empty()) {
        painter.setPen(Qt::white);
        painter.drawText(rect(), Qt::AlignCenter, "No disk mounted");
        return;
    }
    
    int blockSize = blockDisplaySize_;
    int spacing = blockSpacing_;
    blocksPerRow_ = std::max(1, width() / (blockSize + spacing));
    
    for (size_t i = 0; i < blockStates_.size(); ++i) {
        int row = i / blocksPerRow_;
        int col = i % blocksPerRow_;
        
        int x = col * (blockSize + spacing);
        int y = row * (blockSize + spacing);
        
        QColor color = getFileColor(i);
        painter.fillRect(x, y, blockSize, blockSize, color);
        
        // Highlight hovered block
        if (i == hoveredBlock_) {
            painter.setPen(QPen(Qt::yellow, 2));
            painter.drawRect(x, y, blockSize, blockSize);
        }
    }
}

void BlockMapWidget::mouseMoveEvent(QMouseEvent *event) {
    if (blockStates_.empty()) {
        return;
    }
    
    QPoint pos = event->pos();
    int blockSize = blockDisplaySize_;
    int spacing = 1;
    
    if (blocksPerRow_ > 0) {
        int col = pos.x() / (blockSize + spacing);
        int row = pos.y() / (blockSize + spacing);
        uint32_t blockNum = row * blocksPerRow_ + col;
        
        if (blockNum < blockStates_.size()) {
            hoveredBlock_ = blockNum;
            
            // Build tooltip with block info
            BlockState state = blockStates_[blockNum];
            QString stateStr;
            switch (state) {
                case BlockState::FREE: stateStr = "Free"; break;
                case BlockState::USED: stateStr = "Used"; break;
                case BlockState::SUPERBLOCK: stateStr = "Superblock"; break;
                case BlockState::INODE_TABLE: stateStr = "Inode Table"; break;
                default: stateStr = "Unknown"; break;
            }
            
            QString tooltip = QString("Block: %1 | State: %2").arg(blockNum).arg(stateStr);
            
            // If block is used, try to find owner and filename
            if (state == BlockState::USED && fileSystem_) {
                uint32_t inodeNum = fileSystem_->getBlockOwner(blockNum);
                if (inodeNum != UINT32_MAX) {
                    QString filename = QString::fromStdString(
                        fileSystem_->getFilenameFromInode(inodeNum));
                    
                    if (!filename.isEmpty()) {
                        tooltip += QString(" | Inode: %1 | File: %2")
                                  .arg(inodeNum)
                                  .arg(filename);
                    } else {
                        tooltip += QString(" | Inode: %1").arg(inodeNum);
                    }
                }
            }
            
            QToolTip::showText(event->globalPosition().toPoint(), tooltip);
            
            emit blockHovered(blockNum, state);
            update();
        }
    } else {
        hoveredBlock_ = UINT32_MAX;
        update();
    }
}

void BlockMapWidget::mousePressEvent(QMouseEvent *event) {
    if (hoveredBlock_ != UINT32_MAX && hoveredBlock_ < blockStates_.size()) {
        emit blockSelected(hoveredBlock_, blockStates_[hoveredBlock_]);
    }
}

void BlockMapWidget::resizeEvent(QResizeEvent *event) {
    QWidget::resizeEvent(event);
    update();
}

void BlockMapWidget::updateBlockStates() {
    totalBlocks_ = fileSystem_->getTotalBlocks();
    blockStates_.clear();
    blockStates_.reserve(totalBlocks_);
    
    // Query block states - only query valid blocks
    for (uint32_t i = 0; i < totalBlocks_; ++i) {
        blockStates_.push_back(getBlockState(i));
    }
}

BlockState BlockMapWidget::getBlockState(uint32_t blockNum) {
    if (!fileSystem_ || blockNum >= totalBlocks_) {
        return BlockState::FREE;
    }
    
    const auto& sb = fileSystem_->getDisk()->getSuperblock();
    
    // Check if block is corrupted (from power cut simulation)
    if (fileSystem_->hasCorruption()) {
        const auto& corruptedBlocks = fileSystem_->getCorruptedBlocks();
        if (std::find(corruptedBlocks.begin(), corruptedBlocks.end(), blockNum) != corruptedBlocks.end()) {
            return BlockState::CORRUPTED;
        }
    }
    
    // Superblock
    if (blockNum == 0) {
        return BlockState::SUPERBLOCK;
    }
    
    // Bitmap blocks
    if (blockNum >= sb.bitmapStart && blockNum < sb.inodeTableStart) {
        return BlockState::INODE_TABLE;  // Use same coloras inode for bitmap
    }
    
    // Inode table blocks
    if (blockNum >= sb.inodeTableStart && blockNum < sb.dataBlocksStart) {
        return BlockState::INODE_TABLE;
    }
    
    // Data blocks
    if (blockNum >= sb.dataBlocksStart) {
        bool isFree = fileSystem_->getDisk()->isBlockFree(blockNum);
        return isFree ? BlockState::FREE : BlockState::USED;
    }
    
    return BlockState::FREE;
}

QColor BlockMapWidget::getBlockColor(BlockState state) {
    switch (state) {
        case BlockState::FREE:
            return QColor(40, 180, 99);  // Green
        case BlockState::USED:
            // For USED blocks, check if they belong to a file
            // This will be set from updateBlockStates
            return QColor(231, 76, 60);  // Red (fallback)
        case BlockState::CORRUPTED:
            return QColor(30, 30, 30);   // BLACK - corrupted blocks
        case BlockState::JOURNAL:
            return QColor(52, 152, 219); // Blue
        case BlockState::INODE_TABLE:
            return QColor(155, 89, 182); // Purple
        case BlockState::SUPERBLOCK:
            return QColor(241, 196, 15); // Yellow
        default:
            return QColor(100, 100, 100); // Gray
    }
}

QColor BlockMapWidget::getFileColor(uint32_t blockNum) {
    if (blockNum >= blockStates_.size()) {
        return Qt::gray;
    }
    
    // Use the block state to determine color
    BlockState state = blockStates_[blockNum];
    return getBlockColor(state);
}

QString BlockMapWidget::getBlockStateText(BlockState state) {
    switch (state) {
        case BlockState::FREE: return "Free";
        case BlockState::USED: return "Used";
        case BlockState::CORRUPTED: return "Corrupted";
        case BlockState::SUPERBLOCK: return "Superblock";
        case BlockState::INODE_TABLE: return "Inode Table";
        case BlockState::JOURNAL: return "Journal";
        default: return "Unknown";
    }
}

int BlockMapWidget::getBlockSize() const {
    return blockDisplaySize_;
}

} // namespace FileSystemTool
