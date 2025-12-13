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
    : QWidget(parent), fileSystem_(nullptr), defragMgr_(nullptr), operationCount_(0) {
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
    
    // Three visualization charts (setup but don't display inline)
    setupPerformanceChart();
    setupHealthChart();
    setupChaosChart();
    
    // Buttons to open charts in modals
    QGroupBox* chartsGroup = new QGroupBox("Visualization Graphs", this);
    QVBoxLayout* chartsLayout = new QVBoxLayout(chartsGroup);
    
    QPushButton* performanceBtn = new QPushButton("📊 Performance Comparison", this);
    performanceBtn->setToolTip("Show before/after defragmentation performance chart");
    performanceBtn->setMinimumHeight(35);
    connect(performanceBtn, &QPushButton::clicked, this, &PerformanceWidget::showPerformanceChartDialog);
    
    QPushButton* healthBtn = new QPushButton("🏥 System Health", this);
    healthBtn->setToolTip("Show disk health across crash/recovery states");
    healthBtn->setMinimumHeight(35);
    connect(healthBtn, &QPushButton::clicked, this, &PerformanceWidget::showHealthChartDialog);
    
    QPushButton* chaosBtn = new QPushButton("📈 Fragmentation Lifecycle", this);
    chaosBtn->setToolTip("Show fragmentation over time");
    chaosBtn->setMinimumHeight(35);
    connect(chaosBtn, &QPushButton::clicked, this, &PerformanceWidget::showChaosChartDialog);
    
    chartsLayout->addWidget(performanceBtn);
    chartsLayout->addWidget(healthBtn);
    chartsLayout->addWidget(chaosBtn);
    
    mainLayout->addWidget(metricsGroup);
    mainLayout->addWidget(latencyChartView_);
    mainLayout->addWidget(chartsGroup);
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

void PerformanceWidget::setupPerformanceChart() {
    // Graph 1: Performance Comparison (Grouped Bar Chart)
    performanceChart_ = new QChart();
    performanceChart_->setTitle("Performance: Before vs After Defragmentation");
    performanceChart_->setAnimationOptions(QChart::SeriesAnimations);
    
    performanceSeries_ = new QBarSeries();
    
    QBarSet* fragmentedSet = new QBarSet("Fragmented");
    fragmentedSet->setColor(QColor(231, 76, 60));  // Red
    
    QBarSet* defraggedSet = new QBarSet("Defragmented");
    defraggedSet->setColor(QColor(46, 204, 113));  // Green
    
    performanceSeries_->append(fragmentedSet);
    performanceSeries_->append(defraggedSet);
    
    performanceChart_->addSeries(performanceSeries_);
    performanceChart_->createDefaultAxes();
    performanceChart_->legend()->setVisible(true);
    performanceChart_->legend()->setAlignment(Qt::AlignBottom);
    
    // Create chart view but DON'T add to layout (only for dialog)
    performanceChartView_ = new QChartView(performanceChart_);
    performanceChartView_->setRenderHint(QPainter::Antialiasing);
    performanceChartView_->hide();  // Hidden until shown in dialog
}

void PerformanceWidget::setupHealthChart() {
    // Graph 2: Structure Health (Stacked Bar Chart)
    healthChart_ = new QChart();
    healthChart_->setTitle("System Health: Normal → Crash → Recovery");
    healthChart_->setAnimationOptions(QChart::SeriesAnimations);
    
    healthSeries_ = new QStackedBarSeries();
    
    QBarSet* freeSet = new QBarSet("Free Space");
    freeSet->setColor(QColor(46, 204, 113));  // Green
    
    QBarSet* validSet = new QBarSet("Valid Data");
    validSet->setColor(QColor(52, 152, 219));  // Blue
    
    QBarSet* orphanedSet = new QBarSet("Orphaned Blocks");
    orphanedSet->setColor(QColor(231, 76, 60));  // Red
    
    healthSeries_->append(freeSet);
    healthSeries_->append(validSet);
    healthSeries_->append(orphanedSet);
    
    healthChart_->addSeries(healthSeries_);
    
    QBarCategoryAxis* axisX = new QBarCategoryAxis();
    axisX->append({"Normal", "After Crash", "After Recovery"});
    healthChart_->addAxis(axisX, Qt::AlignBottom);
    healthSeries_->attachAxis(axisX);
    
    QValueAxis* axisY = new QValueAxis();
    axisY->setTitleText("Blocks");
    healthChart_->addAxis(axisY, Qt::AlignLeft);
    healthSeries_->attachAxis(axisY);
    
    healthChart_->legend()->setVisible(true);
    healthChart_->legend()->setAlignment(Qt::AlignBottom);
    
    // Create chart view but DON'T add to layout (only for dialog)
    healthChartView_ = new QChartView(healthChart_);
    healthChartView_->setRenderHint(QPainter::Antialiasing);
    healthChartView_->hide();  // Hidden until shown in dialog
}

void PerformanceWidget::setupChaosChart() {
    // Graph 3: Fragmentation Over Time (Line Graph)
    chaosChart_ = new QChart();
    chaosChart_->setTitle("Fragmentation Lifecycle");
    chaosChart_->setAnimationOptions(QChart::SeriesAnimations);
    
    chaosLine_ = new QLineSeries();
    chaosLine_->setName("Fragmentation %");
    chaosLine_->setColor(QColor(231, 76, 60));  // Red
    
    // Start at 0%
    chaosLine_->append(0, 0);
    
    chaosChart_->addSeries(chaosLine_);
    
    QValueAxis* axisX = new QValueAxis();
    axisX->setTitleText("Operations");
    axisX->setLabelFormat("%d");
    axisX->setRange(0, 100);
    
    QValueAxis* axisY = new QValueAxis();
    axisY->setTitleText("Fragmentation (%)");
    axisY->setRange(0, 100);
    
    chaosChart_->addAxis(axisX, Qt::AlignBottom);
    chaosChart_->addAxis(axisY, Qt::AlignLeft);
    chaosLine_->attachAxis(axisX);
    chaosLine_->attachAxis(axisY);
    
    chaosChart_->legend()->setVisible(true);
    chaosChart_->legend()->setAlignment(Qt::AlignBottom);
    
    // Create chart view but DON'T add to layout (only for dialog)
    chaosChartView_ = new QChartView(chaosChart_);
    chaosChartView_->setRenderHint(QPainter::Antialiasing);
    chaosChartView_->hide();  // Hidden until shown in dialog
}

// Graph update methods
void PerformanceWidget::updatePerformanceChart(const std::vector<FilePerformance>& data) {
    // Clear existing data
    while (performanceSeries_->count() > 0) {
        performanceSeries_->remove(performanceSeries_->barSets().at(0));
    }
    
    QBarSet* fragmentedSet = new QBarSet("Fragmented");
    fragmentedSet->setColor(QColor(231, 76, 60));
    
    QBarSet* defraggedSet = new QBarSet("Defragmented");
    defraggedSet->setColor(QColor(46, 204, 113));
    
    for (const auto& perf : data) {
        fragmentedSet->append(perf.fragmentedTime);
        defraggedSet->append(perf.defraggedTime);
    }
    
    performanceSeries_->append(fragmentedSet);
    performanceSeries_->append(defraggedSet);
}

void PerformanceWidget::updateHealthChart(int freeBlocks, int validBlocks, int orphanedBlocks) {
    // Update stacked bar series with new data
    if (healthSeries_->count() >= 3) {
        healthSeries_->barSets().at(0)->append(freeBlocks);     // Free
        healthSeries_->barSets().at(1)->append(validBlocks);    // Valid
        healthSeries_->barSets().at(2)->append(orphanedBlocks); // Orphaned
    }
}

void PerformanceWidget::updateChaosChart() {
    if (!fileSystem_) return;
    
    double fragmentation = fileSystem_->getFragmentationScore();
    chaosLine_->append(operationCount_, fragmentation);
    
    // Update X axis range if needed
    if (operationCount_ > 100) {
        auto* axisX = qobject_cast<QValueAxis*>(chaosChart_->axes(Qt::Horizontal).at(0));
        if (axisX) {
            axisX->setRange(0, operationCount_ + 10);
        }
    }
}

void PerformanceWidget::recordOperation() {
    operationCount_++;
    updateChaosChart();
}

// Dialog methods to show charts in modal windows
void PerformanceWidget::showPerformanceChartDialog() {
    QDialog* dialog = new QDialog(this);
    dialog->setWindowTitle("Performance: Before vs After Defragmentation");
    dialog->resize(800, 600);
    
    QVBoxLayout* layout = new QVBoxLayout(dialog);
    QChartView* chartView = new QChartView(performanceChart_, dialog);
    chartView->setRenderHint(QPainter::Antialiasing);
    
    layout->addWidget(chartView);
    
    QPushButton* closeBtn = new QPushButton("Close", dialog);
    connect(closeBtn, &QPushButton::clicked, dialog, &QDialog::accept);
    layout->addWidget(closeBtn);
    
    dialog->exec();
}

void PerformanceWidget::showHealthChartDialog() {
    QDialog* dialog = new QDialog(this);
    dialog->setWindowTitle("System Health: Normal → Crash → Recovery");
    dialog->resize(800, 600);
    
    QVBoxLayout* layout = new QVBoxLayout(dialog);
    QChartView* chartView = new QChartView(healthChart_, dialog);
    chartView->setRenderHint(QPainter::Antialiasing);
    
    layout->addWidget(chartView);
    
    QPushButton* closeBtn = new QPushButton("Close", dialog);
    connect(closeBtn, &QPushButton::clicked, dialog, &QDialog::accept);
    layout->addWidget(closeBtn);
    
    dialog->exec();
}

void PerformanceWidget::showChaosChartDialog() {
    QDialog* dialog = new QDialog(this);
    dialog->setWindowTitle("Fragmentation Lifecycle");
    dialog->resize(800, 600);
    
    QVBoxLayout* layout = new QVBoxLayout(dialog);
    QChartView* chartView = new QChartView(chaosChart_, dialog);
    chartView->setRenderHint(QPainter::Antialiasing);
    
    layout->addWidget(chartView);
    
    QPushButton* closeBtn = new QPushButton("Close", dialog);
    connect(closeBtn, &QPushButton::clicked, dialog, &QDialog::accept);
    layout->addWidget(closeBtn);
    
    dialog->exec();
}

} // namespace FileSystemTool
