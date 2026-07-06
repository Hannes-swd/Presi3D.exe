#include "VariableEngine.h"
#include <QDateTime>
#include <QVector>
#include <cmath>

namespace VariableEngine {

QString Value::toDisplayString() const {
    switch (kind) {
    case Number: {
        if (std::abs(number - std::round(number)) < 1e-9)
            return QString::number(std::round(number), 'f', 0);
        return QString::number(number, 'f', 2);
    }
    case Boolean:
        return boolean ? QStringLiteral("wahr") : QStringLiteral("falsch");
    case Text:
    default:
        return text;
    }
}

namespace {

// ── Errors ────────────────────────────────────────────────────────────────
struct EvalError {
    QString message;
};

// ── Tokenizer ─────────────────────────────────────────────────────────────
struct Token {
    enum Kind { Num, Str, Ident, Op, End };
    Kind    kind = End;
    QString text;
    double  num = 0.0;
};

QVector<Token> tokenize(const QString& src) {
    QVector<Token> tokens;
    int i = 0;
    const int n = src.size();
    while (i < n) {
        QChar c = src[i];
        if (c.isSpace()) { ++i; continue; }

        if (c.isDigit() || (c == '.' && i + 1 < n && src[i + 1].isDigit())) {
            int start = i;
            while (i < n && (src[i].isDigit() || src[i] == '.')) ++i;
            Token t; t.kind = Token::Num; t.num = src.mid(start, i - start).toDouble();
            tokens << t;
            continue;
        }
        if (c == '"') {
            ++i;
            int start = i;
            while (i < n && src[i] != '"') ++i;
            Token t; t.kind = Token::Str; t.text = src.mid(start, i - start);
            tokens << t;
            if (i < n) ++i; // closing quote
            continue;
        }
        if (c.isLetter() || c == '_') {
            int start = i;
            while (i < n && (src[i].isLetterOrNumber() || src[i] == '_')) ++i;
            Token t; t.kind = Token::Ident; t.text = src.mid(start, i - start);
            tokens << t;
            continue;
        }
        // operators
        auto two = (i + 1 < n) ? src.mid(i, 2) : QString();
        if (two == "==" || two == "!=" || two == "<=" || two == ">=") {
            tokens << Token{Token::Op, two, 0.0};
            i += 2;
            continue;
        }
        if (QString("+-*/()<>").contains(c)) {
            tokens << Token{Token::Op, QString(c), 0.0};
            ++i;
            continue;
        }
        throw EvalError{QStringLiteral("unerwartetes Zeichen '%1'").arg(c)};
    }
    tokens << Token{Token::End, {}, 0.0};
    return tokens;
}

// ── Parser / evaluator ────────────────────────────────────────────────────
class Parser {
public:
    Parser(const QVector<Token>& tokens, const VariableSet& vars, const QString& slideId)
        : m_tokens(tokens), m_vars(vars), m_slideId(slideId) {}

    Value parseExpr() {
        Value v = parseComparison();
        if (!at(Token::End))
            throw EvalError{QStringLiteral("unerwartetes Zeichen am Ende")};
        return v;
    }

private:
    const QVector<Token>& m_tokens;
    const VariableSet&    m_vars;
    QString                m_slideId;
    int                    m_pos = 0;

    const Token& cur() const { return m_tokens[m_pos]; }
    bool at(Token::Kind k) const { return cur().kind == k; }
    bool atOp(const QString& s) const { return cur().kind == Token::Op && cur().text == s; }
    void advance() { if (m_pos + 1 < m_tokens.size()) ++m_pos; }

    static bool isNum(const Value& v)  { return v.kind == Value::Number; }

    static double asNumber(const Value& v) {
        if (v.kind == Value::Number)  return v.number;
        if (v.kind == Value::Boolean) return v.boolean ? 1.0 : 0.0;
        throw EvalError{QStringLiteral("erwarte eine Zahl, nicht Text")};
    }

