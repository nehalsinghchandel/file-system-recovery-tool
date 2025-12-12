#ifndef CONTROLPANEL_H
#define CONTROLPANEL_H

#include <QWidget>
#include <QPushButton>
#include <QTextEdit>
#include <QProgressBar>
#include <QGroupBox>
#include <QLineEdit>
#include <QComboBox>

namespace FileSystemTool {

class FileSystem;
class RecoveryManager;
class DefragManager;

class ControlPanel : public QWidget {
    Q_OBJECT

public:
    explicit ControlPanel(QWidget *parent = nullptr);
    
    void setFileSystem(FileSystem* fs);
    void setRecoveryManager(RecoveryManager* recoveryMgr);
    void setDefragManager(DefragManager* defragMgr);
    
    void setDiskMounted(bool mounted);
    void appendLog(const QString& message);
    void clearLog();

signals:
    void mountRequested();
    void unmountRequested();
    void formatRequested();
    void createFileRequested(const QString& filename);
    void deleteFileRequested(const QString& filename);
    void writeRandomDataRequested(int numFiles);
    void simulateCrashRequested();
    void runRecoveryRequested();
    void runDefragRequested();
    void operationCompleted();

private slots:
    void onMountClicked();
    void onUnmountClicked();
    void onFormatClicked();
    void onCreateFileClicked();
    void onDeleteFileClicked();
    void onWriteRandomDataClicked();
    void onSimulateCrashClicked();
    void onRunRecoveryClicked();
    void onRunDefragClicked();

private:
    void setupUI();
    void updateButtonStates();
    
    FileSystem* fileSystem_;
    RecoveryManager* recoveryMgr_;
    DefragManager* defragMgr_;
    bool diskMounted_;
    
    // Disk operations
    QGroupBox* diskOpsGroup_;
    QPushButton* mountBtn_;
    QPushButton* unmountBtn_;
    QPushButton* formatBtn_;
    
    // File operations
    QGroupBox* fileOpsGroup_;
    QLineEdit* filenameInput_;
    QPushButton* createFileBtn_;
    QPushButton* deleteFileBtn_;
    QComboBox* numFilesCombo_;
    QPushButton* writeRandomBtn_;
    
    // Recovery operations
    QGroupBox* recoveryOpsGroup_;
    QPushButton* crashBtn_;
    QPushButton* recoveryBtn_;
    QPushButton* defragBtn_;
    
    // Status
    QProgressBar* progressBar_;
    QTextEdit* logOutput_;
};

} // namespace FileSystemTool

#endif // CONTROLPANEL_H
