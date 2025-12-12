#ifndef BLOCKMAPWIDGET_H
#define BLOCKMAPWIDGET_H

#include <QWidget>
#include <QScrollArea>
#include <vector>
#include <cstdint>

namespace FileSystemTool {

class FileSystem;

enum class BlockState {
    FREE,
    USED,
    CORRUPTED,
    JOURNAL,
    INODE_TABLE,
    SUPERBLOCK
};

class BlockMapWidget : public QWidget {
    Q_OBJECT

public:
    explicit BlockMapWidget(QWidget *parent = nullptr);
    
    void setFileSystem(FileSystem* fs);
    void refresh();
    void setZoomLevel(double zoom);
    
    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

signals:
    void blockSelected(uint32_t blockNum, BlockState state);
    void blockHovered(uint32_t blockNum, BlockState state);

private:
    void updateBlockStates();
    BlockState getBlockState(uint32_t blockNum);
    QColor getBlockColor(BlockState state);
    QString getBlockStateText(BlockState state);
    int getBlockSize() const;
    
    FileSystem* fileSystem_;
    std::vector<BlockState> blockStates_;
    uint32_t totalBlocks_;
    uint32_t hoveredBlock_;
    double zoomLevel_;
    
    // Layout
    int blocksPerRow_;
    int blockDisplaySize_;
    int blockSpacing_;
};

} // namespace FileSystemTool

#endif // BLOCKMAPWIDGET_H
