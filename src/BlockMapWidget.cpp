#include "BlockMapWidget.h"
#include "FileSystem.h"
#include <QPainter>
#include <QMouseEvent>
#include <QToolTip>
#include <cmath>

namespace FileSystemTool {

BlockMapWidget::BlockMapWidget(QWidget *parent) 
    : QWidget(parent), fileSystem_(nullptr), totalBlocks_(0), 
      hoveredBlock_(UINT32_MAX), zoomLevel_(1.0), blocksPerRow_(64),
      blockDisplaySize_(10), blockSpacing_(1) {
    
   setMinimumSize(600, 400);
    setMouseTracking(true);
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
    
    int cellSize = blockDisplaySize_ + blockSpacing_;
    blocksPerRow_ = std::max(1, width() / cellSize);
    
    int blockCols = blocksPerRow_; // Renamed for clarity, same as blocksPerRow_
    int blockSize = blockDisplaySize_; // Renamed for clarity, same as blockDisplaySize_
    int gap = blockSpacing_; // Renamed for clarity, same as blockSpacing_
    
    uint32_t totalBlocks = blockStates_.size(); // Use actual size for iteration limit
    uint32_t blockNum = 0; // Initialize block number for the loop
    
    for (size_t row = 0; ; ++row) { // Loop through rows
        if (row * cellSize > height()) break; // Stop if row is out of bounds
        
        for (int col = 0; col < blockCols && blockNum < totalBlocks; ++col, ++blockNum) {
            int x = col * (blockSize + gap);
            int y = row * (blockSize + gap);
            
            BlockState state = blockStates_[blockNum]; // Use blockStates_ directly
            QColor color = getBlockColor(state);
            
            // All USED blocks (files) are red - no per-file colors
            // This makes fragmentation more visible
            
            if (blockNum == hoveredBlock_) { // Use blockNum for hovered check
                color = color.lighter(150);
            }
            
            painter.setBrush(color);
            painter.setPen(QPen(color.darker(120), 1));
            painter.drawRect(x, y, blockSize, blockSize);
        }
        if (blockNum >= totalBlocks) break; // Stop if all blocks are drawn
    }
}

void BlockMapWidget::mouseMoveEvent(QMouseEvent *event) {
    if (blockStates_.empty()) return;
    
    int cellSize = blockDisplaySize_ + blockSpacing_;
    int col = event->pos().x() / cellSize;
    int row = event->pos().y() / cellSize;
    
    uint32_t blockNum = row * blocksPerRow_ + col;
    
    if (blockNum < blockStates_.size()) {
        if (hoveredBlock_ != blockNum) {
            hoveredBlock_ = blockNum;
            update();
            
            BlockState state = blockStates_[blockNum];
            QString tooltip = QString("Block %1: %2")
                            .arg(blockNum)
                            .arg(getBlockStateText(state));
            
            // For USED blocks, add inode and filename info
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
    
    // Query in-memory bitmap state (isBlockFree checks bitmap_ directly)
    // This shows current state, not stale disk state
    for (uint32_t i = 0; i < totalBlocks_; ++i) {
        blockStates_.push_back(getBlockState(i));
    }
}

BlockState BlockMapWidget::getBlockState(uint32_t blockNum) {
    if (!fileSystem_) return BlockState::FREE;
    
    const auto& sb = fileSystem_->getDisk()->getSuperblock();
    
    // Superblock
    if (blockNum == 0) {
        return BlockState::SUPERBLOCK;
    }
    
    // Bitmap
    if (blockNum >= sb.bitmapStart && blockNum < sb.inodeTableStart) {
        return BlockState::INODE_TABLE;
    }
    
    // Inode table
    if (blockNum >= sb.inodeTableStart && blockNum < sb.journalStart) {
        return BlockState::INODE_TABLE;
    }
    
    // Journal
    if (blockNum >= sb.journalStart && blockNum < sb.dataBlocksStart) {
        return BlockState::JOURNAL;
    }
    
    // Data blocks
    if (fileSystem_->getDisk()->isBlockFree(blockNum)) {
        return BlockState::FREE;
    } else {
        return BlockState::USED;
    }
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
            return QColor(241, 196, 15); // Yellow
        case BlockState::JOURNAL:
            return QColor(52, 152, 219); // Blue
        case BlockState::INODE_TABLE:
            return QColor(155, 89, 182); // Purple
        case BlockState::SUPERBLOCK:
            return QColor(26, 188, 156); // Teal
        default:
            return QColor(127, 127, 127); // Gray
    }
}

QColor BlockMapWidget::getFileColor(uint32_t inodeNum) {
    // Generate unique red shade from inode number
    // Use HSL color space for consistent red hues with varying saturation/lightness
    int hue = 0; // Red base (0 degrees in HSL)
    
    // Vary saturation and lightness based on inode number
    // This creates visually distinct red shades
    int saturation = 150 + (inodeNum * 37) % 100;  // Range: 150-250
    int lightness = 80 + (inodeNum * 53) % 60;     // Range: 80-140
    
    return QColor::fromHsl(hue, saturation, lightness);
}

QString BlockMapWidget::getBlockStateText(BlockState state) {
    switch (state) {
        case BlockState::FREE: return "Free";
        case BlockState::USED: return "Used";
        case BlockState::CORRUPTED: return "Corrupted";
        case BlockState::JOURNAL: return "Journal";
        case BlockState::INODE_TABLE: return "Inode Table";
        case BlockState::SUPERBLOCK: return "Superblock";
        default: return "Unknown";
    }
}

int BlockMapWidget::getBlockSize() const {
    return blockDisplaySize_;
}

} // namespace FileSystemTool
