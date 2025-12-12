#include "MainWindow.h"
#include "BlockMapWidget.h"
#include "PerformanceWidget.h"
#include "ControlPanel.h"
#include "FileBrowserWidget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QMessageBox>
#include <QFileDialog>
#include <QDir>
#include <QCloseEvent>
#include <iostream>

namespace FileSystemTool {

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
    setupUI();
    setupMenuBar();
    connectSignals();
    
    statusLabel_ = new QLabel("No disk mounted", this);
    statusBar()->addWidget(statusLabel_);
    
    setWindowTitle("File System Recovery Tool");
}

MainWindow::~MainWindow() {
    if (fileSystem_ && fileSystem_->isMounted()) {
        fileSystem_->unmountFileSystem();
    }
}

void MainWindow::setupUI() {
    // Create central widget
    QWidget* centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);
    
    // Create main layout
    QVBoxLayout* mainLayout = new QVBoxLayout(centralWidget);
    
    // Create widgets
    blockMapWidget_ = new BlockMapWidget(this);
    performanceWidget_ = new PerformanceWidget(this);
    controlPanel_ = new ControlPanel(this);
    fileBrowserWidget_ = new FileBrowserWidget(this);
    
    // Create splitters
    mainSplitter_ = new QSplitter(Qt::Horizontal, this);
    rightSplitter_ = new QSplitter(Qt::Vertical, this);
    
    // Left side: Block map
    blockMapWidget_->setMinimumSize(400, 300);
    mainSplitter_->addWidget(blockMapWidget_);
    
    // Right side: Upper (performance + browser), Lower (control panel)
    QSplitter* upperRight = new QSplitter(Qt::Horizontal, this);
    performanceWidget_->setMinimumSize(200, 200);
    fileBrowserWidget_->setMinimumSize(200, 200);
    upperRight->addWidget(performanceWidget_);
    upperRight->addWidget(fileBrowserWidget_);
    
    controlPanel_->setMaximumHeight(350);  // Limit control panel height
    rightSplitter_->addWidget(upperRight);
    rightSplitter_->addWidget(controlPanel_);
    
    // Set initial sizes for right splitter (upper:lower ratio)
    rightSplitter_->setStretchFactor(0, 2);  // Upper gets 2 parts
    rightSplitter_->setStretchFactor(1, 1);  // Lower gets 1 part
    
    mainSplitter_->addWidget(rightSplitter_);
    
    // Set sizes for main splitter
    mainSplitter_->setStretchFactor(0, 2);  // Block map
    mainSplitter_->setStretchFactor(1, 1);  // Right side
    
    mainLayout->addWidget(mainSplitter_);
    mainLayout->setContentsMargins(5, 5, 5, 5);
}

void MainWindow::setupMenuBar() {
    QMenu* fileMenu = menuBar()->addMenu("&File");
    
    QAction* newDiskAction = fileMenu->addAction("&New Disk...");
    connect(newDiskAction, &QAction::triggered, this, &MainWindow::onNewDisk);
    
    QAction* openDiskAction = fileMenu->addAction("&Open Disk...");
    connect(openDiskAction, &QAction::triggered, this, &MainWindow::onOpenDisk);
    
    QAction* closeDiskAction = fileMenu->addAction("&Close Disk");
    connect(closeDiskAction, &QAction::triggered, this, &MainWindow::onCloseDisk);
    
    fileMenu->addSeparator();
    
    QAction* exitAction = fileMenu->addAction("E&xit");
    connect(exitAction, &QAction::triggered, this, &MainWindow::onExit);
    
    QMenu* helpMenu = menuBar()->addMenu("&Help");
    QAction* aboutAction = helpMenu->addAction("&About");
    connect(aboutAction, &QAction::triggered, this, &MainWindow::onAbout);
}

void MainWindow::connectSignals() {
    // Connect control panel's operationCompleted signal to update all widgets
    connect(controlPanel_, &ControlPanel::operationCompleted, this, [this]() {
        updateAllWidgets();
        updateStatusBar();
    });
}

