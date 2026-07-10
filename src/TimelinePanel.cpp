#include "TimelinePanel.h"
#include "models/VariableModel.h"
#include <QPainter>
#include <QMouseEvent>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QPushButton>
#include <QToolButton>
#include <QSlider>
#include <QLabel>
#include <QTimer>
#include <QScrollArea>
#include <QDialog>
#include <QGroupBox>
#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QMenu>
#include <QPointer>
#include <algorithm>
#include <cmath>
#include <cstdio>

// ── TimelineBarWidget ─────────────────────────────────────────────────────────

TimelineBarWidget::TimelineBarWidget(QWidget* parent) : QWidget(parent) {
    setMinimumHeight(40);
    setMinimumWidth(160);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setMouseTracking(true);
    setToolTip("Right-click: add/remove entrance or exit animation, loop, details…\n"
               "Left-click the wait area: quick-add. Drag the edge: resize. Drag the animated part: shift its timing.");
}

void TimelineBarWidget::bindElement(Presentation* pres, int slideIdx, int elemIdx) {
    m_pres     = pres;
    m_slideIdx = slideIdx;
    m_elemIdx  = elemIdx;
    update();
}

void TimelineBarWidget::invalidate() {
    m_pres = nullptr;
}

TimelineTrack* TimelineBarWidget::resolveTrack() const {
    if (!m_pres) return nullptr;
    Slide* s = m_pres->slideAt(m_slideIdx);
    if (!s || m_elemIdx < 0 || m_elemIdx >= s->elements.size()) return nullptr;
    return &s->elements[m_elemIdx].timeline;
}

QSize TimelineBarWidget::sizeHint() const { return QSize(320, 40); }

float TimelineBarWidget::entryOccupiedFrac(const TimelineTrack& t) const {
    if (!t.hasEntry) return 0.f;
    return std::clamp((t.entryDelay + t.entryDuration) / kBarSeconds, 0.f, 1.f);
}
float TimelineBarWidget::exitOccupiedFrac(const TimelineTrack& t) const {
    if (!t.hasExit) return 0.f;
    return std::clamp((t.exitDelay + t.exitDuration) / kBarSeconds, 0.f, 1.f);
}

void TimelineBarWidget::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    QRectF r = QRectF(rect()).adjusted(1, 6, -1, -6);
    float halfW = float(r.width()) / 2.f;

    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0xbf, 0xdb, 0xfe)); // light blue — "always visible" (no animation)
    p.drawRoundedRect(r, 4, 4);

    TimelineTrack* track = resolveTrack();
    if (!track) return;
    const TimelineTrack& t = *track;

    auto drawSide = [&](bool fromLeft, float delay, float duration, bool has, const QString& trigger) {
        if (!has) return;
        float occTotal = std::clamp((delay + duration) / kBarSeconds, 0.f, 1.f) * halfW;
        if (occTotal <= 0.f) return;
        float durFrac = (delay + duration) > 0.f ? duration / (delay + duration) : 0.f;
        float durW = occTotal * durFrac;
        float waitW = occTotal - durW;

        QRectF waitRect, animRect;
        if (fromLeft) {
            waitRect = QRectF(r.left(), r.top(), waitW, r.height());
            animRect = QRectF(r.left() + waitW, r.top(), durW, r.height());
        } else {
            waitRect = QRectF(r.right() - waitW, r.top(), waitW, r.height());
            animRect = QRectF(r.right() - waitW - durW, r.top(), durW, r.height());
        }
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(0xe5, 0xe7, 0xeb)); // wait segment: light grey
        p.drawRect(waitRect);
        p.setPen(QPen(QColor(0x9c, 0xa3, 0xaf), 1)); // hatch lines: medium grey, still visible on light fill
        for (float x = float(waitRect.left()); x < waitRect.right(); x += 6.f)
            p.drawLine(QPointF(x, waitRect.top()), QPointF(x + waitRect.height(), waitRect.bottom()));
        p.setPen(Qt::NoPen);
        p.setBrush(fromLeft ? QColor(0x86, 0xef, 0xac)   // entry animated segment: light green
                             : QColor(0xfd, 0xba, 0x74)); // exit animated segment: light orange
        p.drawRect(animRect);

        if (trigger == "click") {
            p.setBrush(QColor(0x25, 0x63, 0xeb)); // app accent blue — stays visible against the light fills above
            QPointF c = fromLeft ? animRect.topRight() : animRect.topLeft();
            p.drawEllipse(c, 3, 3);
        }
    };

    drawSide(true,  t.entryDelay, t.entryDuration, t.hasEntry, t.entryTrigger);
    drawSide(false, t.exitDelay,  t.exitDuration,  t.hasExit,  t.exitTrigger);

    p.setPen(QPen(QColor(0x9c, 0xa3, 0xaf), 1, Qt::DashLine)); // entry/exit divider: medium grey
    p.drawLine(QPointF(r.center().x(), r.top() - 3), QPointF(r.center().x(), r.bottom() + 3));

    if (t.loop) {
        p.setPen(QPen(QColor(0xd9, 0x77, 0x06), 2)); // loop outline: amber, still reads on a light fill
        p.setBrush(Qt::NoBrush);
        p.drawRoundedRect(r.adjusted(0.5, 0.5, -0.5, -0.5), 4, 4);
    }
}

