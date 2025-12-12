#include "FileBrowserWidget.h"
#include "FileSystem.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QPushButton>
#include <QLabel>
#include <QFrame>

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
        detailsLabel_->setText("No disk mounted");
        return;
    }
    
    loadDirectory(currentPath_);
}

void FileBrowserWidget::navigateToPath(const QString& path) {
    currentPath_ = path;
    refresh();
}

void FileBrowserWidget::setupUI() {
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    
    // Top bar
    QHBoxLayout* topLayout = new QHBoxLayout();
    QLabel* pathLabel = new QLabel(QString("Path: /"), this);
    refreshBtn_ = new QPushButton("Refresh", this);
    deleteBtn_ = new QPushButton("Delete Selected", this);
    deleteBtn_->setStyleSheet("background-color: #e74c3c; color: white;");
    topLayout->addWidget(pathLabel);
    topLayout->addStretch();
    topLayout->addWidget(deleteBtn_);
    topLayout->addWidget(refreshBtn_);
    
    // File table
    fileTable_ = new QTableWidget(this);
    fileTable_->setColumnCount(5);
    fileTable_->setHorizontalHeaderLabels({"Name", "Type", "Size", "Fragments", "Inode"});
    fileTable_->horizontalHeader()->setStretchLastSection(true);
    fileTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    fileTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    
    // Details label
    detailsLabel_ = new QLabel(QString("No file selected"), this);
    detailsLabel_->setFrameStyle(QFrame::Panel | QFrame::Sunken);
    detailsLabel_->setMinimumHeight(60);
    
    mainLayout->addLayout(topLayout);
    mainLayout->addWidget(fileTable_);
    mainLayout->addWidget(detailsLabel_);
    
    // Connect signals
    connect(refreshBtn_, &QPushButton::clicked, this, &FileBrowserWidget::onRefreshClicked);
    connect(deleteBtn_, &QPushButton::clicked, this, &FileBrowserWidget::onDeleteClicked);
    connect(fileTable_, SIGNAL(itemDoubleClicked(QTableWidgetItem*)), 
            this, SLOT(onTableItemDoubleClicked(QTableWidgetItem*)));
    
    // Enable context menu
    fileTable_->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(fileTable_, &QTableWidget::customContextMenuRequested,
            this, &FileBrowserWidget::onContextMenu);
}

void FileBrowserWidget::loadDirectory(const QString& path) {
    if (!fileSystem_) return;
    
    auto entries = fileSystem_->listDir(path.toStdString());
    populateTable(entries);
}

void FileBrowserWidget::populateTable(const std::vector<DirectoryEntry>& entries) {
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

void FileBrowserWidget::updateFileDetails(const FileInfo& info) {
    QString details = QString("Name: %1 | Type: %2 | Size: %3 bytes | Fragments: %4 | Inode: %5")
                     .arg(info.name)
                     .arg(info.type)
                     .arg(info.size)
                     .arg(info.fragmentCount)
                     .arg(info.inodeNum);
    detailsLabel_->setText(details);
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
        QMessageBox::information(this, "No Selection", "Please select a file to delete");
        return;
    }
    
    int row = fileTable_->currentRow();
    if (row < 0) return;
    
    QString name = fileTable_->item(row, COL_NAME)->text();
    QString type = fileTable_->item(row, COL_TYPE)->text();
    
    // Don't allow deleting "." or ".."
    if (name == "." || name == "..") {
        QMessageBox::warning(this, "Cannot Delete", "Cannot delete current or parent directory entries");
        return;
    }
    
    // Confirm deletion
    auto reply = QMessageBox::question(this, "Confirm Delete",
        QString("Are you sure you want to delete '%1'?").arg(name),
        QMessageBox::Yes | QMessageBox::No);
    
    if (reply != QMessageBox::Yes) {
        return;
    }
    
    // Build full path
    QString fullPath = currentPath_;
    if (!fullPath.endsWith('/')) {
        fullPath += '/';
    }
    fullPath += name;
    
    // Delete the file
    if (fileSystem_ && fileSystem_->deleteFile(fullPath.toStdString())) {
        refresh();
        emit fileDeleted(fullPath);
    } else {
        QMessageBox::critical(this, "Error", "Failed to delete file");
    }
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
