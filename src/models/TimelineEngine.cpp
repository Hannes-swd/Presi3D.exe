#include "TimelineEngine.h"
#include <algorithm>
#include <cmath>

namespace {

double basePropValue(const SlideElement& e, const QString& key) {
    if (key == "x") return e.x;
    if (key == "y") return e.y;
    if (key == "width") return e.width;
    if (key == "height") return e.height;
    if (key == "rotation") return e.rotation;
    if (key == "opacity") return e.opacity;
    if (key == "fontSize") return e.fontSize;
    if (key == "cornerRadius") return e.cornerRadius;
    if (key == "borderWidth") return e.borderWidth;
    return 0.0;
}

void setPropValue(SlideElement& e, const QString& key, double v) {
    if (key == "x") e.x = float(v);
    else if (key == "y") e.y = float(v);
    else if (key == "width") e.width = float(v);
    else if (key == "height") e.height = float(v);
    else if (key == "rotation") e.rotation = float(v);
    else if (key == "opacity") e.opacity = float(v);
    else if (key == "fontSize") e.fontSize = int(std::lround(v));
    else if (key == "cornerRadius") e.cornerRadius = float(v);
    else if (key == "borderWidth") e.borderWidth = float(v);
}

QColor baseColorValue(const SlideElement& e, const QString& key) {
    if (key == "color") return e.color;
    if (key == "backgroundColor") return e.backgroundColor;
    if (key == "borderColor") return e.borderColor;
    return QColor();
}

void setColorValue(SlideElement& e, const QString& key, const QColor& c) {
    if (key == "color") e.color = c;
    else if (key == "backgroundColor") e.backgroundColor = c;
    else if (key == "borderColor") e.borderColor = c;
}

QColor lerpColor(const QColor& a, const QColor& b, double t) {
    return QColor::fromRgbF(
        float(a.redF()   + (b.redF()   - a.redF())   * t),
        float(a.greenF() + (b.greenF() - a.greenF()) * t),
        float(a.blueF()  + (b.blueF()  - a.blueF())  * t),
        float(a.alphaF() + (b.alphaF() - a.alphaF()) * t)
    );
}

} // namespace

QJsonObject TimelineEngine::baseSnapshot(const SlideElement& e, const QStringList& keys) {
    QJsonObject o;
    for (const QString& key : keys) {
        if (isColorProperty(key))
            o[key] = baseColorValue(e, key).name(QColor::HexArgb);
        else
            o[key] = basePropValue(e, key);
    }
    return o;
}

SlideElement TimelineEngine::previewAt(const SlideElement& base, const TimelineTrack& track, float tSeconds) {
    SlideElement cur = base;
    if (track.hasEntry) {
        float p;
        if (tSeconds < track.entryDelay) p = 0.f;
        else if (track.entryDuration <= 0.f) p = 1.f;
        else p = std::clamp((tSeconds - track.entryDelay) / track.entryDuration, 0.f, 1.f);
        cur = interpolate(base, track.entryStart, p);
    }
    if (track.hasExit && tSeconds >= track.exitDelay) {
        float p;
        if (track.exitDuration <= 0.f) p = 1.f;
        else p = std::clamp((tSeconds - track.exitDelay) / track.exitDuration, 0.f, 1.f);
        cur = interpolate(base, track.exitEnd, 1.f - p);
    }
    return cur;
}

SlideElement TimelineEngine::interpolate(const SlideElement& base, const Keyframe& kf, float t) {
    t = std::clamp(t, 0.f, 1.f);
    SlideElement out = base;
    for (auto it = kf.props.constBegin(); it != kf.props.constEnd(); ++it) {
        const QString& key = it.key();
        if (isColorProperty(key)) {
            QColor overrideColor = it.value().value<QColor>();
            QColor baseColor     = baseColorValue(base, key);
            setColorValue(out, key, lerpColor(overrideColor, baseColor, double(t)));
        } else {
            double overrideVal = it.value().toDouble();
            double baseVal     = basePropValue(base, key);
            setPropValue(out, key, overrideVal + (baseVal - overrideVal) * double(t));
        }
    }
    return out;
}
