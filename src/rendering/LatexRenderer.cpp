#include "LatexRenderer.h"
#include <QPainter>
#include <QPainterPath>
#include <QFont>
#include <QFontMetricsF>
#include <QVector>
#include <QPair>
#include <QHash>

namespace {

// ── AST ─────────────────────────────────────────────────────────────────────

enum class Kind { Row, Text, Frac, Sqrt, Script };
enum class Style { Auto, Upright, Italic }; // only relevant for Text nodes

struct MNode {
    Kind    kind  = Kind::Row;
    QString text;                // Text
    Style   style = Style::Auto; // Text
    QVector<MNode> kids;         // Row: sequence | Frac: [num, den] | Sqrt: [radicand]
                                  // Script: [base, sup, sub]
    bool hasSup = false;
    bool hasSub = false;
};

struct Metrics {
    qreal width   = 0.0;
    qreal ascent  = 0.0;
    qreal descent = 0.0;
};

// ── Symbol tables ────────────────────────────────────────────────────────────

struct Sym { const char* cmd; const char* glyph; bool italic; };
static const Sym SYMBOLS[] = {
    {"pm", "±", false}, {"mp", "∓", false},
    {"times", "×", false}, {"cdot", "·", false}, {"div", "÷", false},
    {"leq", "≤", false}, {"geq", "≥", false}, {"neq", "≠", false},
    {"approx", "≈", false}, {"equiv", "≡", false},
    {"infty", "∞", false},
    {"rightarrow", "→", false}, {"to", "→", false}, {"leftarrow", "←", false},
    {"cdots", "⋯", false}, {"ldots", "…", false}, {"dots", "…", false},
    {"sum", "∑", false}, {"int", "∫", false}, {"partial", "∂", false},
    {"nabla", "∇", false}, {"prod", "∏", false},
    {"quad", " ", false}, {"qquad", "  ", false},
    {",", " ", false}, {";", " ", false}, {":", " ", false},
    {"alpha","α",true}, {"beta","β",true}, {"gamma","γ",true},
    {"delta","δ",true}, {"epsilon","ε",true}, {"varepsilon","ε",true},
    {"zeta","ζ",true}, {"eta","η",true}, {"theta","θ",true},
    {"iota","ι",true}, {"kappa","κ",true}, {"lambda","λ",true},
    {"mu","μ",true}, {"nu","ν",true}, {"xi","ξ",true},
    {"pi","π",true}, {"rho","ρ",true}, {"sigma","σ",true},
    {"tau","τ",true}, {"upsilon","υ",true}, {"phi","φ",true},
    {"varphi","φ",true}, {"chi","χ",true}, {"psi","ψ",true},
    {"omega","ω",true},
    {"Gamma","Γ",false}, {"Delta","Δ",false}, {"Theta","Θ",false},
    {"Lambda","Λ",false}, {"Xi","Ξ",false}, {"Pi","Π",false},
    {"Sigma","Σ",false}, {"Upsilon","Υ",false}, {"Phi","Φ",false},
    {"Psi","Ψ",false}, {"Omega","Ω",false},
};

static const QHash<QString, QPair<QString,bool>>& symbolMap() {
    static QHash<QString, QPair<QString,bool>> map = [] {
        QHash<QString, QPair<QString,bool>> m;
        for (const Sym& s : SYMBOLS) m.insert(QString::fromUtf8(s.cmd), {QString::fromUtf8(s.glyph), s.italic});
        return m;
    }();
    return map;
}

// ── Parser ───────────────────────────────────────────────────────────────────

class Parser {
public:
    explicit Parser(const QString& s) : m_s(s) {}

    MNode parseDocument() { return parseRow(); }

private:
    const QString& m_s;
    int m_pos = 0;

    bool atEnd() const { return m_pos >= m_s.size(); }
    QChar cur() const { return atEnd() ? QChar() : m_s[m_pos]; }
    void skipSpaces() { while (!atEnd() && m_s[m_pos].isSpace()) ++m_pos; }

    MNode parseRow() {
        MNode row; row.kind = Kind::Row;
        skipSpaces();
        while (!atEnd() && cur() != '}') {
            MNode atom = parseAtomWithScripts();
            appendMerging(row, atom);
            skipSpaces();
        }
        return row;
    }

    static void appendMerging(MNode& row, const MNode& atom) {
        if (atom.kind == Kind::Text && atom.style == Style::Auto &&
            !row.kids.isEmpty() && row.kids.last().kind == Kind::Text &&
            row.kids.last().style == Style::Auto) {
            row.kids.last().text += atom.text;
        } else {
            row.kids.append(atom);
        }
    }