TimelineBarWidget::Handle TimelineBarWidget::hitTestHandle(const QPointF& pos, const TimelineTrack& t) const {
    QRectF r = QRectF(rect()).adjusted(1, 6, -1, -6);
    float halfW = float(r.width()) / 2.f;
    float entryX = float(r.left()) + entryOccupiedFrac(t) * halfW;
    float exitX  = float(r.right()) - exitOccupiedFrac(t) * halfW;
    const float tol = 8.f;
    if (std::abs(float(pos.x()) - entryX) <= tol) return Handle::Entry;
    if (std::abs(float(pos.x()) - exitX)  <= tol) return Handle::Exit;
    // Inside the already-occupied segment (but not on its edge handle): drag
    // to shift the whole segment (move its delay, keep its duration).
    if (t.hasEntry && pos.x() > r.left() && pos.x() < entryX) return Handle::EntryShift;
    if (t.hasExit  && pos.x() < r.right() && pos.x() > exitX) return Handle::ExitShift;
    return Handle::None;
}

void TimelineBarWidget::mousePressEvent(QMouseEvent* ev) {
    TimelineTrack* track = resolveTrack();
    if (!track) return;

    // Right-click: a proper context menu to add/remove the entrance and exit
    // animation, toggle looping, or jump to the full delay/duration/trigger
    // dialog — the discoverable alternative to guessing that an empty part of
    // the bar is clickable (that shortcut still works too, see below).
    if (ev->button() == Qt::RightButton) {
        QMenu menu(this);
        QAction* entryAct = menu.addAction(track->hasEntry ? "Remove entrance animation"
                                                             : "Add entrance animation…");
        QAction* exitAct  = menu.addAction(track->hasExit  ? "Remove exit animation"
                                                             : "Add exit animation…");
        menu.addSeparator();
        QAction* loopAct = menu.addAction(track->loop ? "Remove loop" : "Make it loop");
        menu.addSeparator();
        QAction* detailsAct = menu.addAction("Timeline details…");

        QAction* chosen = menu.exec(ev->globalPosition().toPoint());

        enum class Choice { None, Entry, Exit, Loop, Details };
        Choice choice = Choice::None;
        if      (chosen == entryAct)   choice = Choice::Entry;
        else if (chosen == exitAct)    choice = Choice::Exit;
        else if (chosen == loopAct)    choice = Choice::Loop;
        else if (chosen == detailsAct) choice = Choice::Details;
        if (choice == Choice::None) return;

        // Any of these choices can end up rebuilding this row's own widget
        // tree (via changed()/detailsRequested() -> ... ->
        // TimelinePanel::rebuildRows(), which deleteLater()s this bar's row)
        // and doing that synchronously here — still on the call stack of
        // QMenu::exec()'s nested event loop — corrupted the heap in practice
        // (this crashed with a debug heap assertion). Deferring to the next
        // event loop tick means rebuildRows() runs with a clean stack, no
        // nested QMenu loop still unwinding underneath it. QPointer guards
        // against `this` already being torn down for some unrelated reason
        // by the time the deferred call actually runs.
        QPointer<TimelineBarWidget> self(this);
        QTimer::singleShot(0, this, [self, choice]() {
            if (!self) return;
            TimelineTrack* liveTrack = self->resolveTrack();
            if (!liveTrack) return;
            switch (choice) {
            case Choice::Entry:
                liveTrack->hasEntry = !liveTrack->hasEntry;
                if (liveTrack->hasEntry && liveTrack->entryDelay <= 0.f && liveTrack->entryDuration <= 0.f)
                    liveTrack->entryDelay = kBarSeconds * 0.25f; // default wait, so the bar visibly shows something
                emit self->changed();
                break;
            case Choice::Exit:
                liveTrack->hasExit = !liveTrack->hasExit;
                if (liveTrack->hasExit && liveTrack->exitDelay <= 0.f && liveTrack->exitDuration <= 0.f)
                    liveTrack->exitDelay = kBarSeconds * 0.25f;
                emit self->changed();
                break;
            case Choice::Loop:
                liveTrack->loop = !liveTrack->loop;
                emit self->changed();
                break;
            case Choice::Details:
                emit self->detailsRequested();
                break;
            default:
                break;
            }
        });
        return;
    }
    if (ev->button() != Qt::LeftButton) return;

    m_dragging = hitTestHandle(ev->position(), *track);
    m_dragStartX = float(ev->position().x());
    if (m_dragging == Handle::EntryShift) {
        m_dragStartDelay = track->entryDelay;
    } else if (m_dragging == Handle::ExitShift) {
        m_dragStartDelay = track->exitDelay;
    } else if (m_dragging == Handle::None) {
        // Click on the empty (not-yet-occupied) part of a half: activate it
        // with a default wait, so the feature is discoverable without the
        // gear dialog. (Deactivating again is a right-click, see above.)
        QRectF r = QRectF(rect()).adjusted(1, 6, -1, -6);
        bool leftHalf = ev->position().x() < r.center().x();
        if (leftHalf && !track->hasEntry) {
            track->hasEntry   = true;
            track->entryDelay = kBarSeconds * 0.25f;
            emit changed();
            update();
        } else if (!leftHalf && !track->hasExit) {
            track->hasExit   = true;
            track->exitDelay = kBarSeconds * 0.25f;
            emit changed();
            update();
        }
    }
}

