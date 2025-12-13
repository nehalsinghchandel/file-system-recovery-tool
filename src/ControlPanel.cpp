#include "ControlPanel.h"
#include "FileSystem.h"
#include "RecoveryManager.h"
#include "DefragManager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QDateTime>
#include <QApplication>
#include <random>

namespace FileSystemTool {

ControlPanel::ControlPanel(QWidget *parent) 
    : QWidget(parent), fileSystem_(nullptr), recoveryMgr_(nullptr), 
      defragMgr_(nullptr), diskMounted_(false) {
    setupUI();
}

void ControlPanel::setFileSystem(FileSystem* fs) {
    fileSystem_ = fs;
    updateButtonStates();
}

void ControlPanel::setRecoveryManager(RecoveryManager* recoveryMgr) {
    recoveryMgr_ = recoveryMgr;
}

void ControlPanel::setDefragManager(DefragManager* defragMgr) {
    defragMgr_ = defragMgr;
}

void ControlPanel::setDiskMounted(bool mounted) {
    diskMounted_ = mounted;
    updateButtonStates();
}

void ControlPanel::appendLog(const QString& message) {
    QString timestamp = QTime::currentTime().toString("hh:mm:ss");
    emit logMessage(QString("[%1] %2").arg(timestamp).arg(message));
}

void ControlPanel::clearLog() {
    // Signal could be added if needed for clearing log
}

// Public methods for triggering operations from MainWindow
void ControlPanel::createFile(const QString& filename) {
    if (!fileSystem_) {
        appendLog("Error: No file system");
        return;
    }
    
    if (fileSystem_->createFile(filename.toStdString())) {
        appendLog("Created file: " + filename);
        emit operationCompleted();
    } else {
        appendLog("Error: Failed to create file: " + filename);
    }
}

void ControlPanel::writeRandomFiles(int numFiles) {
    filenameInput_->setText("");  // Clear input
    numFilesCombo_->setCurrentText(QString::number(numFiles));
    onWriteRandomDataClicked();
}

void ControlPanel::simulateCrash() {
    onSimulateCrashClicked();
}

void ControlPanel::runRecovery() {
    onRunRecoveryClicked();
}

void ControlPanel::runDefrag() {
    onRunDefragClicked();
}

void ControlPanel::setupUI() {
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    
    // File operations group
    fileOpsGroup_ = new QGroupBox("File Operations", this);
    QVBoxLayout* fileLayout = new QVBoxLayout(fileOpsGroup_);
    
    QHBoxLayout* fileInputLayout = new QHBoxLayout();
    filenameInput_ = new QLineEdit(this);
    filenameInput_->setPlaceholderText("Enter filename (e.g., /test.txt)");
    fileInputLayout->addWidget(new QLabel("Filename:", this));
    fileInputLayout->addWidget(filenameInput_);
    
    QHBoxLayout* fileButtonsLayout = new QHBoxLayout();
    createFileBtn_ = new QPushButton("Create File", this);
    deleteFileBtn_ = new QPushButton("Delete File", this);
    fileButtonsLayout->addWidget(createFileBtn_);
    fileButtonsLayout->addWidget(deleteFileBtn_);
    
    QHBoxLayout* randomDataLayout = new QHBoxLayout();
    numFilesCombo_ = new QComboBox(this);
    numFilesCombo_->addItems({"10", "25", "50", "100"});
    numFilesCombo_->setCurrentIndex(1);
    writeRandomBtn_ = new QPushButton("Write Random Files", this);
    randomDataLayout->addWidget(new QLabel("Count:", this));
    randomDataLayout->addWidget(numFilesCombo_);
    randomDataLayout->addWidget(writeRandomBtn_);
    
    fileLayout->addLayout(fileInputLayout);
    fileLayout->addLayout(fileButtonsLayout);
    fileLayout->addLayout(randomDataLayout);
    
    // Recovery operations group
    recoveryOpsGroup_ = new QGroupBox("Recovery & Optimization", this);
    QVBoxLayout* recoveryLayout = new QVBoxLayout(recoveryOpsGroup_);
    
    QHBoxLayout* recoveryButtonsLayout = new QHBoxLayout();
    crashBtn_ = new QPushButton("Simulate Crash", this);
    crashBtn_->setStyleSheet("background-color: #e74c3c; color: white; font-weight: bold;");
    recoveryBtn_ = new QPushButton("Run Recovery", this);
    recoveryBtn_->setStyleSheet("background-color: #3498db; color: white; font-weight: bold;");
    defragBtn_ = new QPushButton("Run Defragmentation", this);
    defragBtn_->setStyleSheet("background-color: #2ecc71; color: white; font-weight: bold;");
    
    recoveryButtonsLayout->addWidget(crashBtn_);
    recoveryButtonsLayout->addWidget(recoveryBtn_);
    recoveryButtonsLayout->addWidget(defragBtn_);
    
    recoveryLayout->addLayout(recoveryButtonsLayout);
    
    // Progress bar
    progressBar_ = new QProgressBar(this);
    progressBar_->setVisible(false);
    
    // Log output (This section is now commented out as logging is handled via signals)
    // QGroupBox* logGroup = new QGroupBox("Log Output", this);
    // QVBoxLayout* logLayout = new QVBoxLayout(logGroup);
    
    // logOutput_ = new QTextEdit(this);
    // logOutput_->setReadOnly(true);
    // logOutput_->setMaximumHeight(150);
    
    // logLayout->addWidget(logOutput_);
    
    // Add all to main layout
    mainLayout->addWidget(fileOpsGroup_);
    mainLayout->addWidget(recoveryOpsGroup_);
    mainLayout->addWidget(progressBar_);
    // mainLayout->addWidget(logGroup); // Removed as log output is now external
    mainLayout->addStretch(); // Added to push content upwards
    
    // Connect signals
    connect(createFileBtn_, &QPushButton::clicked, this, &ControlPanel::onCreateFileClicked);
    connect(deleteFileBtn_, &QPushButton::clicked, this, &ControlPanel::onDeleteFileClicked);
    connect(writeRandomBtn_, &QPushButton::clicked, this, &ControlPanel::onWriteRandomDataClicked);
    connect(crashBtn_, &QPushButton::clicked, this, &ControlPanel::onSimulateCrashClicked);
    connect(recoveryBtn_, &QPushButton::clicked, this, &ControlPanel::onRunRecoveryClicked);
    connect(defragBtn_, &QPushButton::clicked, this, &ControlPanel::onRunDefragClicked);
    
    updateButtonStates();
}

void ControlPanel::updateButtonStates() {
    bool mounted = diskMounted_ && fileSystem_ && fileSystem_->isMounted();
    
    createFileBtn_->setEnabled(mounted);
    deleteFileBtn_->setEnabled(mounted);
    writeRandomBtn_->setEnabled(mounted);
    crashBtn_->setEnabled(mounted);
    recoveryBtn_->setEnabled(mounted);
    defragBtn_->setEnabled(mounted);
}

void ControlPanel::onCreateFileClicked() {
    QString filename = filenameInput_->text();
    if (filename.isEmpty()) {
        appendLog("Error: Please enter a filename");
        return;
    }
    
    if (fileSystem_->createFile(filename.toStdString())) {
        appendLog("Created file: " + filename);
        emit operationCompleted();
    } else {
        appendLog("Error: Failed to create file: " + filename);
    }
}

void ControlPanel::onDeleteFileClicked() {
    QString filename = filenameInput_->text();
    if (filename.isEmpty()) {
        appendLog("Error: Please enter a filename");
        return;
    }
    
    if (fileSystem_->deleteFile(filename.toStdString())) {
        appendLog("Deleted file: " + filename);
        emit operationCompleted();
    } else {
        appendLog("Error: Failed to delete file: " + filename);
    }
}

void ControlPanel::onWriteRandomDataClicked() {
    int numFiles = numFilesCombo_->currentText().toInt();
    
    // Check available resources before starting
    if (!fileSystem_ || !fileSystem_->isMounted()) {
        appendLog("Error: File system not mounted");
        return;
    }
    
    uint32_t freeBlocks = fileSystem_->getFreeBlocks();
    uint32_t estimatedBlocksNeeded = numFiles * 3; // Rough estimate: 2-3 blocks per file
    
    if (freeBlocks < estimatedBlocksNeeded) {
        auto reply = QMessageBox::warning(this, "Low Disk Space",
            QString("Not enough free blocks. Available: %1, Estimated needed: %2\n"
                    "Continue anyway?")
                .arg(freeBlocks).arg(estimatedBlocksNeeded),
            QMessageBox::Yes | QMessageBox::No);
        
        if (reply != QMessageBox::Yes) {
            return;
        }
    }
    
    appendLog(QString("Writing %1 random files...").arg(numFiles));
    progressBar_->setVisible(true);
    progressBar_->setRange(0, numFiles);
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> sizeDist(512, 8192);
    
    // Use timestamp to ensure unique filenames across multiple clicks
    qint64 timestamp = QDateTime::currentMSecsSinceEpoch();
    
    int successCount = 0;
    int failCount = 0;
    
    for (int i = 0; i < numFiles; ++i) {
        // Check if we still have space (check every 10 files)
        if (i % 10 == 0 && fileSystem_->getFreeBlocks() < 10) {
            appendLog(QString("Disk full after %1 files. Stopping.").arg(i));
            break;
        }
        
        // Create unique filename with timestamp
        QString filename = QString("/random_%1_%2.dat").arg(timestamp).arg(i);
        
        if (!fileSystem_->createFile(filename.toStdString())) {
            appendLog(QString("Failed to create: %1").arg(filename));
            failCount++;
            // Stop if we get too many consecutive failures
            if (failCount > 5) {
                appendLog("Too many failures. Stopping.");
                break;
            }
            continue;
        }
        
        // Generate random data
        size_t size = sizeDist(gen);
        std::vector<uint8_t> data(size);
        for (auto& byte : data) {
            byte = static_cast<uint8_t>(gen());
        }
        
        // Write data with error checking
        if (!fileSystem_->writeFile(filename.toStdString(), data)) {
            appendLog(QString("Failed to write: %1").arg(filename));
            failCount++;
            // Clean up the empty file
            fileSystem_->deleteFile(filename.toStdString());
            
            if (failCount > 5) {
                appendLog("Too many write failures. Stopping.");
                break;
            }
            continue;
        }
        
        successCount++;
        failCount = 0; // Reset consecutive fail counter
        progressBar_->setValue(i + 1);
        
        // Process events periodically to keep UI responsive
        if (i % 10 == 0) {
            QApplication::processEvents();
        }
    }
    
    progressBar_->setVisible(false);
    appendLog(QString("Completed: %1 files written, %2 failed")
        .arg(successCount).arg(failCount));
    emit operationCompleted();
}

void ControlPanel::onSimulateCrashClicked() {
    auto reply = QMessageBox::warning(this, "Simulate Crash",
        "This will simulate a system crash by:\n\n"
        "• Creating a partially written file with orphan blocks\n"
        "• Marking disk as 'not cleanly unmounted'\n"
        "• Leaving inconsistent metadata\n\n"
        "The file system will remain usable, but:\n"
        "• Recovery will be needed to clean up orphan blocks\n"
        "• You'll see a warning when remounting the disk\n\n"
        "Continue?",
        QMessageBox::Yes | QMessageBox::No);
    
    if (reply != QMessageBox::Yes) {
        return;
    }
    
    // Create a test file and crash during write
    std::string filename = "/crash_test_file.dat";
    fileSystem_->createFile(filename);
    
    std::vector<uint8_t> data(16384, 0xAA);  // 16KB of data
    
    if (recoveryMgr_) {
        recoveryMgr_->simulateCrashDuringWrite(filename, data, 0.5);
        appendLog("⚠️ CRASH SIMULATED!");
        appendLog("Disk marked as 'not cleanly unmounted'");
        appendLog("Partial file created with orphan blocks");
        appendLog("Close and reopen disk, then run recovery to fix");
        fileSystem_->getDisk()->markDirty();
    }
    
    emit operationCompleted();
}

void ControlPanel::onRunRecoveryClicked() {
    appendLog("Running recovery checks...");
    
    if (!recoveryMgr_) {
        appendLog("Error: Recovery manager not available");
        return;
    }
    
    if (recoveryMgr_->performRecovery()) {
        appendLog("Recovery completed successfully");
        
        const auto& report = recoveryMgr_->getLastReport();
        appendLog(QString("Orphan blocks found: %1").arg(report.orphanBlocks));
        appendLog(QString("Invalid inodes found: %1").arg(report.invalidInodes));
        
        for (const auto& fix : report.fixes) {
            appendLog("Fix: " + QString::fromStdString(fix));
        }
    } else {
        appendLog("Error: Recovery failed");
    }
    
    emit operationCompleted();
}

void ControlPanel::onRunDefragClicked() {
    appendLog("Starting defragmentation...");
    
    if (!defragMgr_) {
        appendLog("Error: Defragmentation manager not available");
        return;
    }
    
    progressBar_->setVisible(true);
    progressBar_->setRange(0, 100);
    
    // Set progress callback
    defragMgr_->setProgressCallback([this](int progress, const std::string& message) {
        progressBar_->setValue(progress);
        if (!message.empty()) {
            appendLog(QString::fromStdString(message));
        }
    });
    
    bool cancelled = false;
    if (defragMgr_->defragmentFileSystem(cancelled)) {
        appendLog("Defragmentation completed");
        
        const auto& before = defragMgr_->getBeforeDefragBenchmark();
        const auto& after = defragMgr_->getAfterDefragBenchmark();
        
        double improvement = ((before.avgReadTimeMs - after.avgReadTimeMs) / 
                             before.avgReadTimeMs) * 100;
        
        appendLog(QString("Performance improvement: %1%").arg(improvement, 0, 'f', 1));
    } else {
        appendLog("Defragmentation cancelled or failed");
    }
    
    progressBar_->setVisible(false);
    emit operationCompleted();
}

void ControlPanel::onMountClicked() {
    emit mountRequested();
}

void ControlPanel::onUnmountClicked() {
    emit unmountRequested();
}

void ControlPanel::onFormatClicked() {
    emit formatRequested();
}

} // namespace FileSystemTool
