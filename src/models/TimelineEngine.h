#pragma once
#include "models/DataModel.h"
#include "models/TimelineTrack.h"
#include <QStringList>
#include <QJsonObject>

// Shared interpolation logic for keyframe-based timeline animation.
// This C++ implementation drives the live editor preview (SlideEditor2D
// scrubbing); HtmlExporter hand-ports the same formula to JS for the
// exported presentation, matching how VariableEngine's evaluator is
// duplicated into JS elsewhere in this codebase (kept in sync manually).
namespace TimelineEngine {

// Returns `base` with every property present in `kf.props` linearly
// interpolated between its override value (t=0) and the base value (t=1).
// Properties not present in kf.props are left untouched. `t` is clamped to [0,1].
// Callers animating "base -> override" (exit) simply pass `1 - t`.
SlideElement interpolate(const SlideElement& base, const Keyframe& kf, float t);

// Returns a JSON object with the *base* (non-animated) values of exactly the
// given property keys. Used by HtmlExporter to embed base values alongside
// the keyframe overrides in data-timeline, so the exported JS player can
// interpolate without having to re-derive base values from rendered CSS.
QJsonObject baseSnapshot(const SlideElement& e, const QStringList& keys);

// Computes the element's appearance at `tSeconds` since the slide became
// active, for the in-editor scrub/play preview (TimelinePanel + SlideEditor2D).
// A simplified single-pass version of the exported JS TimelinePlayer's
// scheduling: click-triggers and loop iterations are treated as if automatic.
SlideElement previewAt(const SlideElement& base, const TimelineTrack& track, float tSeconds);

} // namespace TimelineEngine
