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
    
    for (size_t i = 0; i < blockStates_.size(); ++i) {
        int row = i / blocksPerRow_;
        int col = i % blocksPerRow_;
        
        int x = col * cellSize;
        int y = row * cellSize;
        
        if (y > height()) break;
        
        QColor color = getBlockColor(blockStates_[i]);
        if (i == hoveredBlock_) {
            color = color.lighter(150);
        }
        
        painter.fillRect(x, y, blockDisplaySize_, blockDisplaySize_, color);
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
            
            QString tooltip = QString("Block %1: %2")
                            .arg(blockNum)
                            .arg(getBlockStateText(blockStates_[blockNum]));
            QToolTip::showText(event->globalPosition().toPoint(), tooltip);
            
            emit blockHovered(blockNum, blockStates_[blockNum]);
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
            return QColor(231, 76, 60);  // Red
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
