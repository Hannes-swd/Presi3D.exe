#pragma once
#include <QDialog>
#include "models/ChartData.h"

class QTabWidget;
class QTableWidget;
class QLineEdit;
class QCheckBox;
class QComboBox;
class QLabel;
class QPushButton;
class QWidget;
class QDoubleSpinBox;

// Full chart data editor dialog.
// Opened by double-clicking a chart element in the 2D editor.
class ChartEditorDialog : public QDialog {
    Q_OBJECT
public:
    explicit ChartEditorDialog(const ChartData& data, QWidget* parent = nullptr);
    ChartData chartData() const { return m_data; }

private slots:
    void onAddRow();
    void onRemoveRow();
    void onAddSeries();
    void onRemoveSeries();
    void onCellColorClicked(int row, int col);
    void onSeriesColorClicked(int serIdx);
    void onDataCellChanged(int row, int col);
    void onOptionsChanged();
    void onNodeAddRow();
    void onNodeRemoveRow();
    void onNodeCellChanged(int row, int col);
    void onEventAddRow();
    void onEventRemoveRow();
    void onTaskAddRow();
    void onTaskRemoveRow();
    void onVennAddRow();
    void onVennRemoveRow();
    void onSpecialCellChanged(int row, int col);

private:
    void buildDataTab(QTabWidget* tabs);
    void buildNodeTab(QTabWidget* tabs);
    void buildEventTab(QTabWidget* tabs);
    void buildTaskTab(QTabWidget* tabs);
    void buildVennTab(QTabWidget* tabs);
    void buildOptionsTab(QTabWidget* tabs);
    void buildSeriesColorRow();

    void rebuildDataTable();
    void syncDataFromTable();
    void rebuildNodeTable();
    void syncNodesFromTable();
    void rebuildEventTable();
    void syncEventsFromTable();
    void rebuildTaskTable();
    void syncTasksFromTable();
    void rebuildVennTable();
    void syncVennFromTable();

    // Color button helper
    QPushButton* makeColorButton(const QColor& c, QWidget* parent);
    void         updateColorButton(QPushButton* btn, const QColor& c);

    ChartData m_data;
    bool      m_updating = false;

    // Data tab (for data charts)
    QTableWidget* m_dataTable   = nullptr;
    QWidget*      m_colorRow    = nullptr;  // series color buttons row

    // Node tab (structural charts)
    QTableWidget* m_nodeTable   = nullptr;

    // Event tab (timeline)
    QTableWidget* m_eventTable  = nullptr;

    // Task tab (gantt)
    QTableWidget* m_taskTable   = nullptr;

    // Venn tab
    QTableWidget* m_vennTable   = nullptr;

    // Options tab
    QLineEdit*  m_titleEdit   = nullptr;
    QLineEdit*  m_descEdit    = nullptr;
    QCheckBox*  m_showLegend  = nullptr;
    QCheckBox*  m_showGrid    = nullptr;
    QComboBox*  m_typeCombo   = nullptr;

    // Live preview area
    QWidget*    m_previewWidget = nullptr;
};
