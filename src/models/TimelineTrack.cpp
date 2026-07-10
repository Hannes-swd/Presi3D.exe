#include "TimelineTrack.h"
#include <QColor>

bool isColorProperty(const QString& key) {
    return key == "color" || key == "backgroundColor" || key == "borderColor";
}

QJsonObject Keyframe::toJson() const {
    QJsonObject o;
    for (auto it = props.constBegin(); it != props.constEnd(); ++it) {
        if (isColorProperty(it.key())) {
            o[it.key()] = it.value().value<QColor>().name(QColor::HexArgb);
        } else {
            o[it.key()] = it.value().toDouble();
        }
    }
    return o;
}

Keyframe Keyframe::fromJson(const QJsonObject& o) {
    Keyframe kf;
    for (auto it = o.constBegin(); it != o.constEnd(); ++it) {
        if (isColorProperty(it.key())) {
            kf.props[it.key()] = QVariant(QColor(it.value().toString()));
        } else {
            kf.props[it.key()] = QVariant(it.value().toDouble());
        }
    }
    return kf;
}

QJsonObject TimelineTrack::toJson() const {
    QJsonObject o;
    o["hasEntry"]      = hasEntry;
    o["entryDelay"]    = entryDelay;
    o["entryDuration"] = entryDuration;
    o["entryTrigger"]  = entryTrigger;
    o["entryStart"]    = entryStart.toJson();

    o["hasExit"]      = hasExit;
    o["exitDelay"]    = exitDelay;
    o["exitDuration"] = exitDuration;
    o["exitTrigger"]  = exitTrigger;
    o["exitEnd"]      = exitEnd.toJson();

    o["loop"]      = loop;
    o["loopPause"] = loopPause;

    o["visibilityVarId"] = visibilityVarId;
    return o;
}

TimelineTrack TimelineTrack::fromJson(const QJsonObject& o) {
    TimelineTrack t;
    t.hasEntry      = o["hasEntry"].toBool(false);
    t.entryDelay    = float(o["entryDelay"].toDouble(0.0));
    t.entryDuration = float(o["entryDuration"].toDouble(0.0));
    t.entryTrigger  = o["entryTrigger"].toString("auto");
    t.entryStart    = Keyframe::fromJson(o["entryStart"].toObject());

    t.hasExit      = o["hasExit"].toBool(false);
    t.exitDelay    = float(o["exitDelay"].toDouble(0.0));
    t.exitDuration = float(o["exitDuration"].toDouble(0.0));
    t.exitTrigger  = o["exitTrigger"].toString("auto");
    t.exitEnd      = Keyframe::fromJson(o["exitEnd"].toObject());

    t.loop      = o["loop"].toBool(false);
    t.loopPause = float(o["loopPause"].toDouble(0.0));

    t.visibilityVarId = o["visibilityVarId"].toString();
    return t;
}
