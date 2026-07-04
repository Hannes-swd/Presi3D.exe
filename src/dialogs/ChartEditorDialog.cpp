#include "ChartEditorDialog.h"
#include "rendering/ChartRenderer.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QTabWidget>
#include <QTableWidget>
#include <QHeaderView>
#include <QLineEdit>
#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QColorDialog>
#include <QPainter>
#include <QScrollArea>
#include <QFrame>
#include <QSplitter>
#include <QInputDialog>

// ─── Live preview widget ──────────────────────────────────────────────────────

class ChartPreview : public QWidget {
public:
    ChartPreview(const ChartData* data, QWidget* parent = nullptr)
        : QWidget(parent), m_data(data) {
        setMinimumSize(200, 150);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    }
    void refresh() { update(); }

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        p.fillRect(rect(), QColor(255,255,255));
        p.setPen(QPen(QColor(220,220,220), 1));
        p.drawRect(rect().adjusted(0,0,-1,-1));
        if (m_data) {
            QRectF r(4, 4, width()-8, height()-8);
            ChartRenderer::paint(p, r, *m_data);
        }
    }
private:
    const ChartData* m_data;
};

// ─── Shared table stylesheet ──────────────────────────────────────────────────

static const char* TABLE_SS =
    "QTableWidget {"
    "  color: #111827;"
    "  background: #ffffff;"
    "  alternate-background-color: #f3f4f6;"
    "  gridline-color: #d1d5db;"
    "  selection-background-color: #dbeafe;"
    "  selection-color: #1e3a5f;"
    "}"
    "QTableWidget::item { color: #111827; padding: 2px 4px; }"
    "QHeaderView::section {"
    "  background: #e5e7eb;"
    "  color: #111827;"
    "  font-weight: bold;"
    "  font-size: 11px;"
    "  border: 1px solid #d1d5db;"
    "  padding: 4px 6px;"
    "}"
    "QHeaderView::section:hover { background: #d1fae5; color: #065f46; cursor: pointer; }";

static void styleTable(QTableWidget* t) {
    t->setStyleSheet(TABLE_SS);
}

// ─── Helper ───────────────────────────────────────────────────────────────────

QPushButton* ChartEditorDialog::makeColorButton(const QColor& c, QWidget* parent) {
    auto* btn = new QPushButton(parent);
    btn->setFixedSize(28, 22);
    updateColorButton(btn, c);
    return btn;
}

void ChartEditorDialog::updateColorButton(QPushButton* btn, const QColor& c) {
    QString hex = c.isValid() ? c.name() : "#ffffff";
    btn->setStyleSheet(QString("QPushButton{background:%1;border:1px solid #aaa;"
                               "border-radius:3px;}"
                               "QToolTip{background:#1f2937;color:#f9fafb;border:none;"
                               "padding:4px 8px;border-radius:4px;font-size:10px;}").arg(hex));
    btn->setProperty("color", c);
}

// ─── Constructor ──────────────────────────────────────────────────────────────

