#pragma once
#include <QString>
#include <QVector>
#include <QColor>
#include <QMap>
#include <QUuid>
#include "models/ChartData.h"
#include "models/VariableModel.h"
#include "models/TimelineTrack.h"
#include "models/MeshGradientData.h"

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

// A sub-range of a Text element's `content` marked as an inline code block.
// Offsets are plain QChar indices into `content`, same convention as
// SlideEditor2D's m_cursorPos/m_selAnchor — not a general rich-text run.
struct CodeSpan {
    int     start    = 0;
    int     length   = 0;
    QString language; // hljs "language-xxx" alias, e.g. "javascript", "plaintext"
};

struct SlideElement {
    enum Type { Text, Shape, Image, Table, Chart, Formula, IFrame, Button, Checkbox, Slider, Icon, Video, Audio };
    enum ListStyle { NoList = 0, Bullets, Numbered };

    QString id;
    QString groupId; // empty = not part of a group; shared value links elements into one group
    Type    type = Text;
    QString content; // text string | shape type ("rect","circle","line") | image path | LaTeX (Formula) | button label (Button) | icon id (Icon)
    int     listStyle = NoList;

    // ── Inline code blocks (Text elements only) ───────────────────────────────
    QVector<CodeSpan> codeSpans;

    // ── Hyperlink (Text elements) ────────────────────────────────────────────
    QString hyperlink; // URL the whole text element links to; empty = no link

    // ── Button (only when type == Button) ─────────────────────────────────────
    QString targetSlideId;      // id of the Slide this button jumps to (buttonAction == "navigate")
    QString buttonAction = "navigate"; // "navigate" | "changeVariable"

    // ── Variable binding (Button changeVariable action, Checkbox, Slider) ─────
    QString boundVariableId;    // Variable::id
    QString varOp        = "inc"; // Button changeVariable only: "inc" | "dec" | "set" | "toggle"
    double  varOpNumber  = 1.0;   // amount for inc/dec, or the value for "set" on a Number variable
    QString varOpText;            // value for "set" on a Text variable
    bool    varOpBool    = true;  // value for "set" on a Boolean variable

    // ── Slider (only when type == Slider) ──────────────────────────────────────
    double  sliderMin  = 0.0;
    double  sliderMax  = 100.0;
    double  sliderStep = 1.0;

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
    float   opacity        = 1.f;  // 0=fully transparent, 1=fully opaque; animatable via timeline keyframes

    // Timeline animation (entry/exit, loop, click-triggers, variable-gated visibility)
    TimelineTrack timeline;

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

    float   rotation  = 0.f;    // degrees clockwise; applied around element center
    QString shapeText;           // text overlaid inside shape (type==Shape only)

    // ── Mesh gradient fill (Shape elements only) ──────────────────────────────
    bool             useMeshGradient = false; // false → backgroundColor is the fill (unchanged path)
    MeshGradientData meshGradient;             // ignored unless useMeshGradient && isUsable()

    // ── Image/texture fill (Shape elements only) ──────────────────────────────
    // Mutually exclusive with useMeshGradient (mesh takes priority if both set).
    // The image always covers the shape bounds (like CSS background-size:cover);
    // fillOffsetX/Y pan within the resulting slack, fillScale zooms in further.
    bool    useImageFill = false;   // false → backgroundColor/meshGradient path unchanged
    QString fillImagePath;          // absolute path to the source image (Image-element convention)
    float   fillOffsetX  = 0.f;     // pan, -1..1 (0 = centered)
    float   fillOffsetY  = 0.f;     // pan, -1..1 (0 = centered)
    float   fillScale    = 1.f;     // 1.0 = cover with no extra zoom; >1 = zoomed in

    // ── Chart (only when type == Chart) ──────────────────────────────────────
    ChartData chartData;

    // ── Media (Video/Audio) ────────────────────────────────────────────────────
    // content = absolute path to the .mp4/.mp3 file (same convention as Image).
    bool mediaAutoplay     = false; // true = play() when this element's slide becomes the active step; false = user starts it via the native/waveform play control
    bool audioShowWaveform = false; // Audio only: false = compact native player (icon + duration); true = full WhatsApp-style waveform

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

// Persistent ruler guide — a full-slide-height/width line, created by dragging
// out of the horizontal/vertical ruler in the 2D editor. Elements snap to it
// while dragging, same as they snap to slide bounds/other elements.
struct GuideLine {
    bool  vertical = true; // true = vertical line at a given X; false = horizontal line at a given Y
    float pos      = 0.f;  // slide-space coordinate (X if vertical, Y otherwise)
};

// Persistent circular guide ("Zirkel"/compass mode) — center + radius, for
// aligning elements around a circle or reading off radii/distances.
struct GuideCircle {
    float cx     = 0.f;
    float cy     = 0.f;
    float radius = 0.f;
};

struct Slide {
    QString              id;
    QString              name;
    QVector<SlideElement> elements;
    QColor               backgroundColor = Qt::transparent; // transparent = no background
    QVector<GuideLine>   guides;
    QVector<GuideCircle> guideCircles;

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

// A free-floating 3D model in world space, not bound to any slide.
// Only visible/editable in the 3D editor view; never a camera/navigation target.
struct WorldObject {
    QString id;
    QString modelPath; // absolute path to a .gltf/.glb file

    // World transform, same coordinate convention as Slide::posX/Y/Z, rotX/Y/Z
    float posX = 0.f;
    float posY = 0.f;
    float posZ = 0.f;
    float rotX = 0.f; // degrees
    float rotY = 0.f;
    float rotZ = 0.f;
    float scale = 1.f; // multiplies the mesh's auto-normalization factor

    float opacity = 1.f;

    WorldObject() : id(QUuid::createUuid().toString(QUuid::WithoutBraces)) {}
};

class Presentation {
public:
    QVector<Slide> slides;
    QVector<WorldObject> worldObjects;
    QString        filePath;
    QString        exportPath; // last exported/imported folder
    bool           modified = false;

    // Project-wide settings
    QString title                 = "Presentation"; // used as <title> on HTML export
    QColor sceneBackground        = QColor(0x11, 0x11, 0x11);
    float  slideWidth             = 1920.f;
    float  slideHeight            = 1080.f;
    float  defaultInactiveOpacity = 0.3f; // opacity applied to slides with no per-slide override

    VariableSet variables; // user-defined {name} placeholders, see VARIABLEN_PLAN.md

    void addSlide(const QString& name = {});
    void removeSlide(int index);
    void duplicateSlide(int index);
    void moveSlide(int from, int to);
    void removeWorldObjectAt(int index);

    Slide* slideAt(int index) {
        return (index >= 0 && index < slides.size()) ? &slides[index] : nullptr;
    }
    const Slide* slideAt(int index) const {
        return (index >= 0 && index < slides.size()) ? &slides[index] : nullptr;
    }
};
