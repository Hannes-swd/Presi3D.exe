#pragma once
#include <QWidget>
#include <QHash>
#include <QPixmap>
#include "models/DataModel.h"
#include "dialogs/InsertButtonDialog.h"
#include "dialogs/InsertCheckboxDialog.h"
#include "dialogs/InsertSliderDialog.h"

class QTimer;
class QContextMenuEvent;
class QDragEnterEvent;
class QDropEvent;
class QPainter;

class SlideEditor2D : public QWidget {
    Q_OBJECT
public:
    explicit SlideEditor2D(QWidget* parent = nullptr);

    void setSlide(Presentation* pres, int slideIndex);

signals:
    void presentationModified();
    void elementSelected(int elemIndex); // -1 = none
    void tableCellSelected(int row, int col); // -1/-1 = none

public slots:
    void addTextElement();
    void addShapeElement(const QString& shapeType = "rect");
    void addImageElement();
    void addTableElement(int rows, int cols);
    void addChartElement(const QString& chartType);
    void openChartEditor();  // open editor for selected chart element
    void addFormulaElement(const QString& latex);
    void openFormulaEditor(); // open editor for selected formula element
    void addIFrameElement(const QString& url);
    void openIFrameEditor(); // open editor for selected iframe element
    void addButtonElement(const ButtonConfig& cfg);
    void openButtonEditor(); // open editor for selected button element
    void addCheckboxElement(const CheckboxConfig& cfg);
    void openCheckboxEditor(); // open editor for selected checkbox element
    void addSliderElement(const SliderConfig& cfg);
    void openSliderEditor(); // open editor for selected slider element
    void deleteSelectedElement();
    void copySelectedElement();
    void pasteElement();
    void activateFormatPainter(const SlideElement& source);

    // Layer / z-order
    void bringToFront();
    void bringForward();
    void sendBackward();
    void sendToBack();

protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
    void mouseDoubleClickEvent(QMouseEvent*) override;
    void keyPressEvent(QKeyEvent*) override;
    void resizeEvent(QResizeEvent*) override;
    void contextMenuEvent(QContextMenuEvent*) override;
    void focusOutEvent(QFocusEvent*) override;
    void dragEnterEvent(QDragEnterEvent*) override;
    void dropEvent(QDropEvent*) override;

private:
    // Coordinate helpers
    QRectF  slideRect() const;
    QRectF  elemToWidget(const SlideElement&) const;
    QPointF widgetToSlide(const QPointF&) const;
    int     hitTest(const QPointF& widgetPos) const;

    // Table helpers
    struct CellPos { int row = -1; int col = -1; bool valid() const { return row >= 0 && col >= 0; } };
    struct DividerHit { bool valid = false; bool isCol; int idx; };
    CellPos    hitTableCell(int elemIdx, QPointF wpos) const;
    DividerHit hitTableDivider(int elemIdx, QPointF wpos) const;
    QRectF     cellRect(const SlideElement& e, int row, int col) const;
    void       drawTableElement(QPainter& p, const SlideElement& e, bool selected,
                                const QString& currentSlideId) const;
    void       handleTableKey(QKeyEvent*);
    void       pasteExcelIntoTable(SlideElement& e);
    void       exitTableEditMode();
    void       mergeCells();
    void       unmergeCells();

    // Handle hit-test: returns 0-7 for resize handles, 8 for rotation handle, -1 if none
    // Handle order: TL=0, TR=1, BL=2, BR=3, TC=4, BC=5, ML=6, MR=7, Rot=8
    int  hitHandle(const QPointF& widgetPos) const;
    void applyResize(SlideElement&, int handle,
                     const QPointF& curSlide, float origX, float origY,
                     float origW, float origH, bool constrain) const;
    void addImageFromPath(const QString& path, QPointF widgetPos = {-1,-1});

    // Drawing
    void drawElement(QPainter&, const SlideElement&, bool selected, bool isBeingEdited,
                      const QString& currentSlideId) const;
    QString substituteVars(const QString& raw, const QString& currentSlideId) const;
    void drawHandles(QPainter&, const QRectF&, float rotation = 0.f) const;
    void drawTextCursor(QPainter&, const SlideElement&) const;

    // Text-edit helper: returns the text string being edited (content or shapeText)
    const QString& getEditText(const SlideElement& e) const;

    // Snap / alignment guides
    struct SnapGuide { bool vertical; float pos; }; // vertical=X guide, else Y guide
    QVector<SnapGuide> m_snapGuides;
    void applySnapAndGuides(SlideElement& e);

    // Inline text editing (direct WYSIWYG — no overlay widget)
    void startTextEdit(int elemIndex, QPointF clickPos = {-1,-1});
    void finishTextEdit();
    void handleTextEditKey(QKeyEvent*);
    int  textPositionAt(const SlideElement&, QPointF widgetPos) const;

    Presentation* m_pres         = nullptr;
    int           m_slideIndex   = -1;
    int           m_selectedElem = -1;

    // Move drag
    bool    m_dragging   = false;
    QPointF m_dragStartSlide; // drag origin in slide coords (for move & resize)
    QPointF m_dragOrigin;     // original element (x,y)

    // Resize drag
    int   m_resizingHandle = -1; // -1 = not resizing
    float m_resizeOrigX = 0, m_resizeOrigY = 0;
    float m_resizeOrigW = 0, m_resizeOrigH = 0;

    // Rotation drag
    bool  m_rotatingHandle   = false;
    float m_rotateStartAngle = 0.f; // atan2 angle at drag start (degrees)
    float m_rotateOrigAngle  = 0.f; // element.rotation at drag start

    // Format painter
    bool         m_formatPainterMode = false;
    SlideElement m_formatTemplate;

    // Clipboard (shared across all editor instances)
    static SlideElement s_clipboard;
    static bool         s_hasClipboard;

    // Inline text editing state
    int     m_editingElem      = -1;
    int     m_cursorPos        = 0;
    int     m_selAnchor        = -1;  // -1 = no selection
    bool    m_cursorVisible    = false;
    bool    m_textSelecting    = false; // mouse drag for selection
    bool    m_editingShapeText = false; // editing shapeText (vs content)
    QTimer* m_cursorBlink      = nullptr;

    mutable QHash<QString, QPixmap> m_pixmapCache;

    // Table edit state
    bool  m_tableEditMode    = false;
    int   m_selTableRow      = -1;
    int   m_selTableCol      = -1;
    bool  m_tableCellEditing = false;  // cursor active in cell
    int   m_tableCursorPos   = 0;
    int   m_tableSelAnchor   = -1;

    // Multi-cell selection (drag or shift+click)
    int   m_cellSelAnchorRow  = -1;
    int   m_cellSelAnchorCol  = -1;
    bool  m_isDraggingCellSel = false;

    // Table column/row drag resize
    DividerHit m_dragDivider;
    float      m_divDragStart = 0.f;  // widget coord when drag started
    float      m_divFracA = 0.f;      // original frac of divider[idx]
    float      m_divFracB = 0.f;      // original frac of divider[idx+1]
};