ChartEditorDialog::ChartEditorDialog(const ChartData& data, QWidget* parent)
    : QDialog(parent), m_data(data) {
    setWindowTitle("Edit Chart – " + ChartRenderer::typeName(data.type));
    setMinimumSize(700, 520);
    resize(820, 600);

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // Header
    auto* header = new QWidget(this);
    header->setStyleSheet("background:#f9fafb;border-bottom:1px solid #e5e7eb;");
    header->setFixedHeight(44);
    auto* hRow = new QHBoxLayout(header);
    hRow->setContentsMargins(16, 8, 16, 8);
    auto* titleIconLbl = new QLabel(header);
    titleIconLbl->setPixmap(ChartRenderer::typeIconPixmap(data.type, 18, QColor("#111827")));
    hRow->addWidget(titleIconLbl);
    auto* titleLbl = new QLabel(ChartRenderer::typeName(data.type), header);
    titleLbl->setStyleSheet("font-size:13px;font-weight:bold;color:#111827;");
    hRow->addWidget(titleLbl);
    hRow->addStretch();
    mainLayout->addWidget(header);

    // Splitter: tabs on left, preview on right
    auto* splitter = new QSplitter(Qt::Horizontal, this);
    splitter->setChildrenCollapsible(false);
    mainLayout->addWidget(splitter, 1);

    // Left: tab widget
    auto* tabWidget = new QTabWidget(splitter);
    tabWidget->setMinimumWidth(380);

    // Build tabs depending on chart type
    if (m_data.isDataChart()) {
        buildDataTab(tabWidget);
    } else if (m_data.isStructural()) {
        buildNodeTab(tabWidget);
    } else if (m_data.type == "timeline") {
        buildEventTab(tabWidget);
    } else if (m_data.type == "gantt") {
        buildTaskTab(tabWidget);
    } else if (m_data.type == "venn") {
        buildVennTab(tabWidget);
    }
    buildOptionsTab(tabWidget);

    // Right: live preview
    auto* previewContainer = new QWidget(splitter);
    previewContainer->setMinimumWidth(220);
    auto* pvLayout = new QVBoxLayout(previewContainer);
    pvLayout->setContentsMargins(0, 0, 0, 0);
    pvLayout->setSpacing(0);
    auto* pvHeader = new QLabel("Preview", previewContainer);
    pvHeader->setStyleSheet("background:#f3f4f6;color:#6b7280;font-size:10px;"
                            "font-weight:bold;padding:4px 8px;"
                            "border-bottom:1px solid #e5e7eb;");
    pvLayout->addWidget(pvHeader);
    auto* preview = new ChartPreview(&m_data, previewContainer);
    m_previewWidget = preview;
    pvLayout->addWidget(preview, 1);

    splitter->addWidget(tabWidget);
    splitter->addWidget(previewContainer);
    splitter->setStretchFactor(0, 3);
    splitter->setStretchFactor(1, 2);

    // Button box
    auto* sep = new QFrame(this);
    sep->setFrameShape(QFrame::HLine);
    sep->setStyleSheet("color:#e5e7eb;margin:0;");
    mainLayout->addWidget(sep);

    auto* btnBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    btnBox->setContentsMargins(12, 8, 12, 12);
    btnBox->button(QDialogButtonBox::Ok)->setText("Apply");
    mainLayout->addWidget(btnBox);

    connect(btnBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(btnBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

// ─── Data tab (bar/line/area/pie/donut/scatter) ───────────────────────────────

void ChartEditorDialog::buildDataTab(QTabWidget* tabs) {
    auto* w = new QWidget;
    auto* vbox = new QVBoxLayout(w);
    vbox->setContentsMargins(8, 8, 8, 8);
    vbox->setSpacing(6);

    // Toolbar
    auto* toolbar = new QHBoxLayout;
    toolbar->setSpacing(6);
    auto* addRowBtn = new QPushButton("+ Row", w);
    auto* delRowBtn = new QPushButton("- Row", w);
    addRowBtn->setFixedHeight(26);
    delRowBtn->setFixedHeight(26);
    toolbar->addWidget(addRowBtn);
    toolbar->addWidget(delRowBtn);

    bool singleSeries = (m_data.type == "pie" || m_data.type == "donut");
    if (!singleSeries) {
        auto* addSerBtn = new QPushButton("+ Series", w);
        auto* delSerBtn = new QPushButton("- Series", w);
        addSerBtn->setFixedHeight(26);
        delSerBtn->setFixedHeight(26);
        toolbar->addWidget(addSerBtn);
        toolbar->addWidget(delSerBtn);
        connect(addSerBtn, &QPushButton::clicked, this, &ChartEditorDialog::onAddSeries);
        connect(delSerBtn, &QPushButton::clicked, this, &ChartEditorDialog::onRemoveSeries);
    }
    toolbar->addStretch();
    vbox->addLayout(toolbar);

    // Color row for series (shown above the table)
    m_colorRow = new QWidget(w);
    auto* colorRowLayout = new QHBoxLayout(m_colorRow);
    colorRowLayout->setContentsMargins(0,0,0,0);
    colorRowLayout->setSpacing(4);
    vbox->addWidget(m_colorRow);

    // Main table
    m_dataTable = new QTableWidget(w);
    m_dataTable->setAlternatingRowColors(true);
    m_dataTable->horizontalHeader()->setStretchLastSection(false);
    m_dataTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    m_dataTable->horizontalHeader()->setMinimumSectionSize(60);
    m_dataTable->verticalHeader()->setVisible(false);
    m_dataTable->setSelectionMode(QAbstractItemView::ExtendedSelection);
    styleTable(m_dataTable);
    vbox->addWidget(m_dataTable, 1);

    // Double-click on column header → rename series
    connect(m_dataTable->horizontalHeader(), &QHeaderView::sectionDoubleClicked,
            this, [this](int col) {
        bool isPie = (m_data.type == "pie" || m_data.type == "donut");
        bool isScatter = (m_data.type == "scatter");
        // col 0 = "Kategorie" / "Beschriftung" → not a series, skip
        // for pie: col 1 = "Wert" → also fixed label, skip
        if (isPie) return;
        // For regular charts: col 0 = category label (skip), col 1+ = series
        int serIdx = -1;
        if (!isScatter && col >= 1) serIdx = col - 1;
        else if (isScatter && col >= 0) serIdx = col / 2;  // X/Y pairs
        if (serIdx < 0 || serIdx >= m_data.series.size()) return;

        bool ok;
        QString newName = QInputDialog::getText(
            this, "Rename Series",
            "New name for series " + QString::number(serIdx + 1) + ":",
            QLineEdit::Normal,
            m_data.series[serIdx].name, &ok);
        if (!ok || newName.trimmed().isEmpty()) return;
        syncDataFromTable();
        m_data.series[serIdx].name = newName.trimmed();
        rebuildDataTable();
        buildSeriesColorRow();
        if (m_previewWidget) m_previewWidget->update();
    });

    // Hint for header renaming
    auto* headerHint = new QLabel("Tip: Double-click a column header to rename it", w);
    headerHint->setStyleSheet("color:#6b7280;font-size:9px;font-style:italic;");
    vbox->addWidget(headerHint);

    // Hint
    QString hint;
    if (m_data.type == "scatter")
        hint = "Scatter: values as X,Y pairs per series (X and Y columns per series)";
    else if (singleSeries)
        hint = "Label and value per row. Color optional.";
    else
        hint = "First column = category name, others = series values.";
    auto* hintLbl = new QLabel(hint, w);
    hintLbl->setStyleSheet("color:#9ca3af;font-size:9px;");
    vbox->addWidget(hintLbl);

    rebuildDataTable();
    buildSeriesColorRow();

    connect(addRowBtn, &QPushButton::clicked, this, &ChartEditorDialog::onAddRow);
    connect(delRowBtn, &QPushButton::clicked, this, &ChartEditorDialog::onRemoveRow);
    connect(m_dataTable, &QTableWidget::cellChanged,
            this, &ChartEditorDialog::onDataCellChanged);

    tabs->addTab(w, "Data");
}

void ChartEditorDialog::buildSeriesColorRow() {
    if (!m_colorRow) return;
    QLayoutItem* child;
    while ((child = m_colorRow->layout()->takeAt(0)) != nullptr) {
        delete child->widget();
        delete child;
    }
    auto* lbl = new QLabel("Series colors:", m_colorRow);
    lbl->setStyleSheet("font-size:9px;color:#6b7280;");
    m_colorRow->layout()->addWidget(lbl);

    bool isPieLike = (m_data.type == "pie" || m_data.type == "donut");

    if (isPieLike && !m_data.series.isEmpty()) {
        const ChartSeries& s0 = m_data.series[0];
        for (int i = 0; i < m_data.labels.size(); ++i) {
            QString hex = (i < s0.valueColors.size()) ? s0.valueColors[i] : "";
            QColor c = hex.isEmpty() ? QColor(ChartData::defaultColor(i)) : QColor(hex);
            auto* btn = makeColorButton(c, m_colorRow);
            btn->setToolTip(i < m_data.labels.size() ? m_data.labels[i] : "");
            m_colorRow->layout()->addWidget(btn);
            connect(btn, &QPushButton::clicked, this, [this, i]() { onCellColorClicked(0, i); });
        }
    } else {
        for (int si = 0; si < m_data.series.size(); ++si) {
            QColor c = m_data.series[si].color.isEmpty()
                     ? QColor(ChartData::defaultColor(si))
                     : QColor(m_data.series[si].color);
            auto* btn = makeColorButton(c, m_colorRow);
            btn->setToolTip(m_data.series[si].name);
            m_colorRow->layout()->addWidget(btn);
            connect(btn, &QPushButton::clicked, this, [this, si]() { onSeriesColorClicked(si); });
        }
    }
    static_cast<QHBoxLayout*>(m_colorRow->layout())->addStretch();
}

void ChartEditorDialog::rebuildDataTable() {
    if (!m_dataTable) return;
    QSignalBlocker blk(m_dataTable);
    m_dataTable->clear();

    bool isPieLike = (m_data.type == "pie" || m_data.type == "donut");
    bool isScatter = (m_data.type == "scatter");

    if (isPieLike) {
        m_dataTable->setColumnCount(2);
        m_dataTable->setHorizontalHeaderLabels({"Label", "Value"});
        int n = m_data.labels.size();
        m_dataTable->setRowCount(n);
        const ChartSeries& s0 = m_data.series.isEmpty() ? ChartSeries{} : m_data.series[0];
        for (int r = 0; r < n; ++r) {
            m_dataTable->setItem(r, 0, new QTableWidgetItem(
                r < m_data.labels.size() ? m_data.labels[r] : ""));
            double val = (r < s0.values.size()) ? s0.values[r] : 0.0;
            m_dataTable->setItem(r, 1, new QTableWidgetItem(QString::number(val)));
        }
    } else if (isScatter) {
        int nSer = qMax(1, m_data.series.size());
        m_dataTable->setColumnCount(nSer * 2);
        QStringList headers;
        for (int s = 0; s < nSer; ++s) {
            QString sName = s < m_data.series.size() ? m_data.series[s].name : QString("Series %1").arg(s+1);
            headers << sName + " X" << sName + " Y";
        }
        m_dataTable->setHorizontalHeaderLabels(headers);
        int nRows = 0;
        for (const auto& s : m_data.series) nRows = qMax(nRows, s.values.size() / 2);
        m_dataTable->setRowCount(qMax(nRows, 1));
        for (int s = 0; s < m_data.series.size(); ++s) {
            const auto& vals = m_data.series[s].values;
            for (int r = 0; r < vals.size() / 2; ++r) {
                m_dataTable->setItem(r, s*2,   new QTableWidgetItem(QString::number(vals[r*2])));
                m_dataTable->setItem(r, s*2+1, new QTableWidgetItem(QString::number(vals[r*2+1])));
            }
        }
    } else {
        int nSer = qMax(1, m_data.series.size());
        m_dataTable->setColumnCount(1 + nSer);
        QStringList headers;
        headers << "Category";
        for (int s = 0; s < nSer; ++s)
            headers << (s < m_data.series.size() ? m_data.series[s].name : QString("Series %1").arg(s+1));
        m_dataTable->setHorizontalHeaderLabels(headers);
        int nRows = m_data.labels.size();
        m_dataTable->setRowCount(qMax(nRows, 1));
        for (int r = 0; r < m_data.labels.size(); ++r) {
            m_dataTable->setItem(r, 0, new QTableWidgetItem(m_data.labels[r]));
            for (int s = 0; s < m_data.series.size(); ++s) {
                double v = (r < m_data.series[s].values.size()) ? m_data.series[s].values[r] : 0.0;
                m_dataTable->setItem(r, 1+s, new QTableWidgetItem(QString::number(v)));
            }
        }
        // Series names are editable via the Options tab
    }
    m_dataTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    m_dataTable->horizontalHeader()->setStretchLastSection(true);
}

void ChartEditorDialog::syncDataFromTable() {
    if (!m_dataTable) return;
    bool isPieLike = (m_data.type == "pie" || m_data.type == "donut");
    bool isScatter = (m_data.type == "scatter");
    int nRows = m_dataTable->rowCount();

    if (isPieLike) {
        m_data.labels.clear();
        if (m_data.series.isEmpty()) m_data.series.append({"Shares", {}, "", {}});
        m_data.series[0].values.clear();
        for (int r = 0; r < nRows; ++r) {
            QTableWidgetItem* li = m_dataTable->item(r, 0);
            QTableWidgetItem* vi = m_dataTable->item(r, 1);
            m_data.labels << (li ? li->text() : "");
            m_data.series[0].values << (vi ? vi->text().toDouble() : 0.0);
        }
    } else if (isScatter) {
        int nSer = m_dataTable->columnCount() / 2;
        while (m_data.series.size() < nSer)
            m_data.series.append({"Series "+QString::number(m_data.series.size()+1), {}, "", {}});
        for (int s = 0; s < nSer && s < m_data.series.size(); ++s) {
            m_data.series[s].values.clear();
            for (int r = 0; r < nRows; ++r) {
                QTableWidgetItem* xi = m_dataTable->item(r, s*2);
                QTableWidgetItem* yi = m_dataTable->item(r, s*2+1);
                if (!xi || !yi) continue;
                m_data.series[s].values << xi->text().toDouble() << yi->text().toDouble();
            }
        }
    } else {
        int nSer = m_dataTable->columnCount() - 1;
        m_data.labels.clear();
        while (m_data.series.size() < nSer)
            m_data.series.append({"Series "+QString::number(m_data.series.size()+1), {}, "", {}});
        while (m_data.series.size() > nSer && !m_data.series.isEmpty())
            m_data.series.removeLast();
        for (int s = 0; s < nSer && s < m_data.series.size(); ++s)
            m_data.series[s].values.clear();
        for (int r = 0; r < nRows; ++r) {
            QTableWidgetItem* li = m_dataTable->item(r, 0);
            m_data.labels << (li ? li->text() : "");
            for (int s = 0; s < nSer && s < m_data.series.size(); ++s) {
                QTableWidgetItem* vi = m_dataTable->item(r, 1+s);
                m_data.series[s].values << (vi ? vi->text().toDouble() : 0.0);
            }
        }
    }
}

// ─── Node tab (flowchart/mindmap/orgchart/uml) ────────────────────────────────

void ChartEditorDialog::buildNodeTab(QTabWidget* tabs) {
    auto* w = new QWidget;
    auto* vbox = new QVBoxLayout(w);
    vbox->setContentsMargins(8, 8, 8, 8);
    vbox->setSpacing(6);

    auto* toolbar = new QHBoxLayout;
    auto* addBtn = new QPushButton("+ Node", w);
    auto* delBtn = new QPushButton("- Node", w);
    addBtn->setFixedHeight(26);
    delBtn->setFixedHeight(26);
    toolbar->addWidget(addBtn);
    toolbar->addWidget(delBtn);
    toolbar->addStretch();
    vbox->addLayout(toolbar);

    m_nodeTable = new QTableWidget(w);
    m_nodeTable->setAlternatingRowColors(true);
    m_nodeTable->verticalHeader()->setVisible(false);
    styleTable(m_nodeTable);
    bool isUml = (m_data.type == "uml");
    if (isUml) {
        m_nodeTable->setColumnCount(5);
        m_nodeTable->setHorizontalHeaderLabels({"ID","Class","Attributes","Methods","Connections"});
    } else {
        m_nodeTable->setColumnCount(4);
        m_nodeTable->setHorizontalHeaderLabels({"ID","Label","Shape","Connects to"});
    }
    m_nodeTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    vbox->addWidget(m_nodeTable, 1);

    auto* hintLbl = new QLabel(
        isUml ? "Connections: \"id|inheritance\" or \"id|association\" etc."
              : "Shape: rect | diamond | oval | rounded | parallelogram\n"
                "Connections: comma-separated IDs (e.g. n2,n3)", w);
    hintLbl->setStyleSheet("color:#9ca3af;font-size:9px;");
    vbox->addWidget(hintLbl);

    rebuildNodeTable();

    connect(addBtn, &QPushButton::clicked, this, &ChartEditorDialog::onNodeAddRow);
    connect(delBtn, &QPushButton::clicked, this, &ChartEditorDialog::onNodeRemoveRow);
    connect(m_nodeTable, &QTableWidget::cellChanged,
            this, &ChartEditorDialog::onNodeCellChanged);

    tabs->addTab(w, "Nodes");
}

void ChartEditorDialog::rebuildNodeTable() {
    if (!m_nodeTable) return;
    QSignalBlocker blk(m_nodeTable);
    bool isUml = (m_data.type == "uml");
    m_nodeTable->setRowCount(m_data.nodes.size());
    for (int r = 0; r < m_data.nodes.size(); ++r) {
        const ChartNode& n = m_data.nodes[r];
        if (isUml) {
            QStringList parts = n.label.split('|');
            m_nodeTable->setItem(r, 0, new QTableWidgetItem(n.id));
            m_nodeTable->setItem(r, 1, new QTableWidgetItem(parts.value(0)));
            m_nodeTable->setItem(r, 2, new QTableWidgetItem(parts.value(1)));
            m_nodeTable->setItem(r, 3, new QTableWidgetItem(parts.value(2)));
            m_nodeTable->setItem(r, 4, new QTableWidgetItem(n.edges.join(",")));
        } else {
            m_nodeTable->setItem(r, 0, new QTableWidgetItem(n.id));
            m_nodeTable->setItem(r, 1, new QTableWidgetItem(n.label));
            m_nodeTable->setItem(r, 2, new QTableWidgetItem(n.shape));
            m_nodeTable->setItem(r, 3, new QTableWidgetItem(n.edges.join(",")));
        }
    }
}

void ChartEditorDialog::syncNodesFromTable() {
    if (!m_nodeTable) return;
    bool isUml = (m_data.type == "uml");
    int nRows = m_nodeTable->rowCount();
    m_data.nodes.clear();
    for (int r = 0; r < nRows; ++r) {
        auto txt = [&](int c) {
            auto* it = m_nodeTable->item(r, c);
            return it ? it->text().trimmed() : QString();
        };
        ChartNode n;
        if (isUml) {
            n.id    = txt(0);
            n.label = txt(1) + "|" + txt(2) + "|" + txt(3);
            n.shape = "class";
            n.edges = txt(4).split(',', Qt::SkipEmptyParts);
        } else {
            n.id    = txt(0);
            n.label = txt(1);
            n.shape = txt(2).isEmpty() ? "rect" : txt(2);
            n.edges = txt(3).split(',', Qt::SkipEmptyParts);
        }
        if (n.id.isEmpty()) continue;
        m_data.nodes.append(n);
    }
}

// ─── Event tab (timeline) ─────────────────────────────────────────────────────

void ChartEditorDialog::buildEventTab(QTabWidget* tabs) {
    auto* w = new QWidget;
    auto* vbox = new QVBoxLayout(w);
    vbox->setContentsMargins(8, 8, 8, 8);
    vbox->setSpacing(6);

    auto* toolbar = new QHBoxLayout;
    auto* addBtn = new QPushButton("+ Event", w);
    auto* delBtn = new QPushButton("- Event", w);
    addBtn->setFixedHeight(26);
    delBtn->setFixedHeight(26);
    toolbar->addWidget(addBtn);
    toolbar->addWidget(delBtn);
    toolbar->addStretch();
    vbox->addLayout(toolbar);

    m_eventTable = new QTableWidget(w);
    m_eventTable->setColumnCount(4);
    m_eventTable->setHorizontalHeaderLabels({"Position (0-100)","Label","Description","Color (hex)"});
    m_eventTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_eventTable->verticalHeader()->setVisible(false);
    m_eventTable->setAlternatingRowColors(true);
    styleTable(m_eventTable);
    vbox->addWidget(m_eventTable, 1);

    rebuildEventTable();

    connect(addBtn, &QPushButton::clicked, this, &ChartEditorDialog::onEventAddRow);
    connect(delBtn, &QPushButton::clicked, this, &ChartEditorDialog::onEventRemoveRow);
    connect(m_eventTable, &QTableWidget::cellChanged,
            this, &ChartEditorDialog::onSpecialCellChanged);

    tabs->addTab(w, "Events");
}

void ChartEditorDialog::rebuildEventTable() {
    if (!m_eventTable) return;
    QSignalBlocker blk(m_eventTable);
    m_eventTable->setRowCount(m_data.events.size());
    for (int r = 0; r < m_data.events.size(); ++r) {
        const auto& ev = m_data.events[r];
        m_eventTable->setItem(r, 0, new QTableWidgetItem(QString::number(ev.pos, 'f', 1)));
        m_eventTable->setItem(r, 1, new QTableWidgetItem(ev.label));
        m_eventTable->setItem(r, 2, new QTableWidgetItem(ev.desc));
        m_eventTable->setItem(r, 3, new QTableWidgetItem(ev.color));
    }
}

void ChartEditorDialog::syncEventsFromTable() {
    if (!m_eventTable) return;
    int n = m_eventTable->rowCount();
    m_data.events.clear();
    for (int r = 0; r < n; ++r) {
        auto txt = [&](int c) { auto* it = m_eventTable->item(r,c); return it?it->text():QString(); };
        ChartTimelineEvent ev;
        ev.pos   = txt(0).toDouble();
        ev.label = txt(1);
        ev.desc  = txt(2);
        ev.color = txt(3);
        m_data.events.append(ev);
    }
}

// ─── Task tab (gantt) ─────────────────────────────────────────────────────────

void ChartEditorDialog::buildTaskTab(QTabWidget* tabs) {
    auto* w = new QWidget;
    auto* vbox = new QVBoxLayout(w);
    vbox->setContentsMargins(8, 8, 8, 8);
    vbox->setSpacing(6);

    auto* toolbar = new QHBoxLayout;
    auto* addBtn = new QPushButton("+ Task", w);
    auto* delBtn = new QPushButton("- Task", w);
    addBtn->setFixedHeight(26);
    delBtn->setFixedHeight(26);
    toolbar->addWidget(addBtn);
    toolbar->addWidget(delBtn);
    toolbar->addStretch();
    vbox->addLayout(toolbar);

    m_taskTable = new QTableWidget(w);
    m_taskTable->setColumnCount(4);
    m_taskTable->setHorizontalHeaderLabels({"Task","Start (0-100)","End (0-100)","Color (hex)"});
    m_taskTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_taskTable->verticalHeader()->setVisible(false);
    m_taskTable->setAlternatingRowColors(true);
    styleTable(m_taskTable);
    vbox->addWidget(m_taskTable, 1);

    auto* axisLbl = new QLabel("Time axis labels (comma-separated):", w);
    axisLbl->setStyleSheet("font-size:9px;color:#374151;margin-top:6px;");
    vbox->addWidget(axisLbl);
    auto* axisEdit = new QLineEdit(m_data.ganttAxisLabels.join(","), w);
    vbox->addWidget(axisEdit);
    connect(axisEdit, &QLineEdit::textChanged, this, [this, axisEdit]() {
        m_data.ganttAxisLabels = axisEdit->text().split(',', Qt::SkipEmptyParts);
        if (m_previewWidget) m_previewWidget->update();
    });

    rebuildTaskTable();

    connect(addBtn, &QPushButton::clicked, this, &ChartEditorDialog::onTaskAddRow);
    connect(delBtn, &QPushButton::clicked, this, &ChartEditorDialog::onTaskRemoveRow);
    connect(m_taskTable, &QTableWidget::cellChanged,
            this, &ChartEditorDialog::onSpecialCellChanged);

    tabs->addTab(w, "Tasks");
}

void ChartEditorDialog::rebuildTaskTable() {
    if (!m_taskTable) return;
    QSignalBlocker blk(m_taskTable);
    m_taskTable->setRowCount(m_data.tasks.size());
    for (int r = 0; r < m_data.tasks.size(); ++r) {
        const auto& t = m_data.tasks[r];
        m_taskTable->setItem(r, 0, new QTableWidgetItem(t.name));
        m_taskTable->setItem(r, 1, new QTableWidgetItem(QString::number(t.start,'f',1)));
        m_taskTable->setItem(r, 2, new QTableWidgetItem(QString::number(t.end,'f',1)));
        m_taskTable->setItem(r, 3, new QTableWidgetItem(t.color));
    }
}

void ChartEditorDialog::syncTasksFromTable() {
    if (!m_taskTable) return;
    int n = m_taskTable->rowCount();
    m_data.tasks.clear();
    for (int r = 0; r < n; ++r) {
        auto txt = [&](int c) { auto* it = m_taskTable->item(r,c); return it?it->text():QString(); };
        ChartGanttTask t;
        t.name  = txt(0);
        t.start = txt(1).toDouble();
        t.end   = txt(2).toDouble();
        t.color = txt(3);
        m_data.tasks.append(t);
    }
}

// ─── Venn tab ────────────────────────────────────────────────────────────────

void ChartEditorDialog::buildVennTab(QTabWidget* tabs) {
    auto* w = new QWidget;
    auto* vbox = new QVBoxLayout(w);
    vbox->setContentsMargins(8, 8, 8, 8);
    vbox->setSpacing(6);

    auto* toolbar = new QHBoxLayout;
    auto* addBtn = new QPushButton("+ Circle", w);
    auto* delBtn = new QPushButton("- Circle", w);
    addBtn->setFixedHeight(26);
    delBtn->setFixedHeight(26);
    toolbar->addWidget(addBtn);
    toolbar->addWidget(delBtn);
    toolbar->addStretch();
    vbox->addLayout(toolbar);

    m_vennTable = new QTableWidget(w);
    m_vennTable->setColumnCount(6);
    m_vennTable->setHorizontalHeaderLabels(
        {"Label","X (0-100)","Y (0-100)","Radius (0-50)","Color (hex)","Opacity (0-1)"});
    m_vennTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_vennTable->verticalHeader()->setVisible(false);
    m_vennTable->setAlternatingRowColors(true);
    styleTable(m_vennTable);
    vbox->addWidget(m_vennTable, 1);

    rebuildVennTable();

    connect(addBtn, &QPushButton::clicked, this, &ChartEditorDialog::onVennAddRow);
    connect(delBtn, &QPushButton::clicked, this, &ChartEditorDialog::onVennRemoveRow);
    connect(m_vennTable, &QTableWidget::cellChanged,
            this, &ChartEditorDialog::onSpecialCellChanged);

    tabs->addTab(w, "Circles");
}

void ChartEditorDialog::rebuildVennTable() {
    if (!m_vennTable) return;
    QSignalBlocker blk(m_vennTable);
    m_vennTable->setRowCount(m_data.vennCircles.size());
    for (int r = 0; r < m_data.vennCircles.size(); ++r) {
        const auto& vc = m_data.vennCircles[r];
        m_vennTable->setItem(r, 0, new QTableWidgetItem(vc.label));
        m_vennTable->setItem(r, 1, new QTableWidgetItem(QString::number(vc.cx,'f',1)));
        m_vennTable->setItem(r, 2, new QTableWidgetItem(QString::number(vc.cy,'f',1)));
        m_vennTable->setItem(r, 3, new QTableWidgetItem(QString::number(vc.radius,'f',1)));
        m_vennTable->setItem(r, 4, new QTableWidgetItem(vc.color));
        m_vennTable->setItem(r, 5, new QTableWidgetItem(QString::number(vc.opacity,'f',2)));
    }
}

void ChartEditorDialog::syncVennFromTable() {
    if (!m_vennTable) return;
    int n = m_vennTable->rowCount();
    m_data.vennCircles.clear();
    for (int r = 0; r < n; ++r) {
        auto txt = [&](int c) { auto* it = m_vennTable->item(r,c); return it?it->text():QString(); };
        ChartVennCircle vc;
        vc.label   = txt(0);
        vc.cx      = txt(1).toDouble();
        vc.cy      = txt(2).toDouble();
        vc.radius  = txt(3).toDouble();
        vc.color   = txt(4);
        vc.opacity = txt(5).toDouble();
        m_data.vennCircles.append(vc);
    }
}

// ─── Options tab ─────────────────────────────────────────────────────────────

void ChartEditorDialog::buildOptionsTab(QTabWidget* tabs) {
    auto* w = new QWidget;
    auto* form = new QFormLayout(w);
    form->setContentsMargins(12, 12, 12, 12);
    form->setSpacing(10);
    form->setRowWrapPolicy(QFormLayout::WrapLongRows);

    m_titleEdit = new QLineEdit(m_data.title, w);
    form->addRow("Title:", m_titleEdit);

    m_descEdit = new QLineEdit(m_data.description, w);
    form->addRow("Description:", m_descEdit);

    m_showLegend = new QCheckBox("Show legend", w);
    m_showLegend->setChecked(m_data.showLegend);
    form->addRow("", m_showLegend);

    m_showGrid = new QCheckBox("Show grid", w);
    m_showGrid->setChecked(m_data.showGrid);
    form->addRow("", m_showGrid);

    // Change chart type
    static const char* ALL_TYPES[] = {
        "bar","bar_h","line","area","pie","donut","scatter",
        "flowchart","mindmap","orgchart","uml","timeline","gantt","venn", nullptr
    };
    m_typeCombo = new QComboBox(w);
    for (int i = 0; ALL_TYPES[i]; ++i)
        m_typeCombo->addItem(ChartRenderer::typeName(ALL_TYPES[i]), ALL_TYPES[i]);
    {
        int idx = m_typeCombo->findData(m_data.type);
        if (idx >= 0) m_typeCombo->setCurrentIndex(idx);
    }
    form->addRow("Chart type:", m_typeCombo);

    connect(m_titleEdit,  &QLineEdit::textChanged, this, &ChartEditorDialog::onOptionsChanged);
    connect(m_descEdit,   &QLineEdit::textChanged, this, &ChartEditorDialog::onOptionsChanged);
    connect(m_showLegend, &QCheckBox::toggled,     this, &ChartEditorDialog::onOptionsChanged);
    connect(m_showGrid,   &QCheckBox::toggled,     this, &ChartEditorDialog::onOptionsChanged);
    connect(m_typeCombo, qOverload<int>(&QComboBox::currentIndexChanged),
            this, &ChartEditorDialog::onOptionsChanged);

    tabs->addTab(w, "Options");
}

// ─── Slots ────────────────────────────────────────────────────────────────────

void ChartEditorDialog::onAddRow() {
    if (!m_dataTable) return;
    syncDataFromTable();
    bool isPie = (m_data.type=="pie"||m_data.type=="donut");
    m_data.labels << (isPie ? "New" : "Category");
    if (!m_data.series.isEmpty()) {
        if (isPie) m_data.series[0].values << 0.0;
        else for (auto& s : m_data.series) s.values << 0.0;
    }
    rebuildDataTable();
    if (m_previewWidget) m_previewWidget->update();
}

void ChartEditorDialog::onRemoveRow() {
    if (!m_dataTable) return;
    int row = m_dataTable->currentRow();
    if (row < 0) row = m_dataTable->rowCount() - 1;
    if (row < 0) return;
    syncDataFromTable();
    if (row < m_data.labels.size()) m_data.labels.removeAt(row);
    for (auto& s : m_data.series)
        if (row < s.values.size()) s.values.removeAt(row);
    rebuildDataTable();
    if (m_previewWidget) m_previewWidget->update();
}

void ChartEditorDialog::onAddSeries() {
    if (!m_dataTable) return;
    syncDataFromTable();
    ChartSeries s;
    s.name = "Series " + QString::number(m_data.series.size() + 1);
    for (int i = 0; i < m_data.labels.size(); ++i) s.values << 0.0;
    m_data.series.append(s);
    rebuildDataTable();
    buildSeriesColorRow();
    if (m_previewWidget) m_previewWidget->update();
}

void ChartEditorDialog::onRemoveSeries() {
    if (!m_dataTable || m_data.series.size() <= 1) return;
    syncDataFromTable();
    m_data.series.removeLast();
    rebuildDataTable();
    buildSeriesColorRow();
    if (m_previewWidget) m_previewWidget->update();
}

void ChartEditorDialog::onDataCellChanged(int row, int col) {
    if (m_updating) return;
    syncDataFromTable();
    if (m_previewWidget) m_previewWidget->update();
}

void ChartEditorDialog::onCellColorClicked(int /*row*/, int sliceIdx) {
    bool isPie = (m_data.type=="pie"||m_data.type=="donut");
    if (!isPie || m_data.series.isEmpty()) return;
    QColor cur;
    if (sliceIdx < m_data.series[0].valueColors.size())
        cur = QColor(m_data.series[0].valueColors[sliceIdx]);
    if (!cur.isValid()) cur = QColor(ChartData::defaultColor(sliceIdx));
    QColor nc = QColorDialog::getColor(cur, this, "Choose Color");
    if (!nc.isValid()) return;
    while (m_data.series[0].valueColors.size() <= sliceIdx)
        m_data.series[0].valueColors << "";
    m_data.series[0].valueColors[sliceIdx] = nc.name();
    buildSeriesColorRow();
    if (m_previewWidget) m_previewWidget->update();
}

void ChartEditorDialog::onSeriesColorClicked(int serIdx) {
    if (serIdx >= m_data.series.size()) return;
    QColor cur = m_data.series[serIdx].color.isEmpty()
               ? QColor(ChartData::defaultColor(serIdx))
               : QColor(m_data.series[serIdx].color);
    QColor nc = QColorDialog::getColor(cur, this, "Choose Series Color");
    if (!nc.isValid()) return;
    m_data.series[serIdx].color = nc.name();
    buildSeriesColorRow();
    if (m_previewWidget) m_previewWidget->update();
}

void ChartEditorDialog::onOptionsChanged() {
    if (m_updating) return;
    if (m_titleEdit)  m_data.title       = m_titleEdit->text();
    if (m_descEdit)   m_data.description = m_descEdit->text();
    if (m_showLegend) m_data.showLegend  = m_showLegend->isChecked();
    if (m_showGrid)   m_data.showGrid    = m_showGrid->isChecked();
    if (m_typeCombo)  m_data.type        = m_typeCombo->currentData().toString();
    if (m_previewWidget) m_previewWidget->update();
}

void ChartEditorDialog::onNodeAddRow() {
    syncNodesFromTable();
    ChartNode n;
    n.id    = "n" + QString::number(m_data.nodes.size() + 1);
    n.label = "Node";
    n.shape = (m_data.type == "uml") ? "class" : "rect";
    m_data.nodes.append(n);
    rebuildNodeTable();
    if (m_previewWidget) m_previewWidget->update();
}

void ChartEditorDialog::onNodeRemoveRow() {
    if (!m_nodeTable) return;
    int row = m_nodeTable->currentRow();
    if (row < 0) row = m_nodeTable->rowCount() - 1;
    if (row < 0 || row >= m_data.nodes.size()) return;
    m_data.nodes.removeAt(row);
    rebuildNodeTable();
    if (m_previewWidget) m_previewWidget->update();
}

void ChartEditorDialog::onNodeCellChanged(int, int) {
    if (m_updating) return;
    syncNodesFromTable();
    if (m_previewWidget) m_previewWidget->update();
}

void ChartEditorDialog::onEventAddRow() {
    syncEventsFromTable();
    ChartTimelineEvent ev;
    ev.pos   = m_data.events.isEmpty() ? 0.0 : m_data.events.last().pos + 20.0;
    ev.label = "Event";
    m_data.events.append(ev);
    rebuildEventTable();
    if (m_previewWidget) m_previewWidget->update();
}

void ChartEditorDialog::onEventRemoveRow() {
    if (!m_eventTable) return;
    int row = m_eventTable->currentRow();
    if (row < 0) row = m_eventTable->rowCount()-1;
    if (row < 0 || row >= m_data.events.size()) return;
    m_data.events.removeAt(row);
    rebuildEventTable();
    if (m_previewWidget) m_previewWidget->update();
}

void ChartEditorDialog::onTaskAddRow() {
    syncTasksFromTable();
    ChartGanttTask t;
    t.name  = "Task " + QString::number(m_data.tasks.size() + 1);
    t.start = 0; t.end = 50;
    m_data.tasks.append(t);
    rebuildTaskTable();
    if (m_previewWidget) m_previewWidget->update();
}

void ChartEditorDialog::onTaskRemoveRow() {
    if (!m_taskTable) return;
    int row = m_taskTable->currentRow();
    if (row < 0) row = m_taskTable->rowCount()-1;
    if (row < 0 || row >= m_data.tasks.size()) return;
    m_data.tasks.removeAt(row);
    rebuildTaskTable();
    if (m_previewWidget) m_previewWidget->update();
}

void ChartEditorDialog::onVennAddRow() {
    syncVennFromTable();
    ChartVennCircle vc;
    vc.label   = "Set " + QString::number(m_data.vennCircles.size() + 1);
    vc.cx = 50; vc.cy = 50; vc.radius = 30;
    vc.color   = ChartData::defaultColor(m_data.vennCircles.size());
    m_data.vennCircles.append(vc);
    rebuildVennTable();
    if (m_previewWidget) m_previewWidget->update();
}

void ChartEditorDialog::onVennRemoveRow() {
    if (!m_vennTable) return;
    int row = m_vennTable->currentRow();
    if (row < 0) row = m_vennTable->rowCount()-1;
    if (row < 0 || row >= m_data.vennCircles.size()) return;
    m_data.vennCircles.removeAt(row);
    rebuildVennTable();
    if (m_previewWidget) m_previewWidget->update();
}

void ChartEditorDialog::onSpecialCellChanged(int, int) {
    if (m_updating) return;
    if (m_eventTable && sender() == m_eventTable) syncEventsFromTable();
    else if (m_taskTable && sender() == m_taskTable) syncTasksFromTable();
    else if (m_vennTable && sender() == m_vennTable) syncVennFromTable();
    if (m_previewWidget) m_previewWidget->update();
}
