#include "SlideListPanel.h"
#include "rendering/ChartRenderer.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QMenu>
#include <QInputDialog>
#include <QPainter>

static const int THUMB_W = 120;
static const int THUMB_H = 68;

SlideListPanel::SlideListPanel(QWidget* parent) : QWidget(parent) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(4);

    auto* title = new QLabel("SLIDES", this);
    title->setStyleSheet("font-weight: bold; font-size: 10px; color: #374151; padding: 8px 8px 4px 8px; letter-spacing: 1px;");
    layout->addWidget(title);

    m_list = new QListWidget(this);
    m_list->setDragDropMode(QAbstractItemView::InternalMove);
    m_list->setContextMenuPolicy(Qt::CustomContextMenu);
    m_list->setIconSize(QSize(THUMB_W, THUMB_H));
    m_list->setSpacing(2);
    layout->addWidget(m_list, 1);

    auto* btnRow = new QHBoxLayout();
    m_addBtn    = new QPushButton("+ Neue Slide", this);
    m_removeBtn = new QPushButton("Löschen", this);
    btnRow->addWidget(m_addBtn);
    btnRow->addWidget(m_removeBtn);
    layout->addLayout(btnRow);

    connect(m_list, &QListWidget::currentRowChanged,
            this, &SlideListPanel::onCurrentRowChanged);
    connect(m_list, &QListWidget::customContextMenuRequested,
            this, &SlideListPanel::onContextMenu);
    connect(m_list, &QListWidget::itemDoubleClicked,
            this, &SlideListPanel::onItemDoubleClicked);
    connect(m_addBtn, &QPushButton::clicked,
            this, &SlideListPanel::slideAdded);
    connect(m_removeBtn, &QPushButton::clicked, this, [this]() {
        int r = m_list->currentRow();
        if (r >= 0) emit slideRemoved(r);
    });

    // Drag-drop reorder
    connect(m_list->model(), &QAbstractItemModel::rowsMoved, this,
        [this](const QModelIndex&, int src, int /*srcEnd*/,
               const QModelIndex&, int dst) {
            int to = dst > src ? dst - 1 : dst;
            emit slideMoved(src, to);
        });
}

void SlideListPanel::setPresentation(Presentation* pres) {
    m_pres = pres;
    populate();
}

void SlideListPanel::setSelectedSlide(int index) {
    m_updating = true;
    m_list->setCurrentRow(index);
    m_updating = false;
}

void SlideListPanel::populate() {
    if (!m_pres) return;
    m_updating = true;

    int cur = m_list->currentRow();
    m_list->clear();

    for (int i = 0; i < m_pres->slides.size(); ++i) {
        const Slide& s = m_pres->slides[i];
        auto* item = new QListWidgetItem();
        item->setText(s.name);
        item->setIcon(QIcon(makeThumbnail(s)));
        m_list->addItem(item);
    }

    m_list->setCurrentRow(qBound(0, cur, m_list->count() - 1));
    m_updating = false;
}

