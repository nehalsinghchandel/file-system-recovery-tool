#ifndef BLOCKMAPWIDGET_H
#define BLOCKMAPWIDGET_H

#include <QWidget>
#include <QPainter>
#include <QMouseEvent>
#include <vector>
#include "FileSystem.h"

namespace FileSystemTool {

class FileSystem;

enum class BlockState {
    FREE,
    USED,
    CORRUPTED,      // For power cut simulation
    SUPERBLOCK,
    INODE_TABLE,
    JOURNAL
};

class BlockMapWidget : public QWidget {
    Q_OBJECT

public:
    explicit BlockMapWidget(QWidget *parent = nullptr);
    
    void setFileSystem(FileSystem* fs);
    void refresh();
    void setZoomLevel(double zoom);
    
    QSize sizeHint() const override;

signals:
    void blockSelected(uint32_t blockNum, BlockState state);
    void blockHovered(uint32_t blockNum, BlockState state);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    QColor getBlockColor(BlockState state);
    QColor getFileColor(uint32_t blockNum);
    void updateBlockStates();
    BlockState getBlockState(uint32_t blockNum);
    QString getBlockStateText(BlockState state);
    int getBlockSize() const;
    
    FileSystem* fileSystem_;
    std::vector<BlockState> blockStates_;
    uint32_t totalBlocks_;
    uint32_t hoveredBlock_;
    int blockDisplaySize_;
    double zoomLevel_;
    int blocksPerRow_;  // Store calculated blocks per row for hover
    int blockSpacing_;
};

} // namespace FileSystemTool

#endif // BLOCKMAPWIDGET_H