void TimelineBarWidget::mouseMoveEvent(QMouseEvent* ev) {
    if (m_dragging == Handle::None) return;
    TimelineTrack* track = resolveTrack();
    if (!track) { m_dragging = Handle::None; return; }
    QRectF r = QRectF(rect()).adjusted(1, 6, -1, -6);
    float halfW = float(r.width()) / 2.f;

    if (m_dragging == Handle::Entry) {
        float occ = std::clamp(float(ev->position().x() - r.left()) / halfW, 0.f, 1.f);
        float total = occ * kBarSeconds;
        float oldTotal = track->entryDelay + track->entryDuration;
        float ratio = oldTotal > 0.f ? track->entryDuration / oldTotal : 0.f;
        track->entryDuration = total * ratio;
        track->entryDelay    = total - track->entryDuration;
        track->hasEntry      = total > 0.001f;
    } else if (m_dragging == Handle::Exit) {
        float occ = std::clamp(float(r.right() - ev->position().x()) / halfW, 0.f, 1.f);
        float total = occ * kBarSeconds;
        float oldTotal = track->exitDelay + track->exitDuration;
        float ratio = oldTotal > 0.f ? track->exitDuration / oldTotal : 0.f;
        track->exitDuration = total * ratio;
        track->exitDelay    = total - track->exitDuration;
        track->hasExit      = total > 0.001f;
    } else if (m_dragging == Handle::EntryShift) {
        float deltaSeconds = (float(ev->position().x()) - m_dragStartX) / halfW * kBarSeconds;
        float maxDelay = qMax(0.f, kBarSeconds - track->entryDuration);
        track->entryDelay = std::clamp(m_dragStartDelay + deltaSeconds, 0.f, maxDelay);
    } else if (m_dragging == Handle::ExitShift) {
        float deltaSeconds = (m_dragStartX - float(ev->position().x())) / halfW * kBarSeconds;
        float maxDelay = qMax(0.f, kBarSeconds - track->exitDuration);
        track->exitDelay = std::clamp(m_dragStartDelay + deltaSeconds, 0.f, maxDelay);
    }
    update();
    emit changed();
}

