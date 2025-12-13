#ifndef PERFORMANCEWIDGET_H
#define PERFORMANCEWIDGET_H

#include <QWidget>
#include <QLabel>
#include <QDialog>
#include <QPushButton>
#include <QVBoxLayout>
#include <deque>
#include <vector>
#include <QtCharts/QChart>
#include <QtCharts/QChartView>
#include <QtCharts/QBarSet>
#include <QtCharts/QBarSeries>
#include <QtCharts/QStackedBarSeries>
#include <QtCharts/QLineSeries>
#include <QtCharts/QValueAxis>
#include <QtCharts/QBarCategoryAxis>

namespace FileSystemTool {

class FileSystem;
class DefragManager;

class PerformanceWidget : public QWidget {
    Q_OBJECT

public:
    explicit PerformanceWidget(QWidget *parent = nullptr);
    
    void setFileSystem(FileSystem* fs);
    void setDefragManager(DefragManager* defragMgr);
    
    void updateMetrics();
    void recordReadOperation(double latencyMs);
    void recordWriteOperation(double latencyMs);
    void updateFragmentationStats();
    void showDefragComparison();
    
    // Graph update methods
    struct FilePerformance {
        QString filename;
        double fragmentedTime;
        double defraggedTime;
    };
    void updatePerformanceChart(const std::vector<FilePerformance>& data);
    void updateHealthChart(int freeBlocks, int validBlocks, int orphanedBlocks);
    void updateChaosChart();
    void recordOperation();
    
    void reset();

private slots:
    void showPerformanceChartDialog();
    void showHealthChartDialog();
    void showChaosChartDialog();

private:
    void setupUI();
    void setupLatencyChart();
    void setupThroughputChart();
    void setupFragmentationDisplay();
    
    void setupPerformanceChart();
    void setupHealthChart();
    void setupChaosChart();
    
    FileSystem* fileSystem_;
    DefragManager* defragMgr_;
    
    // Charts
    QChartView* latencyChartView_;
    QChart* latencyChart_;
    QLineSeries* readLatencySeries_;
    QLineSeries* writeLatencySeries_;
    
    QChartView* throughputChartView_;
    QChart* throughputChart_;
    
    // Three new visualization charts
    QChartView* performanceChartView_;      // Graph 1: Grouped bar
    QChartView* healthChartView_;           // Graph 2: Stacked bar
    QChartView* chaosChartView_;            // Graph 3: Line graph
    
    QChart* performanceChart_;
    QChart* healthChart_;
    QChart* chaosChart_;
    
    QBarSeries* performanceSeries_;
    QStackedBarSeries* healthSeries_;
    QLineSeries* chaosLine_;
    
    // Labels
    QLabel* avgReadLabel_;
    QLabel* avgWriteLabel_;
    QLabel* totalOpsLabel_;
    QLabel* fragmentationLabel_;
    QLabel* latencyLabel_;
    QLabel* throughputLabel_;
    
    // Data storage
    std::vector<FilePerformance> perfData_;
    std::vector<double> fragmentationHistory_;
    int operationCount_;
    
    std::deque<double> readLatencies_;
    std::deque<double> writeLatencies_;
    std::deque<qint64> timestamps_;
    static constexpr int MAX_DATA_POINTS = 100;
};

} // namespace FileSystemTool

#endif // PERFORMANCEWIDGET_H
