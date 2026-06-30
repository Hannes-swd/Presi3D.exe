#pragma once
#include <QString>
#include <QVector>
#include <QColor>
#include <QMap>
#include <QUuid>

struct TableCell {
    QString text;
    QColor  bgColor;       // invalid = use table default
    QColor  textColor;     // invalid = use table default
    bool    bold      = false;
    bool    italic    = false;
    QString textAlign = "left";
    int     colspan   = 1;  // horizontal span
    int     rowspan   = 1;  // vertical span
    bool    merged    = false; // covered by another cell's span (skip drawing)
};

struct SlideElement {
    enum Type { Text, Shape, Image, Table };
    enum ListStyle { NoList = 0, Bullets, Numbered };

    QString id;
    Type    type = Text;
    QString content; // text string | shape type ("rect","circle","line") | image path
    int     listStyle = NoList;

    // Position and size in slide coordinates (0-1920 x 0-1080)
    float x      = 100.f;
    float y      = 100.f;
    float width  = 600.f;
    float height = 100.f;

    QColor color           = Qt::black;
    QColor backgroundColor = Qt::transparent;
    QColor borderColor     = Qt::darkGray;
    float  borderWidth     = 0.f;
    float  cornerRadius    = 0.f;
    int    fontSize        = 32;
    QString fontFamily     = "Arial";
    QString textAlignment         = "left"; // "left" | "center" | "right"
    QString verticalAlignment     = "top";  // "top" | "middle" | "bottom"
    bool    bold           = false;

    // Entrance animation (applied when slide becomes .present in browser)
    QString entranceAnim  = "";    // "" | "fadeIn" | "slideLeft" | "slideRight" | "slideUp" | "slideDown" | "zoomIn"
    float   animDelay     = 0.3f;  // seconds
    float   animDuration  = 0.5f;  // seconds
    bool    italic         = false;
    bool    underline      = false;
    bool    strikethrough  = false;
    QColor  underlineColor;          // invalid = same as text color
    int     underlineStyle = 0;      // 0=solid 1=dashed 2=dotted 3=wavy

    // ── Table (only when type == Table) ───────────────────────────────────────
    int     tableRows        = 3;
    int     tableCols        = 3;
    QVector<QVector<TableCell>> tableCells;   // [row][col]
    QVector<float> tableColFracs;             // column width fractions, sum=1
    QVector<float> tableRowFracs;             // row height fractions, sum=1
    QColor  tableBorderColor = QColor(180, 180, 180);
    float   tableBorderWidth = 1.f;
    bool    tableHasHeader   = true;
    QColor  tableHeaderBg    = QColor(60, 100, 200);
    QColor  tableHeaderText  = Qt::white;
    int     tableFontSize    = 20;
    QString tableFontFamily  = "Arial";
    QColor  tableDefaultBg   = Qt::white;
    QColor  tableDefaultText = Qt::black;

    void initTable(int rows, int cols) {
        type      = Table;
        tableRows = rows;
        tableCols = cols;
        tableCells    = QVector<QVector<TableCell>>(rows, QVector<TableCell>(cols));
        tableColFracs = QVector<float>(cols, 1.f / float(cols));
        tableRowFracs = QVector<float>(rows, 1.f / float(rows));
    }

    SlideElement() : id(QUuid::createUuid().toString(QUuid::WithoutBraces)) {}
};

struct Slide {
    QString              id;
    QString              name;
    QVector<SlideElement> elements;
    QColor               backgroundColor = Qt::transparent; // transparent = no background

    // 3D transform in impress.js coordinate units
    float posX  = 0.f;
    float posY  = 0.f;
    float posZ  = 0.f;
    float rotX  = 0.f; // degrees
    float rotY  = 0.f;
    float rotZ  = 0.f;
    float scale = 1.f;

    // Per-slide dimensions (0 = use presentation default)
    float slideWidth  = 0.f;
    float slideHeight = 0.f;

    // Camera view when this slide is active (3D preview + HTML export)
    // scale=1 → full slide visible, scale=2 → zoomed out (more context), scale=0.5 → zoomed in
    // viewOffsetX/Y shift the camera center relative to the slide center
    float viewOffsetX = 0.f;
    float viewOffsetY = 0.f;

    // Per-slide visibility overrides: when this slide is the active step,
    // what opacity should each other slide have?
    // Key = other slide's UUID. Missing entry = use Presentation::defaultInactiveOpacity.
    // 0.0 = completely hidden, 1.0 = fully visible.
    QMap<QString, float> visibilityOverrides;

    Slide() : id(QUuid::createUuid().toString(QUuid::WithoutBraces)) {}
};

class Presentation {
public:
    QVector<Slide> slides;
    QString        filePath;
    QString        exportPath; // last exported/imported folder
    bool           modified = false;

    // Project-wide settings
    QColor sceneBackground        = QColor(0x11, 0x11, 0x11);
    float  slideWidth             = 1920.f;
    float  slideHeight            = 1080.f;
    float  defaultInactiveOpacity = 0.3f; // opacity applied to slides with no per-slide override

    void addSlide(const QString& name = {});
    void removeSlide(int index);
    void duplicateSlide(int index);
    void moveSlide(int from, int to);

    Slide* slideAt(int index) {
        return (index >= 0 && index < slides.size()) ? &slides[index] : nullptr;
    }
    const Slide* slideAt(int index) const {
        return (index >= 0 && index < slides.size()) ? &slides[index] : nullptr;
    }
};
