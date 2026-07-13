#include "CodeHighlighter.h"
#include <QMap>
#include <QSet>
#include <QRegularExpression>

namespace CodeHighlighter {

namespace {

struct LangConfig {
    QStringList    lineComments;          // e.g. {"//"} or {"#"} or {"--"}
    QString        blockCommentStart;     // e.g. "/*"; empty = none
    QString        blockCommentEnd;       // e.g. "*/"
    QString        stringQuotes;          // chars that open/close a string
    QSet<QString>  keywords;
    bool           caseInsensitiveKeywords = false;
};

QSet<QString> kw(std::initializer_list<const char*> words) {
    QSet<QString> s;
    for (const char* w : words) s.insert(QString::fromLatin1(w));
    return s;
}

const QMap<QString, LangConfig>& configs() {
    static const QMap<QString, LangConfig> table = [] {
        QMap<QString, LangConfig> m;

        m["javascript"] = LangConfig{
            {"//"}, "/*", "*/", "\"'`",
            kw({"function","return","var","let","const","if","else","for","while","do","switch",
                "case","break","continue","class","extends","new","this","typeof","instanceof",
                "try","catch","finally","throw","async","await","import","export","default",
                "from","of","in","null","undefined","true","false","void","yield","static",
                "get","set","super"}),
            false};

        m["python"] = LangConfig{
            {"#"}, "", "", "\"'",
            kw({"def","return","if","elif","else","for","while","break","continue","class",
                "import","from","as","try","except","finally","raise","with","lambda","pass",
                "yield","None","True","False","and","or","not","in","is","global","nonlocal",
                "assert","async","await","del","print"}),
            false};

        m["cpp"] = LangConfig{
            {"//"}, "/*", "*/", "\"'",
            kw({"int","float","double","char","bool","void","class","struct","public","private",
                "protected","static","const","virtual","override","return","if","else","for",
                "while","do","switch","case","break","continue","new","delete","this","namespace",
                "using","template","typename","include","define","nullptr","true","false","auto",
                "sizeof","throw","try","catch","enum"}),
            false};

        m["csharp"] = LangConfig{
            {"//"}, "/*", "*/", "\"'",
            kw({"using","namespace","class","public","private","protected","static","void","int",
                "string","bool","var","new","return","if","else","for","foreach","while","do",
                "switch","case","break","continue","try","catch","finally","throw","null","true",
                "false","async","await","get","set","override","virtual","interface","enum"}),
            false};

        m["java"] = LangConfig{
            {"//"}, "/*", "*/", "\"'",
            kw({"public","private","protected","class","interface","extends","implements","static",
                "void","int","boolean","String","new","return","if","else","for","while","do",
                "switch","case","break","continue","try","catch","finally","throw","throws",
                "import","package","this","super","null","true","false","enum","abstract","final"}),
            false};

        m["php"] = LangConfig{
            {"//","#"}, "/*", "*/", "\"'",
            kw({"function","return","if","else","elseif","for","foreach","while","do","switch",
                "case","break","continue","class","public","private","protected","static","new",
                "echo","print","require","include","namespace","use","try","catch","finally",
                "throw","null","true","false","array","isset","unset","global","const"}),
            false};

        m["bash"] = LangConfig{
            {"#"}, "", "", "\"'",
            kw({"if","then","else","elif","fi","for","do","done","while","case","esac","function",
                "echo","export","local","return","break","continue","in","until","select","exit"}),
            false};

        m["sql"] = LangConfig{
            {"--"}, "/*", "*/", "\"'",
            kw({"select","from","where","insert","into","values","update","set","delete","join",
                "inner","left","right","outer","on","group","by","order","having","as","and","or",
                "not","null","is","in","like","limit","create","table","alter","drop","distinct",
                "union","primary","key","default"}),
            true};

        m["css"] = LangConfig{ {}, "/*", "*/", "\"'", {}, false };

        m["json"] = LangConfig{
            {}, "", "", "\"",
            kw({"true","false","null"}),
            false};

        return m;
    }();
    return table;
}

QVector<Token> tokenizeGeneric(const QString& code, const LangConfig& cfg) {
    QVector<Token> tokens;
    const int n = code.length();
    int i = 0;
    while (i < n) {
        QChar c = code[i];

        bool matchedLineComment = false;
        for (const QString& lc : cfg.lineComments) {
            if (lc.isEmpty()) continue;
            if (code.mid(i, lc.length()) == lc) {
                int nl  = code.indexOf('\n', i);
                int len = (nl < 0 ? n : nl) - i;
                tokens << Token{i, len, TokenKind::Comment};
                i += len;
                matchedLineComment = true;
                break;
            }
        }
        if (matchedLineComment) continue;

        if (!cfg.blockCommentStart.isEmpty() &&
            code.mid(i, cfg.blockCommentStart.length()) == cfg.blockCommentStart) {
            int end = code.indexOf(cfg.blockCommentEnd, i + cfg.blockCommentStart.length());
            int len = (end < 0 ? n : end + cfg.blockCommentEnd.length()) - i;
            tokens << Token{i, len, TokenKind::Comment};
            i += len;
            continue;
        }

        if (cfg.stringQuotes.contains(c)) {
            QChar quote = c;
            int j = i + 1;
            while (j < n) {
                if (code[j] == '\\' && j + 1 < n) { j += 2; continue; }
                if (code[j] == quote) { j++; break; }
                if (code[j] == '\n') break;
                j++;
            }
            tokens << Token{i, j - i, TokenKind::String};
            i = j;
            continue;
        }

        if (c.isDigit()) {
            int j = i;
            while (j < n && (code[j].isLetterOrNumber() || code[j] == '.')) j++;
            tokens << Token{i, j - i, TokenKind::Number};
            i = j;
            continue;
        }

        if (c.isLetter() || c == '_') {
            int j = i;
            while (j < n && (code[j].isLetterOrNumber() || code[j] == '_')) j++;
            QString word = code.mid(i, j - i);
            QString key  = cfg.caseInsensitiveKeywords ? word.toLower() : word;
            if (cfg.keywords.contains(key))
                tokens << Token{i, j - i, TokenKind::Keyword};
            i = j;
            continue;
        }

        i++;
    }
    return tokens;
}

QVector<Token> tokenizeMarkup(const QString& code) {
    QVector<Token> tokens;
    static const QRegularExpression attrRe(
        R"(([a-zA-Z_:][\w:.-]*)\s*=\s*("[^"]*"|'[^']*'))");
    const int n = code.length();
    int i = 0;
    while (i < n) {
        if (code.mid(i, 4) == "<!--") {
            int end = code.indexOf("-->", i + 4);
            int len = (end < 0 ? n : end + 3) - i;
            tokens << Token{i, len, TokenKind::Comment};
            i += len;
            continue;
        }
        if (code[i] == '<') {
            int tagEnd = code.indexOf('>', i);
            int segEnd = (tagEnd < 0 ? n : tagEnd + 1);

            int p = i + 1;
            if (p < segEnd && code[p] == '/') p++;
            int nameStart = p;
            while (p < segEnd && (code[p].isLetterOrNumber() || code[p] == '-' || code[p] == ':')) p++;
            if (p > nameStart)
                tokens << Token{nameStart, p - nameStart, TokenKind::Tag};

            QString tagSlice = code.mid(i, segEnd - i);
            auto it = attrRe.globalMatch(tagSlice);
            while (it.hasNext()) {
                auto m = it.next();
                tokens << Token{i + int(m.capturedStart(1)), int(m.capturedLength(1)), TokenKind::Attribute};
                tokens << Token{i + int(m.capturedStart(2)), int(m.capturedLength(2)), TokenKind::String};
            }
            i = segEnd;
            continue;
        }
        i++;
    }
    return tokens;
}

// Distinctive substrings per language, used both to guess a language and to
// decide whether pasted text looks like code at all.
struct LangSignal { QString lang; QStringList patterns; bool caseInsensitive; };

const QVector<LangSignal>& languageSignals() {
    static const QVector<LangSignal> sigs = {
        {"php",        {"<?php", "->", "$this", "echo ", "function "}, false},
        {"html",       {"<!doctype", "<html", "<div", "<span", "</"}, true},
        {"xml",        {"<?xml", "xmlns"}, false},
        {"bash",       {"#!/bin/", "#!/usr/bin/env", "echo ", "fi\n", "done\n", "$(", "export "}, false},
        {"sql",        {"select ", "from ", "where ", "insert into", "create table"}, true},
        {"python",     {"def ", "elif ", "import ", "print(", "self.", "    return"}, false},
        {"csharp",     {"using system", "namespace ", "public class", "console.write", "void main"}, true},
        {"java",       {"public class", "system.out", "public static void main"}, true},
        {"cpp",        {"#include", "std::", "int main(", "cout <<", "nullptr"}, false},
        {"json",       {"\":", "{\""}, false},
        {"css",        {"px;", "margin:", "padding:", "@media"}, false},
        {"javascript", {"function ", "=>", "const ", "let ", "console.log", "document."}, false},
    };
    return sigs;
}

bool containsSignal(const QString& text, const LangSignal& sig) {
    for (const QString& pat : sig.patterns) {
        bool m = sig.caseInsensitive ? text.contains(pat, Qt::CaseInsensitive) : text.contains(pat);
        if (m) return true;
    }
    return false;
}

} // namespace

QVector<Token> tokenize(const QString& code, const QString& language) {
    if (language == "html" || language == "xml") return tokenizeMarkup(code);
    auto it = configs().find(language);
    if (it == configs().end()) return {};
    return tokenizeGeneric(code, it.value());
}

QColor colorForToken(TokenKind kind) {
    switch (kind) {
        case TokenKind::Keyword:   return QColor(0xc5, 0x86, 0xc0);
        case TokenKind::String:    return QColor(0xce, 0x91, 0x78);
        case TokenKind::Comment:   return QColor(0x6a, 0x99, 0x55);
        case TokenKind::Number:    return QColor(0xb5, 0xce, 0xa8);
        case TokenKind::Tag:       return QColor(0x56, 0x9c, 0xd6);
        case TokenKind::Attribute: return QColor(0x9c, 0xdc, 0xfe);
        case TokenKind::Plain:     break;
    }
    return QColor(); // invalid = caller keeps the element's own text color
}

bool looksLikeCode(const QString& text) {
    QString t = text.trimmed();
    if (t.length() < 8) return false;

    int score = 0;

    bool hasSignal = false;
    for (const LangSignal& sig : languageSignals())
        if (containsSignal(text, sig)) { hasSignal = true; break; }
    if (hasSignal) score += 2;

    int punct = 0;
    for (QChar c : t)
        if (QStringLiteral("{}();=<>[]").contains(c)) punct++;
    if (!t.isEmpty() && double(punct) / t.length() > 0.04) score += 2;

    QStringList lines = t.split('\n');
    if (lines.size() > 1) {
        int indented = 0;
        for (const QString& ln : lines)
            if (ln.startsWith(' ') || ln.startsWith('\t')) indented++;
        if (indented >= 1) score += 1;

        int codeEndings = 0;
        for (const QString& ln : lines) {
            QString tr = ln.trimmed();
            if (tr.endsWith(';') || tr.endsWith('{') || tr.endsWith('}') || tr.endsWith(':'))
                codeEndings++;
        }
        if (codeEndings >= 2) score += 1;
    }

    return score >= 3;
}

QString guessLanguage(const QString& text) {
    int bestScore = 0;
    QString best = "plaintext";
    for (const LangSignal& sig : languageSignals()) {
        int score = 0;
        for (const QString& pat : sig.patterns) {
            bool m = sig.caseInsensitive ? text.contains(pat, Qt::CaseInsensitive) : text.contains(pat);
            if (m) score++;
        }
        if (score > bestScore) { bestScore = score; best = sig.lang; }
    }
    return bestScore > 0 ? best : "plaintext";
}

} // namespace CodeHighlighter
