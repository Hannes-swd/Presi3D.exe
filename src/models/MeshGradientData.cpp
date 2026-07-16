#include "MeshGradientData.h"
#include <QJsonArray>

QJsonObject MeshGradientData::toJson() const {
    QJsonArray arr;
    for (const auto& p : points) {
        QJsonObject po;
        po["x"] = p.x; po["y"] = p.y;
        po["r"] = p.r; po["g"] = p.g; po["b"] = p.b; po["a"] = p.a;
        arr.append(po);
    }
    QJsonObject o;
    o["points"] = arr;
    return o;
}

MeshGradientData MeshGradientData::fromJson(const QJsonObject& obj) {
    MeshGradientData d;
    for (const auto& v : obj["points"].toArray()) {
        QJsonObject po = v.toObject();
        MeshGradientPoint p;
        p.x = po["x"].toDouble(0.5); p.y = po["y"].toDouble(0.5);
        p.r = po["r"].toInt(255);   p.g = po["g"].toInt(255);   p.b = po["b"].toInt(255);
        p.a = po["a"].toDouble(1.0);
        d.points << p;
    }
    return d;
}
