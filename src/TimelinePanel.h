#pragma once
#include <QWidget>
#include <QVector>
#include <QElapsedTimer>
#include "models/DataModel.h"

class QVBoxLayout;
class QPushButton;
class QToolButton;
class QSlider;
class QLabel;
class QTimer;

// One element's timeline bar: a full-width bar by default (always visible).
// The left half can be shortened to add an entry wait/animation, the right
// half to add an exit wait/animation, split at the middle divider. Dragging
// the entry/exit handles sets the combined (delay+duration) time for that
// side, preserving whatever delay:duration split was last configured via the
// detail dialog (opened from TimelinePanel with the gear button).
class TimelineBarWidget : public QWidget {
    Q_OBJECT
public:
    explicit TimelineBarWidget(QWidget* parent = nullptr);

    // The bar never caches a pointer into Presentation data (that data can be
    // freed/reallocated at any time — e.g. opening a different project, or any
    // edit that reallocates Slide::elements) — it re-resolves the live
    // TimelineTrack by index on every paint/interaction instead, the same
    // safe pattern PropertiesPanel's getElem() uses.
    void bindElement(Presentation* pres, int slideIdx, int elemIdx);
    void invalidate(); // called right before this row is torn down / deleteLater()'d
    QSize sizeHint() const override;

signals:
    void changed(); // entry/exit time dragged
    void detailsRequested(); // "Timeline details…" chosen from the right-click menu

protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;

private:
    // Entry/Exit = dragging the edge handle resizes the occupied (delay+duration)
    // time. EntryShift/ExitShift = dragging the already-occupied segment's body
    // moves it (shifts delay, keeps duration) — lets you stagger several
    // elements' animations to play one after another.
    enum class Handle { None, Entry, Exit, EntryShift, ExitShift };
    TimelineTrack* resolveTrack() const;
    Handle hitTestHandle(const QPointF& pos, const TimelineTrack& t) const;
    float entryOccupiedFrac(const TimelineTrack& t) const; // 0..1 of the left half
    float exitOccupiedFrac(const TimelineTrack& t) const;  // 0..1 of the right half

    Presentation* m_pres     = nullptr;
    int           m_slideIdx = -1;
    int           m_elemIdx  = -1;
    Handle  m_dragging = Handle::None;
    float   m_dragStartX     = 0.f; // widget-space mouse x when the shift-drag started
    float   m_dragStartDelay = 0.f; // entryDelay/exitDelay at that moment

    static constexpr float kBarSeconds = 6.f; // seconds represented by each half of the bar
};

// Per-slide timeline sub-view: one TimelineBarWidget row per element, plus a
// transport row (scrub + play) that drives SlideEditor2D's live preview.
// See ANIMATION_PLAN.md for the feature design.
class TimelinePanel : public QWidget {
    Q_OBJECT
public:
    explicit TimelinePanel(QWidget* parent = nullptr);

    void setSlide(Presentation* pres, int slideIndex);
    void setSelectedElement(int elemIndex);
    void refresh(); // rebuild rows after external data changes (undo/redo, drag in canvas, ...)

signals:
    void timelineModified();
    // isEntry=true -> "Set Start State", false -> "Set End State"
    void keyframeEditRequested(int elemIndex, bool isEntry);
    void previewTimeChanged(float tSeconds); // < 0 = preview off

private slots:
    void onPlayTick();
    void onScrubChanged(int value);
    void onPlayClicked();

private:
    void rebuildRows();
    void openDetailDialog(int elemIndex);

    Presentation* m_pres       = nullptr;
    int           m_slideIdx   = -1;
    int           m_selectedElem = -1;

    QWidget*     m_rowsContainer = nullptr;
    QVBoxLayout* m_rowsLayout    = nullptr;

    QPushButton* m_playBtn = nullptr;
    QSlider*     m_scrub   = nullptr;
    QLabel*      m_timeLabel = nullptr;
    QTimer*      m_playTimer = nullptr;
    bool         m_playing = false;

    // Real-time-accurate playback: each tick recomputes the scrub position
    // from actual elapsed wall time rather than a fixed per-tick step, so the
    // preview's pacing matches the delay/duration seconds shown in the gear
    // dialog and the exported HTML player instead of drifting off real-time.
    QElapsedTimer m_playElapsed;
    int           m_playStartValue = 0; // m_scrub value when playback (re)started

    static constexpr float kTotalSeconds = 12.f; // 2 * TimelineBarWidget::kBarSeconds
    static constexpr int   kScrubSteps   = 1000;
};
