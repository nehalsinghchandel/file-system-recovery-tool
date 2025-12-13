#include "MainWindow.h"
#include "BlockMapWidget.h"
#include "PerformanceWidget.h"
#include "ControlPanel.h"
#include "FileBrowserWidget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QMessageBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFile>
#include <QDir>
#include <QThread>
#include <QApplication>
#include <chrono>
#include <cstdlib>  // for rand()
#include <ctime>    // for time()
#include <QCloseEvent>
#include <iostream>

namespace FileSystemTool {

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), fileSystem_(nullptr), recoveryMgr_(nullptr), defragMgr_(nullptr) {
    
    // Initialize random seed for random data generation
    srand(static_cast<unsigned>(time(nullptr)));
    
    setupUI();
    setupMenuBar();
    connectSignals();
    
    statusLabel_ = new QLabel("No disk mounted", this);
    statusBar()->addWidget(statusLabel_);
    
    setWindowTitle("File System Recovery Tool");
    resize(1400, 900);
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
    
    // Main horizontal layout: [Left Panel] [Center+Bottom] [Right Panel]
    QHBoxLayout* mainLayout = new QHBoxLayout(centralWidget);
    mainLayout->setContentsMargins(5, 5, 5, 5);
    mainLayout->setSpacing(5);
    
    // LEFT PANEL: File Browser + File Operations
    QWidget* leftPanel = new QWidget(this);
    QVBoxLayout* leftLayout = new QVBoxLayout(leftPanel);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    leftLayout->setSpacing(5);
    
    fileBrowserWidget_ = new FileBrowserWidget(this);
    
    // File Operations (moved from control panel)
    QGroupBox* fileOpsGroup = new QGroupBox("File Operations", this);
    QVBoxLayout* fileOpsLayout = new QVBoxLayout(fileOpsGroup);
    fileOpsLayout->setSpacing(8);
    
    controlPanel_ = new ControlPanel(this);  // Still create it for functionality
    controlPanel_->hide();  // Hide the actual widget, we'll use its logic
    
    // Action buttons: Read, Delete (with space-between layout)
    readFileBtn_ = new QPushButton("Read", this);
    readFileBtn_->setStyleSheet("background-color: #3498db; color: white; font-weight: bold;");
    readFileBtn_->setToolTip("Read and display selected file contents");
    
    deleteFileBtn_ = new QPushButton("Delete", this);
    deleteFileBtn_->setStyleSheet("background-color: #e74c3c; color: white; font-weight: bold;");
    deleteFileBtn_->setToolTip("Delete selected file(s) from list above");
    
    QHBoxLayout* actionsLayout = new QHBoxLayout();
    actionsLayout->addWidget(readFileBtn_);
    actionsLayout->addStretch();  // Space between
    actionsLayout->addWidget(deleteFileBtn_);
    
    // Separator
    QFrame* separator1 = new QFrame(this);
    separator1->setFrameShape(QFrame::HLine);
    separator1->setFrameShadow(QFrame::Sunken);
    
    // Create file section (compact)
    QLabel* createLabel = new QLabel("Create New File:", this);
    createLabel->setStyleSheet("font-weight: bold;");
    
    filenameInput_ = new QLineEdit(this);
    filenameInput_->setPlaceholderText("Filename (e.g., /test.txt)");
    
    // File size slider (multiples of 4KB)
    QHBoxLayout* sizeLayout = new QHBoxLayout();
    sizeLayout->addWidget(new QLabel("Size:", this));
    
    fileSizeSlider_ = new QSlider(Qt::Horizontal, this);
    fileSizeSlider_->setMinimum(1);     // 1 = 4KB
    fileSizeSlider_->setMaximum(25);    // 25 = 100KB (25 * 4KB)
    fileSizeSlider_->setValue(1);       // Default 4 KB
    fileSizeSlider_->setTickPosition(QSlider::TicksBelow);
    fileSizeSlider_->setTickInterval(5);  // Tick every 20KB
    
    fileSizeLabel_ = new QLabel("4 KB", this);
    fileSizeLabel_->setMinimumWidth(60);
    fileSizeLabel_->setStyleSheet("font-weight: bold;");
    
    sizeLayout->addWidget(fileSizeSlider_, 1);
    sizeLayout->addWidget(fileSizeLabel_);
    
    createFileBtn_ = new QPushButton("Create", this);
    createFileBtn_->setStyleSheet("background-color: #27ae60; color: white; font-weight: bold;");
    
    // Separator
    QFrame* separator2 = new QFrame(this);
    separator2->setFrameShape(QFrame::HLine);
    separator2->setFrameShadow(QFrame::Sunken);
    
    // Bulk operations (compact)
    QLabel* bulkLabel = new QLabel("Bulk Operations:", this);
    bulkLabel->setStyleSheet("font-weight: bold;");
    
    QHBoxLayout* bulkLayout = new QHBoxLayout();
    bulkLayout->addWidget(new QLabel("Count:", this));
    
    numFilesCombo_ = new QComboBox(this);
    numFilesCombo_->addItems({"10", "25", "50", "100"});
    numFilesCombo_->setMaximumWidth(70);
    
    writeRandomBtn_ = new QPushButton("Write Random Files", this);
    writeRandomBtn_->setStyleSheet("background-color: #f39c12; color: white; font-weight: bold;");
    writeRandomBtn_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    
    bulkLayout->addWidget(numFilesCombo_);
    bulkLayout->addWidget(writeRandomBtn_, 1);
    
    // Add all to file ops layout
    fileOpsLayout->addLayout(actionsLayout);
    fileOpsLayout->addWidget(separator1);
    fileOpsLayout->addWidget(createLabel);
    fileOpsLayout->addWidget(filenameInput_);
    fileOpsLayout->addLayout(sizeLayout);
    fileOpsLayout->addWidget(createFileBtn_);
    fileOpsLayout->addWidget(separator2);
    fileOpsLayout->addWidget(bulkLabel);
    fileOpsLayout->addLayout(bulkLayout);
    
    // Progress bar for file write operations (always visible)
    writeProgressBar_ = new QProgressBar(this);
    writeProgressBar_->setMinimum(0);
    writeProgressBar_->setMaximum(100);
    writeProgressBar_->setValue(0);
    writeProgressBar_->setTextVisible(true);
    writeProgressBar_->setFormat("Ready");
    writeProgressBar_->setFixedHeight(20);
    writeProgressBar_->setStyleSheet(
        "QProgressBar {"
        "   border: 1px solid #999;"
        "   border-radius: 3px;"
        "   background-color: #ddd;"  // Grey pipe background
        "   text-align: center;"
        "}"
        "QProgressBar::chunk {"
        "   background-color: #27ae60;"  // Green fill
        "   border-radius: 2px;"
        "}"
    );
    fileOpsLayout->addWidget(writeProgressBar_);
    
    fileOpsLayout->addStretch();
    
    // Connect slider to label update (slider value * 4 = KB)
    connect(fileSizeSlider_, &QSlider::valueChanged, this, [this](int value) {
        int sizeKB = value * 4;  // Convert to 4KB multiples
        fileSizeLabel_->setText(QString("%1 KB").arg(sizeKB));
    });
    
    leftLayout->addWidget(fileBrowserWidget_, 3);
    leftLayout->addWidget(fileOpsGroup, 1);
    leftPanel->setMinimumWidth(250);
    leftPanel->setMaximumWidth(400);
    
    // CENTER: Vertical layout for Block Map + Console
    QWidget* centerWidget = new QWidget(this);
    QVBoxLayout* centerLayout = new QVBoxLayout(centerWidget);
    centerLayout->setContentsMargins(0, 0, 0, 0);
    centerLayout->setSpacing(5);
    
    blockMapWidget_ = new BlockMapWidget(this);
    blockMapWidget_->setMinimumSize(500, 400);
    
    QGroupBox* consoleGroup = new QGroupBox("Console Log", this);
    QVBoxLayout* consoleLayout = new QVBoxLayout(consoleGroup);
    logOutput_ = new QTextEdit(this);
    logOutput_->setReadOnly(true);
    logOutput_->setMaximumHeight(150);
    logOutput_->setStyleSheet("QTextEdit { background-color: #2c3e50; color: #ecf0f1; font-family: monospace; }");
    consoleLayout->addWidget(logOutput_);
    consoleLayout->setContentsMargins(5, 5, 5, 5);
    
    centerLayout->addWidget(blockMapWidget_, 3);
    centerLayout->addWidget(consoleGroup, 1);
    
    // RIGHT PANEL: Performance Metrics + Recovery Operations
    QWidget* rightPanel = new QWidget(this);
    QVBoxLayout* rightLayout = new QVBoxLayout(rightPanel);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->setSpacing(5);
    
    performanceWidget_ = new PerformanceWidget(this);
    
    // Recovery Operations (moved from control panel)
    QGroupBox* recoveryOpsGroup = new QGroupBox("Recovery & Optimization", this);
    QVBoxLayout* recoveryLayout = new QVBoxLayout(recoveryOpsGroup);
    
    crashBtn_ = new QPushButton("Simulate Crash", this);
    crashBtn_->setStyleSheet("background-color: #e74c3c; color: white; font-weight: bold;");
    
    recoveryBtn_ = new QPushButton("Run Recovery", this);
    recoveryBtn_->setStyleSheet("background-color: #3498db; color: white; font-weight: bold;");
    
    defragBtn_ = new QPushButton("Run Defragmentation", this);
    defragBtn_->setStyleSheet("background-color: #2ecc71; color: white; font-weight: bold;");
    
    progressBar_ = new QProgressBar(this);
    progressBar_->setVisible(false);
    
    recoveryLayout->addWidget(crashBtn_);
    recoveryLayout->addWidget(recoveryBtn_);
    recoveryLayout->addWidget(defragBtn_);
    recoveryLayout->addWidget(progressBar_);
    recoveryLayout->addStretch();
    
    rightLayout->addWidget(performanceWidget_, 3);
    rightLayout->addWidget(recoveryOpsGroup, 1);
    rightPanel->setMinimumWidth(250);
    rightPanel->setMaximumWidth(400);
    
    // Add all panels to main layout
    mainLayout->addWidget(leftPanel, 1);      // Left: file browser + ops
    mainLayout->addWidget(centerWidget, 3);   // Center: block map + console
    mainLayout->addWidget(rightPanel, 1);     // Right: performance + recovery
    
    resize(1400, 900);
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
    
    // Connect file browser's fileDeleted signal to update widgets
    connect(fileBrowserWidget_, &FileBrowserWidget::fileDeleted, this, [this]() {
        updateAllWidgets();
        updateStatusBar();
    });
    
    // Connect control panel log messages to central console
    connect(controlPanel_, &ControlPanel::logMessage, this, [this](const QString& msg) {
        logOutput_->append(msg);
        logOutput_->ensureCursorVisible();
    });
    
    // Wire up inline file operation buttons
    
    // Delete button → trigger file browser delete
    connect(deleteFileBtn_, &QPushButton::clicked, fileBrowserWidget_, &FileBrowserWidget::triggerDelete);
    
    // Read button → display file contents in console with incremental progress
    connect(readFileBtn_, &QPushButton::clicked, this, [this]() {
        if (!fileSystem_ || !fileSystem_->isMounted()) {
            logOutput_->append("[ERROR] No disk mounted");
            return;
        }
        
        auto selectedFiles = fileBrowserWidget_->getSelectedFiles();
        if (selectedFiles.isEmpty()) {
            logOutput_->append("[INFO] Please select a file to read");
            return;
        }
        
        QString filename = selectedFiles.first();
        std::vector<uint8_t> data;
        
        // Get file size first to calculate blocks
        auto entries = fileSystem_->listDir("/");
        int fileSizeBytes = 0;
        uint32_t inodeNum = UINT32_MAX;
        
        for (const auto& entry : entries) {
            if (("/" + entry.getName()) == filename.toStdString()) {
                inodeNum = entry.inodeNumber;
                break;
            }
        }
        
        if (inodeNum == UINT32_MAX) {
            logOutput_->append(QString("[ERROR] File not found: %1").arg(filename));
            return;
        }
        
        // Read inode to get file size
        Inode inode;
        if (fileSystem_->getInodeManager()->readInode(inodeNum, inode)) {
            fileSizeBytes = inode.fileSize;
        }
        
        if (fileSizeBytes == 0) {
            logOutput_->append(QString("[INFO] File is empty: %1").arg(filename));
            writeProgressBar_->setValue(0);
            writeProgressBar_->setFormat("Ready");
            return;
        }
        
        int blocksToRead = (fileSizeBytes + 4095) / 4096;
        
        // Configure progress for reading
        writeProgressBar_->setMaximum(blocksToRead);
        writeProgressBar_->setValue(0);
        writeProgressBar_->setFormat(QString("Reading %1: %p%").arg(filename));
        
        // Simulate incremental read (1 block/sec)
        for (int i = 0; i < blocksToRead; ++i) {
            writeProgressBar_->setValue(i + 1);
            QApplication::processEvents();
            
            if (i < blocksToRead - 1) {
                QThread::msleep(1000);  // 1 second per block
            }
        }
        
        // Actual read
        if (fileSystem_->readFile(filename.toStdString(), data)) {
            logOutput_->append(QString("[SUCCESS] Read file: %1 (%2 bytes, %3 blocks)")
                             .arg(filename).arg(data.size()).arg(blocksToRead));
            // Show first 200 bytes as preview
            QString preview = QString::fromUtf8(reinterpret_cast<const char*>(data.data()), 
                                               std::min(200, (int)data.size()));
            logOutput_->append(QString("Preview: %1...").arg(preview));
        } else {
            logOutput_->append(QString("[ERROR] Failed to read file: %1").arg(filename));
        }
        
        // Reset progress
        writeProgressBar_->setValue(0);
        writeProgressBar_->setFormat("Ready");
    });
    
    // Create file button → incremental block-by-block write with random data
    connect(createFileBtn_, &QPushButton::clicked, this, [this]() {
        QString filename = filenameInput_->text().trimmed();
        if (filename.isEmpty()) {
            logOutput_->append("[ERROR] Please enter a filename");
            return;
        }
        
        if (!fileSystem_ || !fileSystem_->isMounted()) {
            logOutput_->append("[ERROR] No disk mounted");
            return;
        }
        
        // Get size from slider (slider value * 4 = KB)
        int sizeKB = fileSizeSlider_->value() * 4;
        int blocksNeeded = (sizeKB * 1024 + 4095) / 4096;
        
        // Create empty file first
        if (!fileSystem_->createFile(filename.toStdString())) {
            logOutput_->append(QString("[ERROR] Failed to create file: %1").arg(filename));
            return;
        }
        
        logOutput_->append(QString("[INFO] Writing %1 KB to %2 (%3 blocks)...")
                         .arg(sizeKB).arg(filename).arg(blocksNeeded));
        
        // Configure progress bar
        writeProgressBar_->setMaximum(blocksNeeded);
        writeProgressBar_->setValue(0);
        writeProgressBar_->setFormat(QString("Writing %1: %p%").arg(filename));
        
        // Disable buttons
        createFileBtn_->setEnabled(false);
        writeRandomBtn_->setEnabled(false);
        
        // Generate random alphanumeric data
        std::vector<uint8_t> allData(sizeKB * 1024);
        const char alphanumeric[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
        for (size_t i = 0; i < allData.size(); ++i) {
            allData[i] = alphanumeric[rand() % (sizeof(alphanumeric) - 1)];
        }
        
        auto startTime = std::chrono::high_resolution_clock::now();
        
        // Write data block by block (1 block = 4KB/sec)
        const int BLOCK_SIZE = 4096;
        
        for (int i = 0; i < blocksNeeded; ++i) {
            writeProgressBar_->setValue(i + 1);
            
            // Write incrementally
            int startByte = 0;
            int endByte = std::min(startByte + (i + 1) * BLOCK_SIZE, (int)allData.size());
            std::vector<uint8_t> incrementalData(allData.begin(), 
                                                 allData.begin() + endByte);
            
            fileSystem_->writeFile(filename.toStdString(), incrementalData);
            
            // Rebuild ownership and refresh
            fileSystem_->rebuildBlockOwnership();
            blockMapWidget_->refresh();
            fileBrowserWidget_->refresh();
            
            QApplication::processEvents();
            
            if (i < blocksNeeded - 1) {
                QThread::msleep(1000);  // 1 sec = 4KB/sec
            }
        }
        
        auto endTime = std::chrono::high_resolution_clock::now();
        double elapsedMs = std::chrono::duration<double, std::milli>(endTime - startTime).count();
        double speedKBps = (sizeKB / (elapsedMs / 1000.0));
        
        logOutput_->append(QString("[SUCCESS] Created %1 (%2 KB, %3 blocks) at %4 KB/s")
                         .arg(filename).arg(sizeKB).arg(blocksNeeded).arg(speedKBps, 0, 'f', 1));
        filenameInput_->clear();
        
        // Reset progress
        writeProgressBar_->setValue(0);
        writeProgressBar_->setFormat("Ready");
        
        // Re-enable buttons
        createFileBtn_->setEnabled(true);
        writeRandomBtn_->setEnabled(true);
        
        updateAllWidgets();
    });
    
    // Write random files button → create multiple 4KB files with incremental animation
    connect(writeRandomBtn_, &QPushButton::clicked, this, [this]() {
        if (!fileSystem_ || !fileSystem_->isMounted()) {
            logOutput_->append("[ERROR] No disk mounted");
            return;
        }
        
        int numFiles = numFilesCombo_->currentText().toInt();
        logOutput_->append(QString("[INFO] Creating %1 random files (4KB each)...").arg(numFiles));
        
        // Disable buttons
        writeRandomBtn_->setEnabled(false);
        createFileBtn_->setEnabled(false);
        
        std::vector<uint8_t> fileData(4096, 0xBB);  // 4KB file with pattern
        
        for (int i = 0; i < numFiles; ++i) {
            QString filename = QString("/random_%1.dat").arg(i + 1);
            
            // Create and write file
            if (fileSystem_->createFile(filename.toStdString()) &&
                fileSystem_->writeFile(filename.toStdString(), fileData)) {
                
                // Update ownership and display
                fileSystem_->rebuildBlockOwnership();
                blockMapWidget_->refresh();
                
                // Update progress
                writeProgressBar_->setMaximum(numFiles);
                writeProgressBar_->setValue(i + 1);
                writeProgressBar_->setFormat(QString("Creating files: %1/%2").arg(i + 1).arg(numFiles));
                
                QApplication::processEvents();
                QThread::msleep(100);  // Small delay for visibility
            }
        }
        
        logOutput_->append(QString("[SUCCESS] Created %1 files (4KB each)").arg(numFiles));
        
        // Reset progress bar
        writeProgressBar_->setValue(0);
        writeProgressBar_->setFormat("Ready");
        
        // Re-enable buttons
        writeRandomBtn_->setEnabled(true);
        createFileBtn_->setEnabled(true);
        
        updateAllWidgets();
    });
    
    // Recovery operation buttons
    connect(crashBtn_, &QPushButton::clicked, this, [this]() {
        controlPanel_->simulateCrash();
        updateAllWidgets();
    });
    
    connect(recoveryBtn_, &QPushButton::clicked, this, [this]() {
        controlPanel_->runRecovery();
        updateAllWidgets();
    });
    
    connect(defragBtn_, &QPushButton::clicked, this, [this]() {
        if (!fileSystem_ || !fileSystem_->isMounted()) {
            logOutput_->append("[ERROR] No disk mounted");
            return;
        }
        
        logOutput_->append("[INFO] Running defragmentation...");
        progressBar_->setVisible(true);
        progressBar_->setValue(0);
        progressBar_->setMaximum(100);
        
        // Run defragmentation
        bool cancelled = false;
        controlPanel_->runDefrag();
        
        // CRITICAL: Rebuild ownership map from new block locations
        fileSystem_->rebuildBlockOwnership();
        
        // Force complete refresh of ALL widgets
        blockMapWidget_->setFileSystem(fileSystem_.get());
        blockMapWidget_->refresh();
        
        fileBrowserWidget_->setFileSystem(fileSystem_.get());
        fileBrowserWidget_->refresh();
        
        performanceWidget_->setFileSystem(fileSystem_.get());
        performanceWidget_->updateMetrics();
        
        progressBar_->setValue(100);
        logOutput_->append("[SUCCESS] Defragmentation complete - check bitmap and file fragments");
        
        QThread::msleep(500);
        progressBar_->setVisible(false);
        
        updateAllWidgets();
    });
}

void MainWindow::onNewDisk() {
    std::cout << "onNewDisk() called" << std::endl;
    
    // Use Qt dialog instead of macOS native to avoidpermissions issues
    QString diskPath = QFileDialog::getSaveFileName(
        this, 
        "Create New Disk", 
        QDir::homePath() + "/disk.bin",
        "Disk Files (*.bin)",
        nullptr,
        QFileDialog::DontUseNativeDialog  // Avoid macOS permissions issues
    );
    
    std::cout << "Selected file: " << diskPath.toStdString() << std::endl;
    
    if (diskPath.isEmpty()) {
        std::cout << "No file selected, returning" << std::endl;
        return;
    }
    
    std::cout << "Creating file system..." << std::endl;
    
    // Create new file system using constructor
    fileSystem_ = std::make_unique<FileSystem>(diskPath.toStdString());
    
    if (!fileSystem_->createFileSystem()) {
        QMessageBox::critical(this, "Error", "Failed to create file system");
        fileSystem_.reset();
        return;
    }
    
    std::cout << "File system created successfully" << std::endl;
    
    // Initialize managers
    recoveryMgr_ = std::make_unique<RecoveryManager>(fileSystem_.get());
    defragMgr_ = std::make_unique<DefragManager>(fileSystem_.get());
    
    // Set file system for all widgets
    blockMapWidget_->setFileSystem(fileSystem_.get());
    performanceWidget_->setFileSystem(fileSystem_.get());
    fileBrowserWidget_->setFileSystem(fileSystem_.get());
    controlPanel_->setFileSystem(fileSystem_.get());
    controlPanel_->setRecoveryManager(recoveryMgr_.get());
    controlPanel_->setDefragManager(defragMgr_.get());
    
    // DON'T rebuild ownership on empty disk!
    // Refresh all widgets
    updateAllWidgets();
    updateStatusBar();
    
    logOutput_->append("[SUCCESS] Created new disk");
}

void MainWindow::onOpenDisk() {
    // Use Qt dialog instead of macOS native
    QString diskPath = QFileDialog::getOpenFileName(
        this, 
        "Mount Existing Disk",
        QDir::homePath(),
        "Disk Files (*.bin)",
        nullptr,
        QFileDialog::DontUseNativeDialog  // Avoid macOS permissions issues
    );
    
    if (diskPath.isEmpty()) return;
    
    // Mount existing disk using constructor
    fileSystem_ = std::make_unique<FileSystem>(diskPath.toStdString());
    if (!fileSystem_->mountFileSystem()) {
        QMessageBox::critical(this, "Error", "Failed to mount disk");
        fileSystem_.reset();
        return;
    }
    
    // Initialize managers
    recoveryMgr_ = std::make_unique<RecoveryManager>(fileSystem_.get());
    defragMgr_ = std::make_unique<DefragManager>(fileSystem_.get());
    
    // Set file system for all widgets
    blockMapWidget_->setFileSystem(fileSystem_.get());
    performanceWidget_->setFileSystem(fileSystem_.get());
    fileBrowserWidget_->setFileSystem(fileSystem_.get());
    controlPanel_->setFileSystem(fileSystem_.get());
    controlPanel_->setRecoveryManager(recoveryMgr_.get());
    controlPanel_->setDefragManager(defragMgr_.get());
    
    // DON'T rebuild ownership automatically - it will rebuild on demand when files are accessed
    // This prevents crashes from reading uninitialized blocks
    
    // Refresh all widgets
    updateAllWidgets();
    updateStatusBar();
    
    logOutput_->append("[SUCCESS] Mounted disk: " + diskPath);
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