void TimelineBarWidget::mouseReleaseEvent(QMouseEvent*) {
    m_dragging = Handle::None;
}

// ── TimelinePanel ─────────────────────────────────────────────────────────────

static SlideElement* tpGetElem(Presentation* pres, int slideIdx, int elemIdx) {
    Slide* s = pres ? pres->slideAt(slideIdx) : nullptr;
    if (!s || elemIdx < 0 || elemIdx >= s->elements.size()) return nullptr;
    return &s->elements[elemIdx];
}

TimelinePanel::TimelinePanel(QWidget* parent) : QWidget(parent) {
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(4, 4, 4, 4);
    outer->setSpacing(4);

    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    m_rowsContainer = new QWidget(scroll);
    m_rowsLayout = new QVBoxLayout(m_rowsContainer);
    m_rowsLayout->setContentsMargins(0, 0, 0, 0);
    m_rowsLayout->setSpacing(2);
    m_rowsLayout->addStretch(1);
    scroll->setWidget(m_rowsContainer);
    outer->addWidget(scroll, 1);

    auto* transport = new QHBoxLayout;
    m_playBtn = new QPushButton("▶", this); // ▶
    m_playBtn->setFixedWidth(32);
    m_scrub = new QSlider(Qt::Horizontal, this);
    m_scrub->setRange(0, kScrubSteps);
    m_timeLabel = new QLabel("0.0s", this);
    m_timeLabel->setFixedWidth(48);
    transport->addWidget(m_playBtn);
    transport->addWidget(m_scrub, 1);
    transport->addWidget(m_timeLabel);
    outer->addLayout(transport);

    m_playTimer = new QTimer(this);
    m_playTimer->setInterval(33);
    connect(m_playTimer, &QTimer::timeout, this, &TimelinePanel::onPlayTick);
    connect(m_playBtn, &QPushButton::clicked, this, &TimelinePanel::onPlayClicked);
    connect(m_scrub, &QSlider::valueChanged, this, &TimelinePanel::onScrubChanged);
}

void TimelinePanel::setSlide(Presentation* pres, int slideIndex) {
    m_pres = pres;
    m_slideIdx = slideIndex;
    m_selectedElem = -1;
    m_scrub->setValue(0);
    rebuildRows();
}

void TimelinePanel::setSelectedElement(int elemIndex) {
    m_selectedElem = elemIndex;
    rebuildRows();
}

void TimelinePanel::refresh() { rebuildRows(); }

