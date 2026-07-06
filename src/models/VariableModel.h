#pragma once
#include <QString>
#include <QVector>
#include <QJsonObject>

// A single user-defined variable. Kept intentionally flat (like SlideElement) —
// no polymorphism, one struct with a type tag.
struct Variable {
    enum Type { Text, Number, Boolean };

    QString id;              // UUID, internal only (never shown to the user)
    QString name;             // used in placeholders, e.g. "preis" -> {preis}
    Type    type = Text;

    QString textValue;
    double  numberValue = 0.0;
    bool    boolValue   = false;

    // Empty = global (whole presentation). Otherwise Slide::id -> only usable/visible on that slide.
    QString scopeSlideId;

    QJsonObject toJson() const;
    static Variable fromJson(const QJsonObject& obj);
};

// All variables of a presentation.
struct VariableSet {
    QVector<Variable> items;

    // Looks up by name, preferring a variable scoped to currentSlideId over a global one.
    const Variable* find(const QString& name, const QString& currentSlideId) const;

    // Looks up by stable id (used by elements that bind to a variable, e.g. Button/Checkbox/Slider).
    const Variable* findById(const QString& id) const;
    Variable*       findById(const QString& id);

    // True if another variable with this name already exists in the same scope.
    bool nameExists(const QString& name, const QString& scopeSlideId, const QString& excludeId = {}) const;

    QJsonObject toJson() const;
    static VariableSet fromJson(const QJsonObject& obj);
};
