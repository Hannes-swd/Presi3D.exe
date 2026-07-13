#pragma once
#include <QString>
#include <QVector>
#include <QColor>

// Lightweight, self-contained syntax highlighting for inline code blocks in
// Text elements — no external dependency, covers the curated language list
// offered in FormatBar's language combo (see FormatBar.cpp). Not a real
// parser: a best-effort tokenizer good enough for readable in-editor colors
// and for guessing "is this code, and which language" when text is pasted.
namespace CodeHighlighter {

enum class TokenKind { Plain, Keyword, String, Comment, Number, Tag, Attribute };

struct Token {
    int       start  = 0;
    int       length = 0;
    TokenKind kind   = TokenKind::Plain;
};

// Tokenizes `code` for the given language slug (matches FormatBar's combo
// data: "javascript", "python", "cpp", "csharp", "java", "html", "css",
// "json", "bash", "sql", "php", "xml", "plaintext"). Returns only the
// non-plain ranges (comments/strings/numbers/keywords/tags/attributes) —
// anything not covered is implicitly rendered as plain text.
QVector<Token> tokenize(const QString& code, const QString& language);

QColor colorForToken(TokenKind kind);

// Heuristic: does this pasted text look like source code at all (as
// opposed to ordinary prose)? Deliberately conservative to avoid marking
// normal sentences as code.
bool looksLikeCode(const QString& text);

// Heuristic best guess at which of the curated languages this text is,
// based on keyword/pattern scoring. Returns "plaintext" if no language
// scores convincingly.
QString guessLanguage(const QString& text);

} // namespace CodeHighlighter
