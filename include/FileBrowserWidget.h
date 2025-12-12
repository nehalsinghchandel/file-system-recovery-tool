#ifndef FILEBROWSERWIDGET_H
#define FILEBROWSERWIDGET_H

#include <QWidget>
#include <QTreeWidget>
#include <QTableWidget>
#include <QLabel>
#include <QPushButton>
#include <QSplitter>
#include <QMenu>
#include <QMessageBox>
#include "Directory.h"

namespace FileSystemTool {

class FileSystem;

struct FileInfo {
    QString name;
    QString type;
    uint64_t size;
    uint32_t inodeNum;
    bool isDirectory;
    int fragmentCount;
};

class FileBrowserWidget : public QWidget {
    Q_OBJECT

public:
    explicit FileBrowserWidget(QWidget *parent = nullptr);
    
    void setFileSystem(FileSystem* fs);
    void refresh();
    void navigateToPath(const QString& path);

signals:
    void fileSelected(const QString& path);
    void fileDoubleClicked(const QString& path);
    void directoryChanged(const QString& path);
    void fileDeleted(const QString& path);

private slots:
    void onTreeItemClicked(QTreeWidgetItem* item, int column);
    void onTableItemDoubleClicked(QTableWidgetItem* item);
    void onRefreshClicked();
    void onDeleteClicked();
    void onContextMenu(const QPoint& pos);

private:
    void setupUI();
    void loadDirectory(const QString& path);
    void populateTable(const std::vector<DirectoryEntry>& entries);
    void updateFileDetails(const FileInfo& info);
    
    FileSystem* fileSystem_;
    QString currentPath_;
    
    // UI components
    QTreeWidget* directoryTree_;
    QTableWidget* fileTable_;
    QLabel* detailsLabel_;
    QPushButton* refreshBtn_;
    QPushButton* deleteBtn_;
    
    // Columns in file table
    enum FileTableColumn {
        COL_NAME = 0,
        COL_TYPE,
        COL_SIZE,
        COL_FRAGMENTS,
        COL_INODE
    };
};

} // namespace FileSystemTool

#endif // FILEBROWSERWIDGET_H