    MNode parseAtomWithScripts() {
        MNode atom = parseAtom();
        skipSpaces();
        while (!atEnd() && (cur() == '^' || cur() == '_')) {
            QChar op = cur(); ++m_pos;
            skipSpaces();
            MNode script = parseAtom();
            atom = wrapScript(atom, op, script);
            skipSpaces();
        }
        return atom;
    }

    static MNode wrapScript(MNode atom, QChar op, const MNode& script) {
        MNode node;
        if (atom.kind == Kind::Script) {
            node = atom;
        } else {
            node.kind = Kind::Script;
            node.kids = { atom, MNode{}, MNode{} };
        }
        if (op == '^') { node.kids[1] = script; node.hasSup = true; }
        else            { node.kids[2] = script; node.hasSub = true; }
        return node;
    }

    MNode parseGroup() {
        // assumes cur() == '{'
        ++m_pos; // consume '{'
        MNode row = parseRow();
        if (!atEnd() && cur() == '}') ++m_pos; // consume '}'
        return row;
    }

    // Reads raw text until the matching closing brace, without interpreting commands.
    QString parseRawGroup() {
        if (atEnd() || cur() != '{') return {};
        ++m_pos;
        QString out;
        int depth = 1;
        while (!atEnd() && depth > 0) {
            QChar c = cur();
            if (c == '{') ++depth;
            else if (c == '}') { --depth; if (depth == 0) { ++m_pos; break; } }
            if (depth > 0) out += c;
            ++m_pos;
        }
        return out;
    }

    // Skips an optional [ ... ] (e.g. the root index of \sqrt[3]{x})
    void skipOptionalBracket() {
        skipSpaces();
        if (!atEnd() && cur() == '[') {
            ++m_pos;
            while (!atEnd() && cur() != ']') ++m_pos;
            if (!atEnd()) ++m_pos;
        }
    }

    QString readCommandName() {
        // m_pos is positioned right after the backslash
        if (atEnd()) return {};
        if (cur().isLetter()) {
            QString name;
            while (!atEnd() && cur().isLetter()) { name += cur(); ++m_pos; }
            return name;
        }
        QString single(cur());
        ++m_pos;
        return single;
    }

    static MNode textNode(const QString& s, Style style = Style::Auto) {
        MNode n; n.kind = Kind::Text; n.text = s; n.style = style;
        return n;
    }

