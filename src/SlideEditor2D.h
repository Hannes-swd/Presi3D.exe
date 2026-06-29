#pragma once
#include <QWidget>
#include <QHash>
#include <QPixmap>
#include "models/DataModel.h"

class QPlainTextEdit;
class QContextMenuEvent;
class QDragEnterEvent;
class QDropEvent;

class SlideEditor2D : public QWidget {
    Q_OBJECT
public:
    explicit SlideEditor2D(QWidget* parent = nullptr);

    void setSlide(Presentation* pres, int slideIndex);

signals:
    void presentationModified();
    void elementSelected(int elemIndex); // -1 = none

public slots:
    void addTextElement();
    void addShapeElement(const QString& shapeType = "rect");
    void addImageElement();
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
    bool eventFilter(QObject*, QEvent*) override;
    void dragEnterEvent(QDragEnterEvent*) override;
    void dropEvent(QDropEvent*) override;

private:
    // Coordinate helpers
    QRectF  slideRect() const;
    QRectF  elemToWidget(const SlideElement&) const;
    QPointF widgetToSlide(const QPointF&) const;
    int     hitTest(const QPointF& widgetPos) const;

    // Handle hit-test: returns 0-7 for resize handles, -1 if none
    // Handle order: TL=0, TR=1, BL=2, BR=3, TC=4, BC=5, ML=6, MR=7
    int  hitHandle(const QPointF& widgetPos) const;
    void applyResize(SlideElement&, int handle,
                     const QPointF& curSlide, float origX, float origY,
                     float origW, float origH, bool constrain) const;
    void addImageFromPath(const QString& path, QPointF widgetPos = {-1,-1});

    // Drawing
    void drawElement(QPainter&, const SlideElement&, bool selected) const;
    void drawHandles(QPainter&, const QRectF&) const;

    // Snap / alignment guides
    struct SnapGuide { bool vertical; float pos; }; // vertical=X guide, else Y guide
    QVector<SnapGuide> m_snapGuides;
    void applySnapAndGuides(SlideElement& e);

    // Inline text editing
    void startTextEdit(int elemIndex);
    void finishTextEdit();

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

    // Format painter
    bool         m_formatPainterMode = false;
    SlideElement m_formatTemplate;

    // Clipboard (shared across all editor instances)
    static SlideElement s_clipboard;
    static bool         s_hasClipboard;

    QPlainTextEdit* m_textEdit    = nullptr;
    int             m_editingElem = -1;

    mutable QHash<QString, QPixmap> m_pixmapCache;
};