void TimelinePanel::rebuildRows() {
    QLayoutItem* item;
    while ((item = m_rowsLayout->takeAt(0)) != nullptr) {
        if (QWidget* w = item->widget()) {
            // Invalidate immediately: deleteLater() only destroys the widget on
            // the next event loop turn, and it must not touch the Presentation
            // (which may be replaced/freed right after this, e.g. when opening
            // a different project) in the meantime.
            if (auto* bar = w->findChild<TimelineBarWidget*>())
                bar->invalidate();
            w->deleteLater();
        }
        delete item;
    }

    Slide* slide = m_pres ? m_pres->slideAt(m_slideIdx) : nullptr;
    m_rowsLayout->addStretch(1); // trailing stretch every row gets inserted before
    if (!slide) return;

    static const char* typeNames[] = {"Text", "Shape", "Image", "Table", "Chart",
                                       "Formula", "iFrame", "Button", "Checkbox", "Slider"};

    for (int i = 0; i < slide->elements.size(); ++i) {
        SlideElement& e = slide->elements[i];
        auto* row = new QWidget(m_rowsContainer);
        auto* hl  = new QHBoxLayout(row);
        hl->setContentsMargins(2, 2, 2, 2);
        hl->setSpacing(4);
        row->setStyleSheet(i == m_selectedElem
            ? "background: rgba(90,140,255,60); color:#111827;"
            : "background: transparent; color:#111827;");

        QString shortId = e.id.left(6);
        QString labelTxt = QString("%1 %2").arg(typeNames[e.type], shortId);
        auto* label = new QLabel(labelTxt, row);
        label->setFixedWidth(90);
        label->setStyleSheet("font-size:10px; color:#111827; background:transparent;");
        hl->addWidget(label);

        auto* bar = new TimelineBarWidget(row);
        bar->bindElement(m_pres, m_slideIdx, i);
        hl->addWidget(bar, 1);

        auto* gearBtn = new QToolButton(row);
        gearBtn->setText("⚙"); // ⚙
        gearBtn->setToolTip("Timeline details: delay/duration/trigger, loop, variable-gated visibility");
        hl->addWidget(gearBtn);

        auto* startBtn = new QPushButton("Start▸", row); // Start▸
        startBtn->setToolTip("Drag this element on the canvas to set its entry start state");
        startBtn->setFixedHeight(22);
        hl->addWidget(startBtn);

        auto* endBtn = new QPushButton("◂End", row); // ◂End
        endBtn->setToolTip("Drag this element on the canvas to set its exit end state");
        endBtn->setFixedHeight(22);
        hl->addWidget(endBtn);

        connect(bar, &TimelineBarWidget::changed, this, [this]() { emit timelineModified(); });
        connect(bar, &TimelineBarWidget::detailsRequested, this, [this, i]() { openDetailDialog(i); });
        connect(gearBtn, &QToolButton::clicked, this, [this, i]() { openDetailDialog(i); });
        connect(startBtn, &QPushButton::clicked, this, [this, i]() { emit keyframeEditRequested(i, true); });
        connect(endBtn, &QPushButton::clicked, this, [this, i]() { emit keyframeEditRequested(i, false); });

        m_rowsLayout->insertWidget(m_rowsLayout->count() - 1, row);
    }
}