    MNode parseAtom() {
        skipSpaces();
        if (atEnd()) return textNode(QString());

        if (cur() == '\\') {
            ++m_pos; // consume backslash
            QString cmd = readCommandName();

            if (cmd == "frac") {
                MNode f; f.kind = Kind::Frac;
                skipSpaces(); MNode num = (cur() == '{') ? parseGroup() : parseAtom();
                skipSpaces(); MNode den = (cur() == '{') ? parseGroup() : parseAtom();
                f.kids = { num, den };
                return f;
            }
            if (cmd == "sqrt") {
                skipOptionalBracket();
                MNode s; s.kind = Kind::Sqrt;
                skipSpaces();
                MNode radicand = (cur() == '{') ? parseGroup() : parseAtom();
                s.kids = { radicand };
                return s;
            }
            if (cmd == "text" || cmd == "mathrm" || cmd == "operatorname" || cmd == "mbox") {
                return textNode(parseRawGroup(), Style::Upright);
            }
            if (cmd == "mathbf" || cmd == "boldsymbol") {
                return textNode(parseRawGroup(), Style::Upright);
            }
            if (cmd == "left" || cmd == "right") {
                skipSpaces();
                QString delim;
                if (!atEnd() && cur() == '\\') {
                    ++m_pos;
                    delim = readCommandName();
                    if (delim == "{") delim = "{";
                    else if (delim == "}") delim = "}";
                } else if (!atEnd()) {
                    delim = cur(); ++m_pos;
                }
                if (delim == "." ) return textNode(QString());
                return textNode(delim, Style::Upright);
            }
            const auto& map = symbolMap();
            auto it = map.find(cmd);
            if (it != map.end())
                return textNode(it->first, it->second ? Style::Italic : Style::Upright);

            // Unknown command: show its name so nothing silently vanishes.
            return textNode(cmd, Style::Upright);
        }

        if (cur() == '{') return parseGroup();

        QChar c = cur(); ++m_pos;
        return textNode(QString(c));
    }
};

// ── Text run splitting (italic letters, upright everything else) ───────────

static QVector<QPair<QString,bool>> splitRuns(const QString& text, Style style) {
    QVector<QPair<QString,bool>> runs;
    if (style == Style::Upright) { if (!text.isEmpty()) runs.append({text, false}); return runs; }
    if (style == Style::Italic)  { if (!text.isEmpty()) runs.append({text, true});  return runs; }

    bool curItalic = false;
    QString cur;
    for (const QChar& c : text) {
        bool italic = c.isLetter();
        if (cur.isEmpty()) { curItalic = italic; cur += c; continue; }
        if (italic == curItalic) { cur += c; continue; }
        runs.append({cur, curItalic});
        cur = QString(c);
        curItalic = italic;
    }
    if (!cur.isEmpty()) runs.append({cur, curItalic});
    return runs;
}

static Metrics measureText(const QString& text, Style style, const QString& family, qreal px) {
    QFont f(family, qMax(1, int(px)));
    QFontMetricsF fm(f);
    qreal w = 0;
    for (const auto& run : splitRuns(text, style)) {
        QFont rf = f; rf.setItalic(run.second);
        w += QFontMetricsF(rf).horizontalAdvance(run.first);
    }
    return { w, qreal(fm.ascent()), qreal(fm.descent()) };
}

static void paintText(QPainter& p, const QString& text, Style style, qreal x, qreal baselineY,
                      const QString& family, qreal px, const QColor& color) {
    QFont f(family, qMax(1, int(px)));
    qreal cx = x;
    for (const auto& run : splitRuns(text, style)) {
        QFont rf = f; rf.setItalic(run.second);
        p.setFont(rf);
        p.setPen(color);
        p.drawText(QPointF(cx, baselineY), run.first);
        cx += QFontMetricsF(rf).horizontalAdvance(run.first);
    }
}

// ── Recursive measure / paint ───────────────────────────────────────────────

static Metrics measureNode(const MNode& n, const QString& family, qreal px);
static void paintNode(QPainter& p, const MNode& n, qreal x, qreal baselineY,
                      const QString& family, qreal px, const QColor& color);

static Metrics measureNode(const MNode& n, const QString& family, qreal px) {
    switch (n.kind) {
    case Kind::Text:
        return measureText(n.text, n.style, family, px);

    case Kind::Row: {
        Metrics m;
        for (const MNode& k : n.kids) {
            Metrics km = measureNode(k, family, px);
            m.width  += km.width;
            m.ascent  = qMax(m.ascent,  km.ascent);
            m.descent = qMax(m.descent, km.descent);
        }
        return m;
    }
    case Kind::Frac: {
        Metrics num = measureNode(n.kids[0], family, px);
        Metrics den = measureNode(n.kids[1], family, px);
        qreal gap    = px * 0.14;
        qreal thick  = qMax(1.0, px * 0.05);
        qreal pad    = px * 0.12;
        qreal axis   = px * 0.25; // bar sits a fixed distance above the baseline, not scaled by content
        Metrics m;
        m.width   = qMax(num.width, den.width) + pad * 2;
        m.ascent  = axis + thick * 0.5 + gap + num.ascent + num.descent;
        m.descent = qMax(0.0, thick * 0.5 + gap + den.ascent + den.descent - axis);
        return m;
    }
    case Kind::Sqrt: {
        Metrics r = measureNode(n.kids[0], family, px);
        qreal gapAbove = px * 0.14;
        qreal thick    = qMax(1.0, px * 0.05);
        qreal glyphH   = r.ascent + r.descent + gapAbove;
        qreal symW     = glyphH * 0.5;
        qreal gap      = px * 0.08;
        Metrics m;
        m.width   = symW + gap + r.width;
        m.ascent  = r.ascent + gapAbove + thick;
        m.descent = r.descent;
        return m;
    }
    case Kind::Script: {
        Metrics base = measureNode(n.kids[0], family, px);
        qreal scriptPx = px * 0.66;
        Metrics sup = n.hasSup ? measureNode(n.kids[1], family, scriptPx) : Metrics{};
        Metrics sub = n.hasSub ? measureNode(n.kids[2], family, scriptPx) : Metrics{};
        qreal supShiftUp   = px * 0.42;
        qreal subShiftDown = px * 0.18;
        Metrics m;
        qreal scriptW = qMax(n.hasSup ? sup.width : 0.0, n.hasSub ? sub.width : 0.0);
        m.width   = base.width + (scriptW > 0 ? scriptW + px * 0.06 : 0);
        m.ascent  = qMax(base.ascent,  n.hasSup ? supShiftUp + sup.ascent : 0.0);
        m.descent = qMax(base.descent, n.hasSub ? subShiftDown + sub.descent : 0.0);
        return m;
    }
    }
    return {};
}

static void paintNode(QPainter& p, const MNode& n, qreal x, qreal baselineY,
                      const QString& family, qreal px, const QColor& color) {
    switch (n.kind) {
    case Kind::Text:
        paintText(p, n.text, n.style, x, baselineY, family, px, color);
        return;

    case Kind::Row: {
        qreal cx = x;
        for (const MNode& k : n.kids) {
            paintNode(p, k, cx, baselineY, family, px, color);
            cx += measureNode(k, family, px).width;
        }
        return;
    }
    case Kind::Frac: {
        Metrics num = measureNode(n.kids[0], family, px);
        Metrics den = measureNode(n.kids[1], family, px);
        qreal gap   = px * 0.14;
        qreal thick = qMax(1.0, px * 0.05);
        qreal pad   = px * 0.12;
        qreal axis  = px * 0.25;
        qreal width = qMax(num.width, den.width) + pad * 2;

        qreal barY = baselineY - axis;
        qreal numBaselineY = barY - thick * 0.5 - gap - num.descent;
        qreal denBaselineY = barY + thick * 0.5 + gap + den.ascent;

        paintNode(p, n.kids[0], x + (width - num.width) / 2.0, numBaselineY, family, px, color);
        paintNode(p, n.kids[1], x + (width - den.width) / 2.0, denBaselineY, family, px, color);

        QPen pen(color, thick);
        p.save();
        p.setPen(pen);
        p.drawLine(QPointF(x, barY), QPointF(x + width, barY));
        p.restore();
        return;
    }
    case Kind::Sqrt: {
        Metrics r = measureNode(n.kids[0], family, px);
        qreal gapAbove = px * 0.14;
        qreal thick    = qMax(1.0, px * 0.05);
        qreal glyphH   = r.ascent + r.descent + gapAbove;
        qreal symW     = glyphH * 0.5;
        qreal gap      = px * 0.08;

        qreal topY = baselineY - r.ascent - gapAbove;
        qreal botY = baselineY + r.descent;

        QPainterPath path;
        path.moveTo(x, topY + glyphH * 0.55);
        path.lineTo(x + symW * 0.3, botY);
        path.lineTo(x + symW * 0.7, topY);
        path.lineTo(x + symW + gap + r.width, topY);

        QPen pen(color, thick, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        p.save();
        p.setPen(pen);
        p.drawPath(path);
        p.restore();

        paintNode(p, n.kids[0], x + symW + gap, baselineY, family, px, color);
        return;
    }
    case Kind::Script: {
        Metrics base = measureNode(n.kids[0], family, px);
        qreal scriptPx = px * 0.66;
        qreal supShiftUp   = px * 0.42;
        qreal subShiftDown = px * 0.18;

        paintNode(p, n.kids[0], x, baselineY, family, px, color);
        qreal scriptX = x + base.width + px * 0.04;
        if (n.hasSup) paintNode(p, n.kids[1], scriptX, baselineY - supShiftUp, family, scriptPx, color);
        if (n.hasSub) paintNode(p, n.kids[2], scriptX, baselineY + subShiftDown, family, scriptPx, color);
        return;
    }
    }
}

} // namespace

QSizeF LatexRenderer::measure(const QString& latex, const QString& fontFamily, qreal fontPx) {
    Parser parser(latex);
    MNode root = parser.parseDocument();
    Metrics m = measureNode(root, fontFamily, fontPx);
    return QSizeF(m.width, m.ascent + m.descent);
}

void LatexRenderer::paint(QPainter& painter, const QRectF& rect, const QString& latex,
                          const QString& fontFamily, qreal fontPx, const QColor& color) {
    if (latex.trimmed().isEmpty() || rect.width() < 1.0 || rect.height() < 1.0) return;

    Parser parser(latex);
    MNode root = parser.parseDocument();
    Metrics m = measureNode(root, fontFamily, fontPx);
    if (m.width <= 0 || (m.ascent + m.descent) <= 0) return;

    // Scale to fill the box (both up and down) so resizing the element resizes the formula,
    // rather than only shrinking oversized content and leaving extra space unused otherwise.
    qreal scale = qMin(rect.width() / m.width, rect.height() / (m.ascent + m.descent));

    qreal usedPx = fontPx * scale;
    if (!qFuzzyCompare(scale, 1.0)) m = measureNode(root, fontFamily, usedPx);

    qreal totalW = m.width;
    qreal totalH = m.ascent + m.descent;
    qreal x = rect.x() + (rect.width()  - totalW) / 2.0;
    qreal baselineY = rect.y() + (rect.height() - totalH) / 2.0 + m.ascent;

    painter.save();
    painter.setRenderHint(QPainter::Antialiasing);
    paintNode(painter, root, x, baselineY, fontFamily, usedPx, color);
    painter.restore();
}
