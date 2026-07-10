#pragma once
#include <QString>
#include <QMap>
#include <QVariant>
#include <QJsonObject>

// Sparse property overrides used as the "start" (entry) or "end" (exit) state
// of a keyframe animation. Only properties the user actually changed while
// editing the keyframe are present here; everything else keeps the base
// SlideElement's value for the whole animation. This is what makes "any
// property can differ" work generically instead of a fixed animation list.
struct Keyframe {
    QMap<QString, QVariant> props; // key -> double, or QColor for color properties (see isColorProperty)

    bool isEmpty() const { return props.isEmpty(); }

    QJsonObject toJson() const;
    static Keyframe fromJson(const QJsonObject& o);
};

// True for SlideElement property names that are colors (QColor) rather than
// plain numbers. Shared by Keyframe JSON (de)serialization and
// TimelineEngine::interpolate so both agree on how to read/write a key.
bool isColorProperty(const QString& key);

// Per-element timeline track: entry (appear) + exit (disappear) behaviour,
// optional looping, and optional bool-variable-gated visibility.
// See ANIMATION_PLAN.md for the design rationale. Default-constructed means
// "always visible, no animation" — identical to having no timeline at all.
struct TimelineTrack {
    // ── Entry (left half of the timeline bar) ──────────────────────────────
    bool    hasEntry      = false;
    float   entryDelay    = 0.f;      // seconds of pure wait before the entry animation starts
    float   entryDuration = 0.f;      // seconds of animation; 0 = instant appear after the wait
    QString entryTrigger  = "auto";   // "auto" | "click"
    Keyframe entryStart;              // property overrides at t=0 of entry (interpolates -> base)

    // ── Exit (right half of the timeline bar) ──────────────────────────────
    bool    hasExit      = false;
    float   exitDelay    = 0.f;       // seconds after stepenter before the exit animation starts (auto only)
    float   exitDuration = 0.f;
    QString exitTrigger  = "auto";    // "auto" | "click"
    Keyframe exitEnd;                 // property overrides at end of exit (interpolates base -> this)

    // ── Loop ────────────────────────────────────────────────────────────────
    bool  loop      = false;
    float loopPause = 0.f;            // wait between loop iterations

    // ── Conditional visibility ──────────────────────────────────────────────
    QString visibilityVarId;          // Variable::id of a Boolean var; empty = no gating

    bool isDefault() const {
        return !hasEntry && !hasExit && !loop && visibilityVarId.isEmpty();
    }

    QJsonObject toJson() const;
    static TimelineTrack fromJson(const QJsonObject& o);
};