void TimelinePanel::openDetailDialog(int elemIndex) {
    SlideElement* e = tpGetElem(m_pres, m_slideIdx, elemIndex);
    if (!e) return;
    TimelineTrack& track = e->timeline;

    QDialog dlg(this);
    dlg.setWindowTitle("Timeline: " + e->id.left(8));
    auto* vbox = new QVBoxLayout(&dlg);

    // ── Entry ──────────────────────────────────────────────────────────────
    auto* entryGroup = new QGroupBox("Entry (appear)", &dlg);
    auto* entryForm = new QFormLayout(entryGroup);
    auto* entryEnabled = new QCheckBox("Enabled", entryGroup);
    entryEnabled->setChecked(track.hasEntry);
    auto* entryDelay = new QDoubleSpinBox(entryGroup);
    entryDelay->setRange(0.0, 60.0); entryDelay->setDecimals(2); entryDelay->setValue(track.entryDelay);
    auto* entryDuration = new QDoubleSpinBox(entryGroup);
    entryDuration->setRange(0.0, 60.0); entryDuration->setDecimals(2); entryDuration->setValue(track.entryDuration);
    auto* entryTrigger = new QComboBox(entryGroup);
    entryTrigger->addItem("Automatic", "auto");
    entryTrigger->addItem("On click", "click");
    entryTrigger->setCurrentIndex(track.entryTrigger == "click" ? 1 : 0);
    entryTrigger->setToolTip("\"On click\" only gates the FIRST time this slide is shown per visit.\n"
                              "If Loop is also enabled, repeat iterations play automatically after\n"
                              "the wait time instead of requiring another click each time.");
    entryForm->addRow(entryEnabled);
    entryForm->addRow("Wait before (s):", entryDelay);
    entryForm->addRow("Animation (s):", entryDuration);
    entryForm->addRow("Trigger:", entryTrigger);
    vbox->addWidget(entryGroup);

    // ── Exit ───────────────────────────────────────────────────────────────
    auto* exitGroup = new QGroupBox("Exit (disappear)", &dlg);
    auto* exitForm = new QFormLayout(exitGroup);
    auto* exitEnabled = new QCheckBox("Enabled", exitGroup);
    exitEnabled->setChecked(track.hasExit);
    auto* exitDelay = new QDoubleSpinBox(exitGroup);
    exitDelay->setRange(0.0, 60.0); exitDelay->setDecimals(2); exitDelay->setValue(track.exitDelay);
    auto* exitDuration = new QDoubleSpinBox(exitGroup);
    exitDuration->setRange(0.0, 60.0); exitDuration->setDecimals(2); exitDuration->setValue(track.exitDuration);
    auto* exitTrigger = new QComboBox(exitGroup);
    exitTrigger->addItem("Automatic", "auto");
    exitTrigger->addItem("On click", "click");
    exitTrigger->setCurrentIndex(track.exitTrigger == "click" ? 1 : 0);
    exitTrigger->setToolTip("\"On click\" only gates the FIRST time this slide is shown per visit.\n"
                             "If Loop is also enabled, repeat iterations play automatically after\n"
                             "the wait time instead of requiring another click each time.");
    exitForm->addRow(exitEnabled);
    exitForm->addRow("Wait before (s):", exitDelay);
    exitForm->addRow("Animation (s):", exitDuration);
    exitForm->addRow("Trigger:", exitTrigger);
    vbox->addWidget(exitGroup);

    // ── Loop ───────────────────────────────────────────────────────────────
    auto* loopGroup = new QGroupBox("Loop", &dlg);
    auto* loopForm = new QFormLayout(loopGroup);
    auto* loopEnabled = new QCheckBox("Repeat while slide is active", loopGroup);
    loopEnabled->setChecked(track.loop);
    auto* loopPause = new QDoubleSpinBox(loopGroup);
    loopPause->setRange(0.0, 60.0); loopPause->setDecimals(2); loopPause->setValue(track.loopPause);
    loopForm->addRow(loopEnabled);
    loopForm->addRow("Pause between loops (s):", loopPause);
    vbox->addWidget(loopGroup);

    // ── Conditional visibility ───────────────────────────────────────────────
    auto* visGroup = new QGroupBox("Conditional Visibility", &dlg);
    auto* visForm = new QFormLayout(visGroup);
    auto* visCombo = new QComboBox(visGroup);
    visCombo->addItem("(always visible)", QString());
    const Slide* curSlide = m_pres->slideAt(m_slideIdx);
    QString curSlideId = curSlide ? curSlide->id : QString();
    int visCurrentIdx = 0;
    if (m_pres) {
        for (const Variable& v : m_pres->variables.items) {
            if (v.type != Variable::Boolean) continue;
            if (!v.scopeSlideId.isEmpty() && v.scopeSlideId != curSlideId) continue;
            visCombo->addItem(v.name, v.id);
            if (v.id == track.visibilityVarId) visCurrentIdx = visCombo->count() - 1;
        }
    }
    visCombo->setCurrentIndex(visCurrentIdx);
    visForm->addRow("Show only when:", visCombo);
    vbox->addWidget(visGroup);

    bool changedAny = false;
    auto apply = [&, this, elemIndex]() {
        SlideElement* live = tpGetElem(m_pres, m_slideIdx, elemIndex);
        if (!live) return;
        TimelineTrack& t = live->timeline;
        t.hasEntry      = entryEnabled->isChecked();
        t.entryDelay    = float(entryDelay->value());
        t.entryDuration = float(entryDuration->value());
        t.entryTrigger  = entryTrigger->currentData().toString();
        t.hasExit       = exitEnabled->isChecked();
        t.exitDelay     = float(exitDelay->value());
        t.exitDuration  = float(exitDuration->value());
        t.exitTrigger   = exitTrigger->currentData().toString();
        t.loop          = loopEnabled->isChecked();
        t.loopPause     = float(loopPause->value());
        t.visibilityVarId = visCombo->currentData().toString();
        changedAny = true;
        rebuildRows();
        emit timelineModified();
    };

    connect(entryEnabled, &QCheckBox::toggled, this, apply);
    connect(entryDelay, &QDoubleSpinBox::valueChanged, this, apply);
    connect(entryDuration, &QDoubleSpinBox::valueChanged, this, apply);
    connect(entryTrigger, &QComboBox::currentIndexChanged, this, apply);
    connect(exitEnabled, &QCheckBox::toggled, this, apply);
    connect(exitDelay, &QDoubleSpinBox::valueChanged, this, apply);
    connect(exitDuration, &QDoubleSpinBox::valueChanged, this, apply);
    connect(exitTrigger, &QComboBox::currentIndexChanged, this, apply);
    connect(loopEnabled, &QCheckBox::toggled, this, apply);
    connect(loopPause, &QDoubleSpinBox::valueChanged, this, apply);
    connect(visCombo, &QComboBox::currentIndexChanged, this, apply);

    auto* closeBtn = new QPushButton("Close", &dlg);
    connect(closeBtn, &QPushButton::clicked, &dlg, &QDialog::accept);
    vbox->addWidget(closeBtn);

    dlg.exec();
}

