#include "IconPickerDialog.h"
#include "IconPackManager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QScrollArea>
#include <QLabel>
#include <QLineEdit>
#include <QToolButton>
#include <QPushButton>
#include <QFrame>
#include <QPainter>
#include <QPixmap>
#include <QIcon>

namespace {

QPixmap renderIconPixmap(const QString& id, int w, int h, const QColor& fg) {
    QPixmap pm(w, h);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(Qt::NoPen);
    p.setBrush(fg);
    p.drawPath(IconUtils::iconToPath(id, QRectF(4, 4, w - 8, h - 8)));
    return pm;
}

const int kUnfilteredCap = 200;

} // namespace

IconPickerDialog::IconPickerDialog(QWidget* parent) : QDialog(parent)
{
    setWindowTitle("Insert Icon");
    setMinimumSize(620, 520);
    resize(660, 580);

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(10, 10, 10, 10);
    mainLayout->setSpacing(6);

    m_filterEdit = new QLineEdit(this);
    m_filterEdit->setPlaceholderText("Filter icons\xE2\x80\xA6");
    mainLayout->addWidget(m_filterEdit);

    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    m_gridContainer = new QWidget(scroll);
    scroll->setWidget(m_gridContainer);
    mainLayout->addWidget(scroll, 1);

    auto* bottomBar = new QHBoxLayout();
    auto* manageBtn = new QPushButton("Manage Icon Package\xE2\x80\xA6", this);
    connect(manageBtn, &QPushButton::clicked, this, [this]() {
        auto& mgr = IconPackManager::instance();
        if (mgr.isInstalled()) mgr.uninstallInteractive(this);
        else                    mgr.installInteractive(this);
        rebuildGrid(m_filterEdit->text());
    });
    bottomBar->addWidget(manageBtn);
    bottomBar->addStretch();
    auto* cancelBtn = new QPushButton("Cancel", this);
    cancelBtn->setFixedWidth(110);
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    bottomBar->addWidget(cancelBtn);
    mainLayout->addLayout(bottomBar);

    connect(m_filterEdit, &QLineEdit::textChanged, this, &IconPickerDialog::rebuildGrid);
    rebuildGrid(QString());
}

void IconPickerDialog::rebuildGrid(const QString& filter)
{
    delete m_gridContainer->layout();
    qDeleteAll(m_gridContainer->findChildren<QWidget*>(QString(), Qt::FindDirectChildrenOnly));

    auto* contentLayout = new QVBoxLayout(m_gridContainer);
    contentLayout->setContentsMargins(4, 4, 4, 8);
    contentLayout->setSpacing(2);

    auto& mgr = IconPackManager::instance();
    if (!mgr.isInstalled()) {
        auto* empty = new QLabel(
            "No icon package installed yet.\n\n"
            "Click \"Manage Icon Package\xE2\x80\xA6\" below to download the official "
            "Material Icons set (~2100 icons, one-time download, then available offline).",
            m_gridContainer);
        empty->setWordWrap(true);
        empty->setAlignment(Qt::AlignCenter);
        empty->setStyleSheet("color:#6b7280; padding:32px; font-size:12px;");
        contentLayout->addWidget(empty);
        contentLayout->addStretch();
        return;
    }

    const int COLS   = 6;
    const int ICON_PX = 40;
    const int BTN_W  = 84, BTN_H  = 76;
    const QString needle = filter.trimmed().toLower();

    static const char* BTN_SS =
        "QToolButton { border:1px solid transparent; border-radius:5px;"
        "  padding:1px; font-size:9px; color:#374151; background:transparent; }"
        "QToolButton:hover   { background:#eff6ff; border-color:#93c5fd; }"
        "QToolButton:pressed { background:#dbeafe; border-color:#3b82f6; }";

    auto* gridWidget = new QWidget(m_gridContainer);
    auto* grid = new QGridLayout(gridWidget);
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setSpacing(3);
    contentLayout->addWidget(gridWidget);

    int col = 0;
    int shown = 0;
    int totalMatching = 0;

    for (const auto& def : IconUtils::allIcons()) {
        if (!needle.isEmpty() &&
            !def.label.toLower().contains(needle) &&
            !def.id.toLower().contains(needle))
            continue;
        ++totalMatching;
        if (needle.isEmpty() && shown >= kUnfilteredCap) continue;
        ++shown;

        QPixmap pm = renderIconPixmap(def.id, ICON_PX, ICON_PX, QColor(55, 65, 81));

        auto* btn = new QToolButton(gridWidget);
        btn->setIcon(QIcon(pm));
        btn->setIconSize(QSize(ICON_PX, ICON_PX));
        btn->setText(def.label);
        btn->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        btn->setFixedSize(BTN_W, BTN_H);
        btn->setStyleSheet(BTN_SS);

        const QString iconId = def.id;
        connect(btn, &QToolButton::clicked, this, [this, iconId]() {
            m_selected = iconId;
            accept();
        });

        grid->addWidget(btn, col / COLS, col % COLS);
        ++col;
    }

    if (totalMatching == 0) {
        auto* emptyMsg = new QLabel("No icons match your filter.", m_gridContainer);
        emptyMsg->setStyleSheet("color:#9ca3af; padding:16px;");
        contentLayout->addWidget(emptyMsg);
    } else if (needle.isEmpty() && totalMatching > kUnfilteredCap) {
        auto* hint = new QLabel(
            QString("Showing %1 of %2 icons \xE2\x80\x94 type above to search the full set.")
                .arg(shown).arg(totalMatching),
            m_gridContainer);
        hint->setStyleSheet("color:#9ca3af; padding:8px; font-size:11px;");
        contentLayout->addWidget(hint);
    }

    contentLayout->addStretch();
}
