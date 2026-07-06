#pragma once
#include <QString>
#include "VariableModel.h"

// Evaluates the app's tiny variable-expression syntax used inside "{...}"
// placeholders, e.g. {preis}, {preis + 10}, {menge > 0}.
//
// Deliberately minimal: variable names / number literals / "text literals",
// + - * / on numbers (or + for text concatenation), comparisons
// (== != < > <= >=), parentheses, and the built-in pseudo-variables
// "heute" (today's date) and "jetzt" (current date+time). No functions,
// no loops. See VARIABLEN_PLAN.md for the rationale.
namespace VariableEngine {

struct Value {
    enum Kind { Number, Text, Boolean };
    Kind    kind = Text;
    double  number = 0.0;
    QString text;
    bool    boolean = false;

    static Value fromNumber(double n)   { Value v; v.kind = Number;  v.number = n; return v; }
    static Value fromText(const QString& s) { Value v; v.kind = Text;    v.text = s; return v; }
    static Value fromBool(bool b)       { Value v; v.kind = Boolean; v.boolean = b; return v; }

    // Human-readable form used when a value is inserted into displayed text.
    QString toDisplayString() const;
};

// Evaluates a single expression (the part between "{" and "}", without the
// braces). Returns false and fills errorOut on parse/evaluation errors
// (unknown variable, division by zero, type mismatch, ...).
bool evaluate(const QString& expr, const VariableSet& vars, const QString& currentSlideId,
              Value& result, QString& errorOut);

// Scans `raw` for "{...}" placeholders, evaluates each one and replaces it
// with its display value. Unrecognized/erroring placeholders are left
// visibly marked (not silently dropped, not a crash) so the author notices.
QString substitute(const QString& raw, const VariableSet& vars, const QString& currentSlideId);

} // namespace VariableEngine