void MainWindow::onNewDisk() {
    std::cout << "onNewDisk() called" << std::endl;
    
    // Use Qt's file dialog (not native) to avoid macOS permissions issues
    QString filename = QFileDialog::getSaveFileName(
        this, 
        "Create New Disk",
        QDir::homePath() + "/disk.bin",  // Default to home directory
        "Disk Files (*.bin)",
        nullptr,
        QFileDialog::DontUseNativeDialog  // Use Qt dialog instead of macOS native
    );
    std::cout << "Selected file: " << filename.toStdString() << std::endl;
    
    if (filename.isEmpty()) {
        std::cout << "No file selected, returning" << std::endl;
        return;
    }
    
    try {
        currentDiskPath_ = filename;
        fileSystem_ = std::make_unique<FileSystem>(filename.toStdString());
        
        std::cout << "Creating file system..." << std::endl;
        if (!fileSystem_->createFileSystem()) {
            std::cerr << "Failed to create file system" << std::endl;
            QMessageBox::critical(this, "Error", "Failed to create file system");
            fileSystem_.reset();
            return;
        }
        std::cout << "File system created successfully" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Exception during disk creation: " << e.what() << std::endl;
        QMessageBox::critical(this, "Error", QString("Exception: %1").arg(e.what()));
        fileSystem_.reset();
        return;
    }
    
    // Create managers
    recoveryMgr_ = std::make_unique<RecoveryManager>(fileSystem_.get());
    defragMgr_ = std::make_unique<DefragManager>(fileSystem_.get());
    
    // Update widgets
    blockMapWidget_->setFileSystem(fileSystem_.get());
    performanceWidget_->setFileSystem(fileSystem_.get());
    performanceWidget_->setDefragManager(defragMgr_.get());
    controlPanel_->setFileSystem(fileSystem_.get());
    controlPanel_->setRecoveryManager(recoveryMgr_.get());
    controlPanel_->setDefragManager(defragMgr_.get());
    fileBrowserWidget_->setFileSystem(fileSystem_.get());
    
    updateAllWidgets();
    updateStatusBar();
    
    QMessageBox::information(this, "Success", "File system created successfully");
}

void MainWindow::onOpenDisk() {
    QString filename = QFileDialog::getOpenFileName(
        this, 
        "Open Disk",
        QDir::homePath(),  // Start in home directory
        "Disk Files (*.bin)",
        nullptr,
        QFileDialog::DontUseNativeDialog  // Use Qt dialog
    );
    if (filename.isEmpty()) {
        return;
    }
    
    currentDiskPath_ = filename;
    fileSystem_ = std::make_unique<FileSystem>(filename.toStdString());
    
    if (!fileSystem_->mountFileSystem()) {
        QMessageBox::critical(this, "Error", "Failed to mount file system");
        fileSystem_.reset();
        return;
    }
    
    // Create managers
    recoveryMgr_ = std::make_unique<RecoveryManager>(fileSystem_.get());
    defragMgr_ = std::make_unique<DefragManager>(fileSystem_.get());
    
    // Update widgets
    blockMapWidget_->setFileSystem(fileSystem_.get());
    performanceWidget_->setFileSystem(fileSystem_.get());
    performanceWidget_->setDefragManager(defragMgr_.get());
    controlPanel_->setFileSystem(fileSystem_.get());
    controlPanel_->setRecoveryManager(recoveryMgr_.get());
    controlPanel_->setDefragManager(defragMgr_.get());
    fileBrowserWidget_->setFileSystem(fileSystem_.get());
    
    updateAllWidgets();
    updateStatusBar();
    
    QMessageBox::information(this, "Success", "File system mounted successfully");
}

void MainWindow::onCloseDisk() {
    if (fileSystem_ && fileSystem_->isMounted()) {
        if (!confirmDiskClose()) {
            return;
        }
        
        fileSystem_->unmountFileSystem();
        fileSystem_.reset();
        recoveryMgr_.reset();
        defragMgr_.reset();
        
        updateAllWidgets();
        updateStatusBar();
    }
}

void MainWindow::onExit() {
    close();
}

void MainWindow::onAbout() {
    QMessageBox::about(this, "About File System Recovery Tool",
        "File System Recovery Tool v1.0\n\n"
        "A user-space file system simulator with:\n"
        "• Crash recovery and journaling\n"
        "• Defragmentation optimization\n"
        "• Real-time visualization\n\n"
        "Built with C++ and Qt");
}

void MainWindow::updateStatusBar() {
    if (fileSystem_ && fileSystem_->isMounted()) {
        uint32_t used = fileSystem_->getUsedBlocks();
        uint32_t total = fileSystem_->getTotalBlocks();
        double percent = (static_cast<double>(used) / total) * 100.0;
        
        QString status = QString("Disk: %1 | Used: %2/%3 blocks (%4%)")
                         .arg(currentDiskPath_)
                         .arg(used)
                         .arg(total)
                         .arg(percent, 0, 'f', 1);
        statusLabel_->setText(status);
    } else {
        statusLabel_->setText("No disk mounted");
    }
}

void MainWindow::updateAllWidgets() {
    if (fileSystem_ && fileSystem_->isMounted()) {
        blockMapWidget_->refresh();
        performanceWidget_->updateMetrics();
        fileBrowserWidget_->refresh();
        controlPanel_->setDiskMounted(true);
    } else {
        controlPanel_->setDiskMounted(false);
    }
}

bool MainWindow::confirmDiskClose() {
    auto reply = QMessageBox::question(this, "Close Disk", 
                                        "Are you sure you want to close the current disk?",
                                        QMessageBox::Yes | QMessageBox::No);
    return reply == QMessageBox::Yes;
}

void MainWindow::closeEvent(QCloseEvent *event) {
    if (fileSystem_ && fileSystem_->isMounted()) {
        if (confirmDiskClose()) {
            fileSystem_->unmountFileSystem();
            event->accept();
        } else {
            event->ignore();
        }
    } else {
        event->accept();
    }
}

} // namespace FileSystemTool
