#include "PerformanceWidget.h"
#include "FileSystem.h"
#include "DefragManager.h"
#include <QVBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QDateTime>
#include <cstring>

namespace FileSystemTool {

PerformanceWidget::PerformanceWidget(QWidget *parent) 
    : QWidget(parent), fileSystem_(nullptr), defragMgr_(nullptr) {
    setupUI();
}

void PerformanceWidget::setFileSystem(FileSystem* fs) {
    fileSystem_ = fs;
    updateMetrics();
}

void PerformanceWidget::setDefragManager(DefragManager* defragMgr) {
    defragMgr_ = defragMgr;
}

void PerformanceWidget::setupUI() {
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    
    // Metrics group
    QGroupBox* metricsGroup = new QGroupBox("Performance Metrics", this);
    QVBoxLayout* metricsLayout = new QVBoxLayout(metricsGroup);
    
    avgReadLabel_ = new QLabel("Avg Read: 0 ms", this);
    avgWriteLabel_ = new QLabel("Avg Write: 0 ms", this);
    totalOpsLabel_ = new QLabel("Total Ops: 0", this);
    fragmentationLabel_ = new QLabel("Fragmentation: 0%", this);
    
    metricsLayout->addWidget(avgReadLabel_);
    metricsLayout->addWidget(avgWriteLabel_);
    metricsLayout->addWidget(totalOpsLabel_);
    metricsLayout->addWidget(fragmentationLabel_);
    
    // Latency chart
    setupLatencyChart();
    
    // Defragmentation Results Section
    QGroupBox* defragGroup = new QGroupBox("Defragmentation Results", this);
    QFormLayout* defragLayout = new QFormLayout(defragGroup);
    
    defragResultsLabel_ = new QLabel("No defrag run yet", this);
    defragResultsLabel_->setStyleSheet("font-weight: bold; color: #95a5a6;");
    
    beforeLatencyLabel_ = new QLabel("N/A", this);
    afterLatencyLabel_ = new QLabel("N/A", this);
    improvementLabel_ = new QLabel("N/A", this);
    improvementLabel_->setStyleSheet("font-weight: bold;");
    
    defragLayout->addRow("Status:", defragResultsLabel_);
    defragLayout->addRow("Before (avg ms):", beforeLatencyLabel_);
    defragLayout->addRow("After (avg ms):", afterLatencyLabel_);
    defragLayout->addRow("Improvement:", improvementLabel_);
    
    mainLayout->addWidget(metricsGroup);
    mainLayout->addWidget(latencyChartView_);
    mainLayout->addWidget(defragGroup);
}

void PerformanceWidget::setupLatencyChart() {
    latencyChart_ = new QChart();
    latencyChart_->setTitle("Read/Write Latency");
    latencyChart_->setAnimationOptions(QChart::SeriesAnimations);
    
    readLatencySeries_ = new QLineSeries();
    readLatencySeries_->setName("Read Latency");
    
    writeLatencySeries_ = new QLineSeries();
    writeLatencySeries_->setName("Write Latency");
    
    latencyChart_->addSeries(readLatencySeries_);
    latencyChart_->addSeries(writeLatencySeries_);
    latencyChart_->createDefaultAxes();
    
    latencyChartView_ = new QChartView(latencyChart_, this);
    latencyChartView_->setRenderHint(QPainter::Antialiasing);
    latencyChartView_->setMinimumHeight(200);
}

void PerformanceWidget::updateMetrics() {
    if (!fileSystem_ || !fileSystem_->isMounted()) {
        return;
    }
    
    const auto& stats = fileSystem_->getStats();
    
    double avgRead = stats.totalReads > 0 ? 
                     stats.lastReadTimeMs : 0;
    double avgWrite = stats.totalWrites > 0 ? 
                      stats.lastWriteTimeMs : 0;
    
    avgReadLabel_->setText(QString("Avg Read: %1 ms").arg(avgRead, 0, 'f', 2));
    avgWriteLabel_->setText(QString("Avg Write: %1 ms").arg(avgWrite, 0, 'f', 2));
    totalOpsLabel_->setText(QString("Total Ops: %1 reads, %2 writes")
                           .arg(stats.totalReads)
                           .arg(stats.totalWrites));
    
    // Get REAL fragmentation score from FileSystem
    double fragScore = fileSystem_->getFragmentationScore();
    fragmentationLabel_->setText(QString("Fragmentation: %1%").arg(fragScore, 0, 'f', 1));
    
    // Record chart data if we have new operations
    if (avgRead > 0 && (readLatencies_.empty() || readLatencies_.back() != avgRead)) {
        readLatencies_.push_back(avgRead);
        timestamps_.push_back(QDateTime::currentMSecsSinceEpoch());
        
        if (readLatencies_.size() > MAX_DATA_POINTS) {
            readLatencies_.erase(readLatencies_.begin());
            timestamps_.erase(timestamps_.begin());
        }
    }
    
    if (avgWrite > 0 && (writeLatencies_.empty() || writeLatencies_.back() != avgWrite)) {
        writeLatencies_.push_back(avgWrite);
        
        if (writeLatencies_.size() > MAX_DATA_POINTS) {
            writeLatencies_.erase(writeLatencies_.begin());
        }
    }
}

void PerformanceWidget::recordReadOperation(double latencyMs) {
    readLatencies_.push_back(latencyMs);
    timestamps_.push_back(QDateTime::currentMSecsSinceEpoch());
    
    if (readLatencies_.size() > MAX_DATA_POINTS) {
        readLatencies_.pop_front();
        timestamps_.pop_front();
    }
    
    // Update chart
    readLatencySeries_->clear();
    for (size_t i = 0; i < readLatencies_.size(); ++i) {
        readLatencySeries_->append(i, readLatencies_[i]);
    }
    
    updateMetrics();
}

void PerformanceWidget::recordWriteOperation(double latencyMs) {
    writeLatencies_.push_back(latencyMs);
    
    if (writeLatencies_.size() > MAX_DATA_POINTS) {
        writeLatencies_.pop_front();
    }
    
    // Update chart
    writeLatencySeries_->clear();
    for (size_t i = 0; i < writeLatencies_.size(); ++i) {
        writeLatencySeries_->append(i, writeLatencies_[i]);
    }
    
    updateMetrics();
}

void PerformanceWidget::updateFragmentationStats() {
    if (!defragMgr_) return;
    
    auto stats = defragMgr_->analyzeFragmentation();
    
    fragmentationLabel_->setText(QString("Fragmentation: %1% (%2/%3 files fragmented)")
                                .arg(stats.fragmentationScore * 100, 0, 'f', 1)
                                .arg(stats.fragmentedFiles)
                                .arg(stats.totalFiles));
}

void PerformanceWidget::reset() {
    readLatencies_.clear();
    writeLatencies_.clear();
    timestamps_.clear();
    readLatencySeries_->clear();
    throughputChart_->update();
    latencyChartView_->update();
}

void PerformanceWidget::setDefragResults(double beforeMs, double afterMs, uint32_t filesDefragged) {
    defragResultsLabel_->setText("Last run: Complete");
    defragResultsLabel_->setStyleSheet("font-weight: bold; color: #27ae60;");
    
    beforeLatencyLabel_->setText(QString("%1 ms").arg(beforeMs, 0, 'f', 4));
    afterLatencyLabel_->setText(QString("%1 ms").arg(afterMs, 0, 'f', 4));
    
    double improvement = beforeMs - afterMs;
    QString improvementText = QString("%1 ms (%2 files)")
                             .arg(improvement, 0, 'f', 4)
                             .arg(filesDefragged);
    
    if (improvement > 0) {
        improvementLabel_->setText("✓ " + improvementText);
        improvementLabel_->setStyleSheet("font-weight: bold; color: #27ae60;");
    } else {
        improvementLabel_->setText(improvementText);
        improvementLabel_->setStyleSheet("font-weight: bold; color: #95a5a6;");
    }
}

} // namespace FileSystemTool