    Value parseComparison() {
        Value left = parseSum();
        if (atOp("==") || atOp("!=") || atOp("<") || atOp(">") || atOp("<=") || atOp(">=")) {
            QString op = cur().text;
            advance();
            Value right = parseSum();
            bool result;
            if (isNum(left) && isNum(right)) {
                double a = left.number, b = right.number;
                if (op == "==") result = (a == b);
                else if (op == "!=") result = (a != b);
                else if (op == "<") result = (a < b);
                else if (op == ">") result = (a > b);
                else if (op == "<=") result = (a <= b);
                else result = (a >= b);
            } else if (left.kind == Value::Boolean && right.kind == Value::Boolean) {
                if (op == "==") result = (left.boolean == right.boolean);
                else if (op == "!=") result = (left.boolean != right.boolean);
                else throw EvalError{QStringLiteral("Wahr/Falsch-Werte lassen sich nur mit == oder != vergleichen")};
            } else {
                QString a = left.toDisplayString(), b = right.toDisplayString();
                int c = QString::compare(a, b);
                if (op == "==") result = (c == 0);
                else if (op == "!=") result = (c != 0);
                else if (op == "<") result = (c < 0);
                else if (op == ">") result = (c > 0);
                else if (op == "<=") result = (c <= 0);
                else result = (c >= 0);
            }
            return Value::fromBool(result);
        }
        return left;
    }

    Value parseSum() {
        Value left = parseProduct();
        while (atOp("+") || atOp("-")) {
            QString op = cur().text;
            advance();
            Value right = parseProduct();
            if (op == "+") {
                if (isNum(left) && isNum(right))
                    left = Value::fromNumber(left.number + right.number);
                else
                    left = Value::fromText(left.toDisplayString() + right.toDisplayString());
            } else {
                left = Value::fromNumber(asNumber(left) - asNumber(right));
            }
        }
        return left;
    }

    Value parseProduct() {
        Value left = parseUnary();
        while (atOp("*") || atOp("/")) {
            QString op = cur().text;
            advance();
            Value right = parseUnary();
            double a = asNumber(left), b = asNumber(right);
            if (op == "*") {
                left = Value::fromNumber(a * b);
            } else {
                if (b == 0.0) throw EvalError{QStringLiteral("Division durch 0")};
                left = Value::fromNumber(a / b);
            }
        }
        return left;
    }

    Value parseUnary() {
        if (atOp("-")) {
            advance();
            return Value::fromNumber(-asNumber(parseUnary()));
        }
        return parsePrimary();
    }

    Value parsePrimary() {
        if (at(Token::Num)) {
            double n = cur().num; advance();
            return Value::fromNumber(n);
        }
        if (at(Token::Str)) {
            QString s = cur().text; advance();
            return Value::fromText(s);
        }
        if (at(Token::Ident)) {
            QString name = cur().text; advance();
            return resolveIdent(name);
        }
        if (atOp("(")) {
            advance();
            Value v = parseComparison();
            if (!atOp(")")) throw EvalError{QStringLiteral("schließende Klammer ')' fehlt")};
            advance();
            return v;
        }
        throw EvalError{QStringLiteral("Ausdruck erwartet")};
    }

    Value resolveIdent(const QString& name) {
        if (name.compare("heute", Qt::CaseInsensitive) == 0)
            return Value::fromText(QDate::currentDate().toString("dd.MM.yyyy"));
        if (name.compare("jetzt", Qt::CaseInsensitive) == 0)
            return Value::fromText(QDateTime::currentDateTime().toString("dd.MM.yyyy HH:mm"));

        const Variable* v = m_vars.find(name, m_slideId);
        if (!v) throw EvalError{QStringLiteral("unbekannte Variable \"%1\"").arg(name)};

        switch (v->type) {
        case Variable::Number:  return Value::fromNumber(v->numberValue);
        case Variable::Boolean: return Value::fromBool(v->boolValue);
        case Variable::Text:
        default:                return Value::fromText(v->textValue);
        }
    }
};

} // namespace

bool evaluate(const QString& expr, const VariableSet& vars, const QString& currentSlideId,
              Value& result, QString& errorOut) {
    try {
        QVector<Token> tokens = tokenize(expr.trimmed());
        Parser parser(tokens, vars, currentSlideId);
        result = parser.parseExpr();
        return true;
    } catch (const EvalError& e) {
        errorOut = e.message;
        return false;
    }
}

QString substitute(const QString& raw, const VariableSet& vars, const QString& currentSlideId) {
    if (!raw.contains('{')) return raw;

    QString out;
    out.reserve(raw.size());
    int i = 0;
    const int n = raw.size();
    while (i < n) {
        QChar c = raw[i];
        if (c == '{') {
            int close = raw.indexOf('}', i + 1);
            if (close < 0) { out += raw.mid(i); break; } // no closing brace, keep rest literal
            QString expr = raw.mid(i + 1, close - i - 1);
            Value result;
            QString error;
            if (evaluate(expr, vars, currentSlideId, result, error))
                out += result.toDisplayString();
            else
                out += QStringLiteral("⟨?%1?⟩").arg(expr.trimmed());
            i = close + 1;
        } else {
            out += c;
            ++i;
        }
    }
    return out;
}

} // namespace VariableEngine