QPixmap SlideListPanel::makeThumbnail(const Slide& slide) const {
    QPixmap pix(THUMB_W, THUMB_H);
    pix.fill(slide.backgroundColor);

    QPainter p(&pix);
    p.setRenderHint(QPainter::Antialiasing);

    const double sx = THUMB_W / 1920.0;
    const double sy = THUMB_H / 1080.0;

    for (const auto& elem : slide.elements) {
        QRectF r(elem.x * sx, elem.y * sy, elem.width * sx, elem.height * sy);
        if (elem.type == SlideElement::Text) {
            p.setPen(elem.color);
            QFont f(elem.fontFamily, qMax(1, (int)(elem.fontSize * sy)));
            p.setFont(f);
            p.drawText(r, Qt::AlignLeft | Qt::AlignVCenter | Qt::TextWordWrap, elem.content);
        } else if (elem.type == SlideElement::Shape) {
            p.setPen(QPen(elem.borderColor, 1));
            p.setBrush(elem.backgroundColor);
            if (elem.content == "circle")
                p.drawEllipse(r);
            else
                p.drawRect(r);
        } else if (elem.type == SlideElement::Image) {
            p.fillRect(r, QColor(200, 200, 220));
        } else if (elem.type == SlideElement::Table) {
            p.fillRect(r, elem.tableDefaultBg);
            int fontPx = qMax(1, int(elem.tableFontSize * sy));
            float rowY = float(r.y());
            for (int row = 0; row < elem.tableRows && row < elem.tableCells.size(); ++row) {
                float rowH = elem.tableRowFracs[row] * float(r.height());
                float colX = float(r.x());
                for (int col = 0; col < elem.tableCols && col < elem.tableCells[row].size(); ++col) {
                    float colW = elem.tableColFracs[col] * float(r.width());
                    const TableCell& cell = elem.tableCells[row][col];
                    if (cell.merged) { colX += colW; continue; }
                    // Span
                    int cs = qMax(1, qMin(cell.colspan, elem.tableCols - col));
                    int rs = qMax(1, qMin(cell.rowspan, elem.tableRows - row));
                    float cw = 0, rh = 0;
                    for (int c2 = col; c2 < col + cs && c2 < elem.tableCols; ++c2)
                        cw += elem.tableColFracs[c2] * float(r.width());
                    for (int r2 = row; r2 < row + rs && r2 < elem.tableRows; ++r2)
                        rh += elem.tableRowFracs[r2] * float(r.height());
                    QRectF cr(colX, rowY, cw, rh);
                    bool isHdr = row == 0 && elem.tableHasHeader;
                    QColor bg = isHdr ? elem.tableHeaderBg
                              : (cell.bgColor.isValid() ? cell.bgColor : elem.tableDefaultBg);
                    p.fillRect(cr, bg);
                    if (elem.tableBorderWidth > 0) {
                        p.setPen(QPen(elem.tableBorderColor, 0.5));
                        p.setBrush(Qt::NoBrush);
                        p.drawRect(cr);
                    }
                    QColor tc = isHdr ? elem.tableHeaderText
                               : (cell.textColor.isValid() ? cell.textColor : elem.tableDefaultText);
                    QFont f(elem.tableFontFamily, fontPx);
                    f.setBold(cell.bold || isHdr);
                    p.setFont(f);
                    p.setPen(tc);
                    p.drawText(cr.adjusted(2,1,-2,-1),
                               int(Qt::AlignVCenter | Qt::AlignLeft | Qt::TextWordWrap),
                               cell.text);
                    colX += colW;
                }
                rowY += rowH;
            }
            if (elem.tableBorderWidth > 0) {
                p.setPen(QPen(elem.tableBorderColor, 1));
                p.setBrush(Qt::NoBrush);
                p.drawRect(r);
            }
        } else if (elem.type == SlideElement::Chart) {
            p.fillRect(r, Qt::white);
            ChartRenderer::paint(p, r, elem.chartData);
        }
    }

    p.setPen(QPen(Qt::gray, 1));
    p.setBrush(Qt::NoBrush);
    p.drawRect(0, 0, THUMB_W - 1, THUMB_H - 1);
    return pix;
}

void SlideListPanel::onCurrentRowChanged(int row) {
    if (!m_updating && row >= 0)
        emit slideSelected(row);
}

void SlideListPanel::onContextMenu(const QPoint& pos) {
    int row = m_list->currentRow();
    if (row < 0 || !m_pres) return;

    QMenu menu(this);
    menu.addAction("Umbenennen", this, [this, row]() {
        if (row >= m_pres->slides.size()) return;
        bool ok;
        QString name = QInputDialog::getText(this, "Slide umbenennen", "Name:",
            QLineEdit::Normal, m_pres->slides[row].name, &ok);
        if (ok && !name.isEmpty())
            emit slideRenamed(row, name);
    });
    menu.addAction("Duplizieren", this, [this, row]() { emit slideDuplicated(row); });
    menu.addSeparator();
    menu.addAction("Löschen",    this, [this, row]() { emit slideRemoved(row); });
    menu.exec(m_list->mapToGlobal(pos));
}

void SlideListPanel::onItemDoubleClicked(QListWidgetItem* item) {
    if (!item || !m_pres) return;
    int row = m_list->row(item);
    if (row < 0 || row >= m_pres->slides.size()) return;
    bool ok;
    QString name = QInputDialog::getText(this, "Slide umbenennen", "Name:",
        QLineEdit::Normal, m_pres->slides[row].name, &ok);
    if (ok && !name.isEmpty())
        emit slideRenamed(row, name);
}
