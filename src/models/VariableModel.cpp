#include "VariableModel.h"
#include <QJsonArray>

QJsonObject Variable::toJson() const {
    QJsonObject o;
    o["id"]            = id;
    o["name"]           = name;
    o["type"]           = int(type);
    o["textValue"]      = textValue;
    o["numberValue"]    = numberValue;
    o["boolValue"]      = boolValue;
    o["scopeSlideId"]   = scopeSlideId;
    return o;
}

Variable Variable::fromJson(const QJsonObject& obj) {
    Variable v;
    v.id            = obj["id"].toString();
    v.name          = obj["name"].toString();
    v.type          = Variable::Type(obj["type"].toInt(int(Variable::Text)));
    v.textValue     = obj["textValue"].toString();
    v.numberValue   = obj["numberValue"].toDouble();
    v.boolValue     = obj["boolValue"].toBool();
    v.scopeSlideId  = obj["scopeSlideId"].toString();
    return v;
}

const Variable* VariableSet::find(const QString& name, const QString& currentSlideId) const {
    const Variable* localMatch = nullptr;
    const Variable* globalMatch = nullptr;
    for (const Variable& v : items) {
        if (v.name.compare(name, Qt::CaseInsensitive) != 0) continue;
        if (!v.scopeSlideId.isEmpty() && v.scopeSlideId == currentSlideId)
            localMatch = &v;
        else if (v.scopeSlideId.isEmpty())
            globalMatch = &v;
    }
    return localMatch ? localMatch : globalMatch;
}

const Variable* VariableSet::findById(const QString& id) const {
    for (const Variable& v : items)
        if (v.id == id) return &v;
    return nullptr;
}

Variable* VariableSet::findById(const QString& id) {
    for (Variable& v : items)
        if (v.id == id) return &v;
    return nullptr;
}

bool VariableSet::nameExists(const QString& name, const QString& scopeSlideId, const QString& excludeId) const {
    for (const Variable& v : items) {
        if (!excludeId.isEmpty() && v.id == excludeId) continue;
        if (v.scopeSlideId != scopeSlideId) continue;
        if (v.name.compare(name, Qt::CaseInsensitive) == 0) return true;
    }
    return false;
}

QJsonObject VariableSet::toJson() const {
    QJsonArray arr;
    for (const Variable& v : items) arr.append(v.toJson());
    QJsonObject o;
    o["items"] = arr;
    return o;
}

VariableSet VariableSet::fromJson(const QJsonObject& obj) {
    VariableSet set;
    for (const auto& v : obj["items"].toArray())
        set.items << Variable::fromJson(v.toObject());
    return set;
}
