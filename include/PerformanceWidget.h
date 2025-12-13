#ifndef PERFORMANCEWIDGET_H
#define PERFORMANCEWIDGET_H

#include <QWidget>
#include <QChart>
#include <QChartView>
#include <QLineSeries>
#include <QBarSeries>
#include <QValueAxis>
#include <QVBoxLayout>
#include <QLabel>
#include <deque>

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
    void setDefragResults(double beforeMs, double afterMs, uint32_t filesDefragged);
    void reset();

private:
    void setupUI();
    void setupLatencyChart();
    void setupThroughputChart();
    void setupFragmentationDisplay();
    
    FileSystem* fileSystem_;
    DefragManager* defragMgr_;
    
    // Charts
    QChartView* latencyChartView_;
    QChart* latencyChart_;
    QLineSeries* readLatencySeries_;
    QLineSeries* writeLatencySeries_;
    
    QChartView* throughputChartView_;
    QChart* throughputChart_;
    // Labels
    QLabel* avgReadLabel_;
    QLabel* avgWriteLabel_;
    QLabel* totalOpsLabel_;
    QLabel* fragmentationLabel_;
    QLabel* latencyLabel_;
    QLabel* throughputLabel_;
    
    // Defragmentation Results
    QLabel* defragResultsLabel_;
    QLabel* beforeLatencyLabel_;
    QLabel* afterLatencyLabel_;
    QLabel* improvementLabel_;
    
    // Data storage (keep last N data points)
    std::deque<double> readLatencies_;
    std::deque<double> writeLatencies_;
    std::deque<qint64> timestamps_;
    static constexpr int MAX_DATA_POINTS = 100;
};

} // namespace FileSystemTool

#endif // PERFORMANCEWIDGET_H
