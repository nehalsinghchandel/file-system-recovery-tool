#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QSplitter>
#include <QStatusBar>
#include <QMenuBar>
#include <QLabel>
#include <QTextEdit>
#include <QGroupBox>
#include <QLineEdit>
#include <QComboBox>
#include <QPushButton>
#include <QProgressBar>
#include <QSlider>
#include <QFrame>
#include <QScrollArea>
#include <memory>
#include "FileSystem.h"
#include "RecoveryManager.h"
#include "DefragManager.h"

namespace FileSystemTool {

class BlockMapWidget;
class PerformanceWidget;
class ControlPanel;
class FileBrowserWidget;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    // Menu actions
    void onNewDisk();
    void onOpenDisk();
    void onCloseDisk();
    void onExit();
    void onAbout();
    
    // Status updates
    void updateStatusBar();
    void updateAllWidgets();

private:
    void setupUI();
    void setupMenuBar();
    void connectSignals();
    bool confirmDiskClose();
    
    // Core components
    std::unique_ptr<FileSystem> fileSystem_;
    std::unique_ptr<RecoveryManager> recoveryMgr_;
    std::unique_ptr<DefragManager> defragMgr_;
    
    // UI components
    BlockMapWidget* blockMapWidget_;
    PerformanceWidget* performanceWidget_;
    ControlPanel* controlPanel_;
    FileBrowserWidget* fileBrowserWidget_;
    QTextEdit* logOutput_;
    
    // Inline operation widgets
    QLineEdit* filenameInput_;
    QPushButton* createFileBtn_;
    QPushButton* readFileBtn_;
    QPushButton* deleteFileBtn_;
    QSlider* fileSizeSlider_;
    QLabel* fileSizeLabel_;
    QComboBox* numFilesCombo_;
    QPushButton* writeRandomBtn_;
    
    // Progress bar for slow operations
    QProgressBar* writeProgressBar_;
    
    // Recovery & Optimization buttons
    QPushButton* crashBtn_;
    QPushButton* recoveryBtn_;
    QPushButton* defragBtn_;
    QProgressBar* progressBar_;
    
    // Helper methods
    // Status
    QLabel* statusLabel_;
    QString currentDiskPath_;
};

} // namespace FileSystemTool

#endif // MAINWINDOW_H