void TimelinePanel::onPlayClicked() {
    m_playing = !m_playing;
    m_playBtn->setText(m_playing ? "⏸" : "▶"); // ⏸ / ▶
    if (m_playing) {
        if (m_scrub->value() >= m_scrub->maximum()) m_scrub->setValue(0);
        m_playStartValue = m_scrub->value();
        m_playElapsed.start();
        m_playTimer->start();
    } else {
        m_playTimer->stop();
    }
}

void TimelinePanel::onPlayTick() {
    // Derive the scrub position from actual elapsed wall time (rather than a
    // fixed step per 33ms tick) so playback runs at real-time speed and the
    // preview's delay/duration pacing matches what the gear dialog and the
    // exported HTML player will actually do — a fixed-step tick loses time to
    // integer truncation and drifts noticeably slow over a few seconds.
    float startSeconds = (float(m_playStartValue) / float(kScrubSteps)) * kTotalSeconds;
    float curSeconds    = startSeconds + float(m_playElapsed.elapsed()) / 1000.f;
    int v = int((curSeconds / kTotalSeconds) * float(kScrubSteps));
    if (v >= m_scrub->maximum()) {
        v = m_scrub->maximum();
        m_playing = false;
        m_playBtn->setText("▶");
        m_playTimer->stop();
    }
    m_scrub->setValue(v);
}

void TimelinePanel::onScrubChanged(int value) {
    float t = (float(value) / float(kScrubSteps)) * kTotalSeconds;
    m_timeLabel->setText(QString::number(double(t), 'f', 1) + "s");
    emit previewTimeChanged(t);
}
