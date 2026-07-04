#include <QApplication>
#include "MainWindow.h"
#include "dialogs/StartDialog.h"

static const char* DARK_QSS = R"(
* { font-family: "Segoe UI", Arial, sans-serif; font-size: 11px; outline: none; }

QWidget  { background: #ffffff; color: #111827; }
QDialog  { background: #ffffff; }

QMenuBar                { background: #ffffff; border-bottom: 1px solid #e5e7eb; padding: 1px 2px; }
QMenuBar::item          { background: transparent; padding: 4px 10px; border-radius: 4px; }
QMenuBar::item:selected { background: #f3f4f6; }
QMenuBar::item:pressed  { background: #2563eb; color: #fff; }
QMenu                   { background: #ffffff; border: 1px solid #e5e7eb; padding: 4px 0; color: #111827; }
QMenu::item             { padding: 6px 22px 6px 10px; border-radius: 4px; margin: 1px 4px; color: #111827; }
QMenu::item:selected    { background: #eff6ff; color: #2563eb; }
QMenu::item:disabled    { color: #9ca3af; }
QMenu::separator        { height: 1px; background: #e5e7eb; margin: 4px 8px; }

QToolBar              { background: #ffffff; border: none; border-bottom: 1px solid #e5e7eb; padding: 3px 4px; spacing: 2px; }
QToolBar::separator   { background: #e5e7eb; width: 1px; margin: 4px 2px; }
QToolButton           { background: transparent; border: 1px solid transparent; border-radius: 4px; padding: 4px 8px; color: #111827; }
QToolButton:hover     { background: #f3f4f6; border-color: #e5e7eb; }
QToolButton:pressed   { background: #eff6ff; color: #2563eb; border-color: #bfdbfe; }

QSplitter::handle:horizontal { width: 1px;  background: #e5e7eb; }
QSplitter::handle:vertical   { height: 1px; background: #e5e7eb; }

QScrollArea { border: none; }

QGroupBox {
    background: transparent;
    border: 1px solid #e5e7eb;
    border-radius: 6px;
    margin-top: 18px;
    padding: 6px 4px 4px 4px;
}
QGroupBox::title {
    subcontrol-origin: margin;
    subcontrol-position: top left;
    left: 8px;
    padding: 2px 6px;
    color: #9ca3af;
    background: #ffffff;
    font-weight: bold;
    font-size: 9px;
}
QGroupBox QLabel    { color: #111827; background: transparent; }
QGroupBox QCheckBox { color: #111827; }
QDialog   QLabel    { color: #111827; }

QLineEdit, QSpinBox, QDoubleSpinBox, QComboBox, QFontComboBox {
    background: #ffffff;
    border: 1px solid #d1d5db;
    border-radius: 4px;
    padding: 3px 6px;
    color: #111827;
    selection-background-color: #bfdbfe;
    selection-color: #1e40af;
    min-height: 20px;
}
QLineEdit:focus, QSpinBox:focus, QDoubleSpinBox:focus, QComboBox:focus, QFontComboBox:focus { border-color: #2563eb; }
QLineEdit:hover, QSpinBox:hover, QDoubleSpinBox:hover, QComboBox:hover, QFontComboBox:hover { border-color: #9ca3af; }

QAbstractSpinBox::up-button   { subcontrol-origin: border; subcontrol-position: top right; width: 16px; border-left: 1px solid #d1d5db; background: #f9fafb; }
QAbstractSpinBox::down-button { subcontrol-origin: border; subcontrol-position: bottom right; width: 16px; border-left: 1px solid #d1d5db; background: #f9fafb; }
QAbstractSpinBox::up-button:hover, QAbstractSpinBox::down-button:hover { background: #e5e7eb; }
QAbstractSpinBox::up-arrow   { width: 0; height: 0; border-left: 4px solid transparent; border-right: 4px solid transparent; border-bottom: 5px solid #4b5563; }
QAbstractSpinBox::down-arrow { width: 0; height: 0; border-left: 4px solid transparent; border-right: 4px solid transparent; border-top: 5px solid #4b5563; }

QComboBox::drop-down { border: none; width: 20px; background: transparent; }
QComboBox QAbstractItemView { background: #ffffff; border: 1px solid #d1d5db; selection-background-color: #eff6ff; selection-color: #2563eb; color: #111827; outline: none; }

QPushButton          { background: #ffffff; border: 1px solid #d1d5db; border-radius: 4px; padding: 4px 12px; min-height: 22px; color: #111827; }
QPushButton:hover    { background: #f9fafb; border-color: #9ca3af; }
QPushButton:pressed  { background: #eff6ff; border-color: #2563eb; color: #2563eb; }
QPushButton:checked  { background: #eff6ff; border-color: #2563eb; color: #2563eb; }
QPushButton:disabled { color: #9ca3af; border-color: #e5e7eb; background: #f9fafb; }

QCheckBox            { spacing: 6px; background: transparent; }
QCheckBox::indicator { width: 14px; height: 14px; border: 1px solid #d1d5db; border-radius: 3px; background: #ffffff; }
QCheckBox::indicator:checked { background: #2563eb; border-color: #2563eb; }
QCheckBox::indicator:hover   { border-color: #9ca3af; }

QLabel { background: transparent; color: #111827; }
QFrame { color: #e5e7eb; }

QListWidget                { background: #ffffff; border: none; outline: none; color: #111827; }
QListWidget::item          { padding: 4px; border-radius: 4px; margin: 1px 2px; color: #111827; }
QListWidget::item:hover    { background: #f3f4f6; color: #111827; }
QListWidget::item:selected { background: #eff6ff; color: #2563eb; }

QScrollBar:vertical               { background: #f9fafb; width: 8px; border: none; margin: 0; }
QScrollBar::handle:vertical       { background: #d1d5db; border-radius: 4px; min-height: 24px; }
QScrollBar::handle:vertical:hover { background: #9ca3af; }
QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: none; }
QScrollBar:horizontal               { background: #f9fafb; height: 8px; border: none; margin: 0; }
QScrollBar::handle:horizontal       { background: #d1d5db; border-radius: 4px; min-width: 24px; }
QScrollBar::handle:horizontal:hover { background: #9ca3af; }
QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width: 0; }
QScrollBar::add-page:horizontal, QScrollBar::sub-page:horizontal { background: none; }

QStatusBar        { background: #f3f4f6; color: #6b7280; font-size: 10px; border-top: 1px solid #e5e7eb; }
QStatusBar QLabel { color: #6b7280; background: transparent; }

QToolTip { background: #1f2937; color: #f9fafb; border: none; padding: 4px 8px; border-radius: 4px; font-size: 10px; }
)";

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("Presi 3D");
    app.setStyleSheet(QString::fromUtf8(DARK_QSS));
    app.setApplicationVersion(QStringLiteral(APP_VERSION));
    app.setOrganizationName("presiEditor");

    StartDialog start;
    if (start.exec() != QDialog::Accepted)
        return 0;

    MainWindow w;
    if (start.choice() == StartDialog::OpenProject)
        w.openPresentationFromFolder(start.selectedPath());

    w.show();
    return app.exec();
}
