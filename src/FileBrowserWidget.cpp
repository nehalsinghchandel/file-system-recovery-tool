#include "FileBrowserWidget.h"
#include "FileSystem.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QPushButton>
#include <QLabel>
#include <QFrame>
#include <QSet>

namespace FileSystemTool {

FileBrowserWidget::FileBrowserWidget(QWidget *parent) 
    : QWidget(parent), fileSystem_(nullptr), currentPath_("/") {
    setupUI();
}

void FileBrowserWidget::setFileSystem(FileSystem* fs) {
    fileSystem_ = fs;
    refresh();
}

void FileBrowserWidget::refresh() {
    if (!fileSystem_ || !fileSystem_->isMounted()) {
        fileTable_->setRowCount(0);
        return;
    }
    
    loadDirectory(currentPath_);
}

void FileBrowserWidget::navigateToPath(const QString& path) {
    loadDirectory(path);
}

QStringList FileBrowserWidget::getSelectedFiles() const {
    QStringList files;
    auto selectedItems = fileTable_->selectedItems();
    QSet<int> uniqueRows;
    
    for (auto* item : selectedItems) {
        uniqueRows.insert(item->row());
    }
    
    for (int row : uniqueRows) {
        auto* nameItem = fileTable_->item(row, COL_NAME);
        if (nameItem) {
            QString name = nameItem->text();
            // Prepend currentPath_ to get full path
            QString fullPath = currentPath_;
            if (!fullPath.endsWith('/')) {
                fullPath += '/';
            }
            fullPath += name;
            files.append(fullPath);
        }
    }
    
    return files;
}

void FileBrowserWidget::triggerDelete() {
    onDeleteClicked();
}

void FileBrowserWidget::setupUI() {
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    
    // Top bar - just path and refresh
    QHBoxLayout* topLayout = new QHBoxLayout();
    QLabel* pathLabel = new QLabel(QString("Path: /"), this);
    refreshBtn_ = new QPushButton("Refresh", this);
    topLayout->addWidget(pathLabel);
    topLayout->addStretch();
    topLayout->addWidget(refreshBtn_);
    
    // File table
    fileTable_ = new QTableWidget(this);
    fileTable_->setColumnCount(5);
    fileTable_->setHorizontalHeaderLabels({"Name", "Type", "Size", "Fragments", "Inode"});
    fileTable_->horizontalHeader()->setStretchLastSection(true);
    fileTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    fileTable_->setSelectionMode(QAbstractItemView::ExtendedSelection);  // Enable multi-select
    fileTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    
    mainLayout->addLayout(topLayout);
    mainLayout->addWidget(fileTable_);
    
    // Connect signals
    connect(refreshBtn_, &QPushButton::clicked, this, &FileBrowserWidget::onRefreshClicked);
    connect(fileTable_, SIGNAL(itemDoubleClicked(QTableWidgetItem*)), 
            this, SLOT(onTableItemDoubleClicked(QTableWidgetItem*)));
    
    // Enable context menu
    fileTable_->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(fileTable_, &QTableWidget::customContextMenuRequested,
            this, &FileBrowserWidget::onContextMenu);
}

void FileBrowserWidget::loadDirectory(const QString& path) {
    currentPath_ = path;
    
    if (!fileSystem_ || !fileSystem_->isMounted()) {
        fileTable_->setRowCount(0);
        return;
    }
    
    // List directory contents
    auto entries = fileSystem_->listDir(path.toStdString());
    populateTable(entries);
}

uint32_t FileBrowserWidget::calculateFragments(const DirectoryEntry& entry) {
    if (!fileSystem_ || entry.fileType != static_cast<uint8_t>(FileType::REGULAR_FILE)) {
        return 0; // Only calculate for regular files
    }

    Inode inode;
    if (fileSystem_->getInodeManager()->readInode(entry.inodeNumber, inode)) {
        auto blocks = fileSystem_->getInodeManager()->getInodeBlocks(inode);
        if (blocks.empty()) return 0;

        uint32_t fragments = 1;
        for (size_t j = 1; j < blocks.size(); ++j) {
            if (blocks[j] != blocks[j-1] + 1) {
                fragments++;
            }
        }
        return fragments;
    }
    return 0;
}

void FileBrowserWidget::populateTable(const std::vector<DirectoryEntry>& entries) {
    fileTable_->setRowCount(0); // Clear existing rows
    fileTable_->setRowCount(entries.size());
    
    for (size_t i = 0; i < entries.size(); ++i) {
        const auto& entry = entries[i];
        
        // Name
        QTableWidgetItem* nameItem = new QTableWidgetItem(
            QString::fromStdString(entry.getName()));
        fileTable_->setItem(i, COL_NAME, nameItem);
        
        // Type
        QString type = (entry.fileType == static_cast<uint8_t>(FileType::DIRECTORY)) ? 
                       "Directory" : "File";
        QTableWidgetItem* typeItem = new QTableWidgetItem(type);
        fileTable_->setItem(i, COL_TYPE, typeItem);
        
        // Size
        Inode inode;
        if (fileSystem_->getInodeManager()->readInode(entry.inodeNumber, inode)) {
            QTableWidgetItem* sizeItem = new QTableWidgetItem(
                QString::number(inode.fileSize));
            fileTable_->setItem(i, COL_SIZE, sizeItem);
            
            // Fragments (only for files)
            if (inode.fileType == FileType::REGULAR_FILE) {
                auto blocks = fileSystem_->getInodeManager()->getInodeBlocks(inode);
                int fragments = 1;
                for (size_t j = 1; j < blocks.size(); ++j) {
                    if (blocks[j] != blocks[j-1] + 1) {
                        fragments++;
                    }
                }
                QTableWidgetItem* fragItem = new QTableWidgetItem(QString::number(fragments));
                fileTable_->setItem(i, COL_FRAGMENTS, fragItem);
            } else {
                QTableWidgetItem* fragItem = new QTableWidgetItem("N/A");
                fileTable_->setItem(i, COL_FRAGMENTS, fragItem);
            }
        }
        
        // Inode
        QTableWidgetItem* inodeItem = new QTableWidgetItem(
            QString::number(entry.inodeNumber));
        fileTable_->setItem(i, COL_INODE, inodeItem);
    }
}

void FileBrowserWidget::onTreeItemClicked(QTreeWidgetItem* item, int column) {
    // Directory tree navigation (if implemented)
}

void FileBrowserWidget::onTableItemDoubleClicked(QTableWidgetItem* item) {
    int row = item->row();
    QString name = fileTable_->item(row, COL_NAME)->text();
    QString type = fileTable_->item(row, COL_TYPE)->text();
    
    if (type == "Directory") {
        // Navigate into directory
        if (name == "..") {
            // Go up one level
            int lastSlash = currentPath_.lastIndexOf('/', currentPath_.length() - 2);
            if (lastSlash > 0) {
                currentPath_ = currentPath_.left(lastSlash + 1);
            } else {
                currentPath_ = "/";
            }
        } else if (name != ".") {
            // Go into subdirectory
            if (!currentPath_.endsWith('/')) {
                currentPath_ += '/';
            }
            currentPath_ += name;
        }
        
        refresh();
        emit directoryChanged(currentPath_);
    } else {
        // File selected
        QString fullPath = currentPath_;
        if (!fullPath.endsWith('/')) {
            fullPath += '/';
        }
        fullPath += name;
        emit fileDoubleClicked(fullPath);
    }
}

void FileBrowserWidget::onRefreshClicked() {
    refresh();
}

void FileBrowserWidget::onDeleteClicked() {
    auto selectedItems = fileTable_->selectedItems();
    if (selectedItems.isEmpty()) {
        QMessageBox::information(this, "No Selection", "Please select file(s) to delete");
        return;
    }
    
    // Get unique rows (selected items includes all columns)
    QSet<int> selectedRows;
    for (auto* item : selectedItems) {
        selectedRows.insert(item->row());
    }
    
    // Build list of files to delete (skip . and ..)
    QStringList filesToDelete;
    for (int row : selectedRows) {
        QString name = fileTable_->item(row, COL_NAME)->text();
        if (name != "." && name != "..") {
            filesToDelete.append(name);
        }
    }
    
    if (filesToDelete.isEmpty()) {
        QMessageBox::warning(this, "Cannot Delete", "Cannot delete current or parent directory entries");
        return;
    }
    
    // Confirm deletion
    QString message = (filesToDelete.size() == 1) 
        ? QString("Are you sure you want to delete '%1'?").arg(filesToDelete[0])
        : QString("Are you sure you want to delete %1 files?").arg(filesToDelete.size());
    
    auto reply = QMessageBox::question(this, "Confirm Delete", message,
        QMessageBox::Yes | QMessageBox::No);
    
    if (reply != QMessageBox::Yes) {
        return;
    }
    
    // Delete files
    int successCount = 0;
    int failCount = 0;
    
    for (const QString& name : filesToDelete) {
        QString fullPath = currentPath_;
        if (!fullPath.endsWith('/')) {
            fullPath += '/';
        }
        fullPath += name;
        
        if (fileSystem_ && fileSystem_->deleteFile(fullPath.toStdString())) {
            successCount++;
            emit fileDeleted(fullPath);
        } else {
            failCount++;
        }
    }
    
    // Show results if multiple files
    if (filesToDelete.size() > 1) {
        QString result = QString("Deleted: %1 files\nFailed: %2 files")
            .arg(successCount).arg(failCount);
        QMessageBox::information(this, "Delete Complete", result);
    } else if (failCount > 0) {
        QMessageBox::critical(this, "Error", "Failed to delete file");
    }
    
    refresh();
}

void FileBrowserWidget::onContextMenu(const QPoint& pos) {
    QTableWidgetItem* item = fileTable_->itemAt(pos);
    if (!item) return;
    
    int row = item->row();
    QString name = fileTable_->item(row, COL_NAME)->text();
    
    // Don't show context menu for . and ..
    if (name == "." || name == "..") return;
    
    QMenu contextMenu(this);
    QAction* deleteAction = contextMenu.addAction("Delete");
    deleteAction->setIcon(QIcon::fromTheme("edit-delete"));
    
    QAction* selectedAction = contextMenu.exec(fileTable_->viewport()->mapToGlobal(pos));
    if (selectedAction == deleteAction) {
        fileTable_->selectRow(row);
        onDeleteClicked();
    }
}

} // namespace FileSystemTool
