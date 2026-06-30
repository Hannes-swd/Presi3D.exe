#pragma once
#include <QWidget>
#include "models/DataModel.h"

class QFontComboBox;
class QSpinBox;
class QDoubleSpinBox;
class QPushButton;
class QComboBox;
class QLabel;

class FormatBar : public QWidget {
    Q_OBJECT
public:
    explicit FormatBar(QWidget* parent = nullptr);

    void setContext(Presentation* pres, int slideIdx, int elemIdx);
    void setTableCell(int row, int col);

signals:
    void modified();
    void formatPainterRequested();

private slots:
    void onFontChanged(const QFont& f);
    void onFontSizeChanged(int v);
    void onColorClicked();
    void onBgColorClicked();
    void onAlignLeft();
    void onAlignCenter();
    void onAlignRight();
    void onVAlignTop();
    void onVAlignMiddle();
    void onVAlignBottom();
    void onListBullet();
    void onListNumbered();
    void onBold();
    void onItalic();
    void onUnderline();
    void onStrikethrough();
    void onUnderlineColorClicked();
    void onUnderlineStyleChanged(int idx);
    void onXChanged(double v);
    void onYChanged(double v);
    void onWChanged(double v);
    void onHChanged(double v);

private:
    void refresh();
    void updateColorSwatch(QPushButton*, const QColor&);
    SlideElement* currentElem();
    TableCell*    currentCell();

    Presentation* m_pres      = nullptr;
    int           m_slideIdx  = -1;
    int           m_elemIdx   = -1;
    int           m_cellRow   = -1;
    int           m_cellCol   = -1;
    bool          m_updating  = false;

    // Text group
    QFontComboBox* m_fontCombo      = nullptr;
    QSpinBox*      m_fontSize       = nullptr;
    QPushButton*   m_colorBtn       = nullptr;
    QPushButton*   m_bgColorBtn     = nullptr;
    QPushButton*   m_alignLeft      = nullptr;
    QPushButton*   m_alignCenter    = nullptr;
    QPushButton*   m_alignRight     = nullptr;
    QPushButton*   m_vAlignTop      = nullptr;
    QPushButton*   m_vAlignMiddle   = nullptr;
    QPushButton*   m_vAlignBottom   = nullptr;
    QPushButton*   m_listBullet     = nullptr;
    QPushButton*   m_listNumbered   = nullptr;
    QPushButton*   m_boldBtn        = nullptr;
    QPushButton*   m_italicBtn      = nullptr;
    QPushButton*   m_underlineBtn   = nullptr;
    QPushButton*   m_strikeBtn      = nullptr;
    QPushButton*   m_ulColorBtn     = nullptr;
    QComboBox*     m_ulStyleCombo   = nullptr;
    QPushButton*   m_fmtPainterBtn  = nullptr;

    // Geometry group
    QDoubleSpinBox* m_posX  = nullptr;
    QDoubleSpinBox* m_posY  = nullptr;
    QDoubleSpinBox* m_sizeW = nullptr;
    QDoubleSpinBox* m_sizeH = nullptr;

    QWidget* m_textGroup = nullptr;
    QWidget* m_geomGroup = nullptr;
};
