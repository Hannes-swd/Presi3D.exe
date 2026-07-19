#include "HtmlExporter.h"
#include "ShapeUtils.h"
#include "IconUtils.h"
#include "rendering/ChartRenderer.h"
#include "models/VariableEngine.h"
#include "models/TimelineEngine.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QImage>
#include <QPainter>
#include <QBuffer>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QUrl>
#include <QSet>
#include <algorithm>
#define TINYGLTF_NO_STB_IMAGE_WRITE
#include "tiny_gltf.h"

// Bundled impress.js — embedded via Qt resource system (resources/icons.qrc)
static const char* IMPRESS_JS_SRC = ":/impress.js";

HtmlExporter::Result HtmlExporter::exportTo(const Presentation& pres,
                                             const QString& outputDir) {
    QDir dir(outputDir);
    if (!dir.exists() && !dir.mkpath("."))
        return {false, "Could not create folder:\n" + outputDir};

    dir.mkpath("assets");
    QString assetsPath = dir.filePath("assets");

    // Copy impress.js
    QString jsDest = dir.filePath("impress.js");
    if (!QFile::exists(jsDest)) {
        if (!QFile::copy(QString::fromUtf8(IMPRESS_JS_SRC), jsDest))
            return {false, QString("Could not copy impress.js.\n"
                                   "Expected at: %1").arg(IMPRESS_JS_SRC)};
    }

    QStringList imgErrors;
    copyImages(pres, assetsPath, imgErrors);
    copyModels(pres, assetsPath, imgErrors);
    cleanupOrphanedAssets(pres, assetsPath);

    // styles.css
    {
        QFile f(dir.filePath("styles.css"));
        if (!f.open(QIODevice::WriteOnly | QIODevice::Text))
            return {false, "Cannot write styles.css."};
        QTextStream s(&f);
        s << generateCss(pres);
    }

    // index.html
    {
        QFile f(dir.filePath("index.html"));
        if (!f.open(QIODevice::WriteOnly | QIODevice::Text))
            return {false, "Cannot write index.html."};
        QTextStream s(&f);
        s.setEncoding(QStringConverter::Utf8);
        s << generateHtml(pres);
    }

    QString warn;
    if (!imgErrors.isEmpty())
        warn = "\n\nWarning - the following images could not be copied:\n"
               + imgErrors.join("\n");

    return {true, warn};
}

QString HtmlExporter::generateCss(const Presentation& pres) {
    QString bg = colorToCss(pres.sceneBackground.isValid()
                             ? pres.sceneBackground : QColor(0x11, 0x11, 0x11));
    int sw = int(pres.slideWidth  > 0 ? pres.slideWidth  : 1920);
    int sh = int(pres.slideHeight > 0 ? pres.slideHeight : 1080);

    return QString(R"css(
* { margin: 0; padding: 0; box-sizing: border-box; }

body {
    background: %1;
    overflow: hidden;
    font-family: sans-serif;
}

.step {
    position: relative;
    width: %2px;
    height: %3px;
    overflow: hidden;
    transition: opacity 0.6s ease-in-out;
}

/* Opacity is managed via JavaScript for per-slide control */

/* Fix: prevent GPU sub-pixel blur on images during 3D CSS transforms */
.step img {
    backface-visibility: hidden;
    -webkit-backface-visibility: hidden;
    transform: translateZ(0);
    image-rendering: -webkit-optimize-contrast;
}

/* Timeline entry/exit animations transition plain CSS properties (opacity,
   transform, color, position, size, ...) driven by the TimelinePlayer JS
   below — no canned @keyframes needed, since keyframe overrides are
   arbitrary per element. Must list every property tlApplyFrame() (in the
   script below) ever sets via el.style.*, or that property jumps instantly
   instead of transitioning regardless of the JS-set transition-duration. */
[data-timeline] { transition-property: opacity, transform, background-color, color, border-color,
    left, top, width, height, border-radius, border-width, font-size; }

.step video, .step audio {
    backface-visibility: hidden;
    -webkit-backface-visibility: hidden;
    transform: translateZ(0);
}

/* Waveform audio player (see initWaveformAudio in the script below): CSS-only
   play/pause glyph swap, toggled by the "playing" class the JS adds/removes. */
.audio-playbtn { position: relative; flex: none; width: 16px; height: 16px; }
.audio-playbtn .ico-play {
    position: absolute; inset: 0; width: 0; height: 0;
    border-style: solid; border-width: 8px 0 8px 13px;
    border-color: transparent transparent transparent #fff;
}
.audio-playbtn .ico-pause { position: absolute; inset: 0; display: none; gap: 3px; }
.audio-playbtn .ico-pause span { display: block; width: 4px; height: 16px; background: #fff; }
[data-audio-mode].playing .audio-playbtn .ico-play  { display: none; }
[data-audio-mode].playing .audio-playbtn .ico-pause { display: flex; }
/* No color here: inherits the per-element color set inline on the wrapper div */
.audio-time { flex: none; font: 12px sans-serif; min-width: 34px; text-align: right; }
.audio-label { flex: 1 1 auto; font: 12px sans-serif; white-space: nowrap; overflow: hidden; text-overflow: ellipsis; }
)css"
    ).arg(bg).arg(sw).arg(sh);
}

// Build uuid→htmlId map and per-slide visibility override strings
static void buildVisibilityData(const Presentation& pres,
                                 QMap<QString, QString>& uuidToHtmlId,
                                 QMap<QString, QString>& uuidToVisString)
{
    for (int i = 0; i < pres.slides.size(); ++i)
        uuidToHtmlId[pres.slides[i].id] = QString("slide-%1").arg(i + 1);

    for (const Slide& s : pres.slides) {
        if (s.visibilityOverrides.isEmpty()) continue;
        QStringList pairs;
        for (auto it = s.visibilityOverrides.cbegin(); it != s.visibilityOverrides.cend(); ++it) {
            if (!uuidToHtmlId.contains(it.key())) continue;
            pairs << uuidToHtmlId[it.key()] + ':' + QString::number(it.value(), 'f', 2);
        }
        if (!pairs.isEmpty())
            uuidToVisString[s.id] = pairs.join(',');
    }
}

QString HtmlExporter::buildVariablesJson(const Presentation& pres,
                                         const QMap<QString, QString>& uuidToHtmlId) {
    QJsonArray arr;
    for (const Variable& v : pres.variables.items) {
        QJsonObject o;
        o["id"]           = v.id;
        o["name"]         = v.name;
        o["type"]         = int(v.type); // Text=0, Number=1, Boolean=2 (mirrors Variable::Type)
        o["textValue"]    = v.textValue;
        o["numberValue"]  = v.numberValue;
        o["boolValue"]    = v.boolValue;
        // Scope is stored as a Slide UUID internally; translate to the exported step's html id.
        o["scopeSlideId"] = v.scopeSlideId.isEmpty() ? QString() : uuidToHtmlId.value(v.scopeSlideId);
        arr.append(o);
    }
    QString json = QString::fromUtf8(QJsonDocument(arr).toJson(QJsonDocument::Compact));
    json.replace("</", "<\\/"); // don't let a text value accidentally close the <script> tag
    return json;
}

// Does any slide contain a formula element? Only then load MathJax.
static bool hasFormulaElement(const Presentation& pres) {
    for (const Slide& s : pres.slides)
        for (const SlideElement& e : s.elements)
            if (e.type == SlideElement::Formula) return true;
    return false;
}

// Does any slide contain a text element with inline code spans? Only then load highlight.js.
static bool hasCodeBlockElement(const Presentation& pres) {
    for (const Slide& s : pres.slides)
        for (const SlideElement& e : s.elements)
            if (e.type == SlideElement::Text && !e.codeSpans.isEmpty()) return true;
    return false;
}

static QString escapeAndBr(const QString& s) {
    return s.toHtmlEscaped().replace("\n", "<br>");
}

// Builds a text element's inner HTML, wrapping e.codeSpans in
// <code class="language-xxx"> for highlight.js. Mirror of HtmlImporter's
// code-span parsing — keep both in sync.
static QString buildTextWithCodeSpans(const SlideElement& e) {
    QVector<CodeSpan> spans = e.codeSpans;
    std::sort(spans.begin(), spans.end(),
              [](const CodeSpan& a, const CodeSpan& b) { return a.start < b.start; });
    const QString& content = e.content;
    QString result;
    int pos = 0;
    for (const CodeSpan& sp : spans) {
        int start = qBound(0, sp.start, content.length());
        int len   = qBound(0, sp.length, content.length() - start);
        if (start < pos || len <= 0) continue; // overlapping/empty span, skip defensively
        if (start > pos)
            result += escapeAndBr(content.mid(pos, start - pos));
        QString lang = sp.language.isEmpty() ? "plaintext" : sp.language;
        result += QString("<code class=\"language-%1\">%2</code>")
                      .arg(lang.toHtmlEscaped(), escapeAndBr(content.mid(start, len)));
        pos = start + len;
    }
    if (pos < content.length())
        result += escapeAndBr(content.mid(pos));
    return result;
}

QString HtmlExporter::generateHtml(const Presentation& pres) {
    QMap<QString, QString> uuidToHtmlId;
    QMap<QString, QString> uuidToVisString;
    buildVisibilityData(pres, uuidToHtmlId, uuidToVisString);

    QString html;
    QTextStream out(&html);

    double defOpa = qBound(0.0, double(pres.defaultInactiveOpacity), 1.0);

    out << "<!DOCTYPE html>\n"
        << "<html lang=\"en\">\n"
        << "<head>\n"
        << "  <meta charset=\"UTF-8\">\n"
        << "  <title>"
        << VariableEngine::substitute(pres.title.isEmpty() ? QString("Presentation") : pres.title,
                                       pres.variables, QString(), 0, pres.slides.size()).toHtmlEscaped()
        << "</title>\n"
        << "  <link rel=\"stylesheet\" href=\"styles.css\">\n";
    if (hasFormulaElement(pres))
        out << "  <script id=\"MathJax-script\" async "
               "src=\"https://cdn.jsdelivr.net/npm/mathjax@3/es5/tex-mml-chtml.js\"></script>\n";
    if (!pres.worldObjects.isEmpty())
        out << "  <script type=\"module\" "
               "src=\"https://unpkg.com/@google/model-viewer/dist/model-viewer.min.js\"></script>\n";
    if (hasCodeBlockElement(pres))
        out << "  <link rel=\"stylesheet\" "
               "href=\"https://cdnjs.cloudflare.com/ajax/libs/highlight.js/11.9.0/styles/github-dark.min.css\">\n"
               "  <script src=\"https://cdnjs.cloudflare.com/ajax/libs/highlight.js/11.9.0/highlight.min.js\"></script>\n";
    out << "</head>\n"
        << "<body>\n\n"
        << "<div id=\"impress\"\n"
        << "     data-transition-duration=\"1000\"\n"
        << "     data-width=\""  << int(pres.slideWidth  > 0 ? pres.slideWidth  : 1920) << "\"\n"
        << "     data-height=\"" << int(pres.slideHeight > 0 ? pres.slideHeight : 1080) << "\"\n"
        << "     data-max-scale=\"3\"\n"
        << "     data-min-scale=\"0\"\n"
        << "     data-default-inactive-opacity=\""
        << QString::number(defOpa, 'f', 2) << "\">\n\n";

    for (int i = 0; i < pres.slides.size(); ++i)
        out << slideToHtml(pres.slides[i], i + 1, pres.slides.size(), uuidToHtmlId, uuidToVisString, pres.variables) << "\n\n";

    for (const WorldObject& w : pres.worldObjects)
        out << worldObjectToHtml(w) << "\n\n";

    out << "</div>\n\n"
        << "<script src=\"impress.js\"></script>\n"
        << "<script>\n"
        << "var api = impress();\n"
        << "api.init();\n"
        // hljs.highlightAll() only picks up `pre code` by default; our code
        // spans are inline <code class="language-x"> without a <pre> wrapper,
        // so each one needs to be highlighted explicitly.
        << "if (window.hljs) document.querySelectorAll('code[class*=\"language-\"]')"
           ".forEach(function(el){ hljs.highlightElement(el); });\n"
        << "var steps = Array.from(document.querySelectorAll('.step'));\n"
        << "var impEl = document.getElementById('impress');\n"
        << "var defaultInactiveOpacity = parseFloat(impEl.dataset.defaultInactiveOpacity || 0.3);\n"
        << "\n"
        << "// ── Variables: tiny expression engine + live substitution/interaction ────\n"
        << "// Mirrors src/models/VariableEngine.cpp — keep both in sync (see VARIABLEN_PLAN.md).\n"
        << "var presVariables = " << buildVariablesJson(pres, uuidToHtmlId) << ";\n"
        << "function findVarById(id) {\n"
        << "  for (var i = 0; i < presVariables.length; i++) if (presVariables[i].id === id) return presVariables[i];\n"
        << "  return null;\n"
        << "}\n"
        << "function findVarByName(name, slideId) {\n"
        << "  var local = null, global = null;\n"
        << "  for (var i = 0; i < presVariables.length; i++) {\n"
        << "    var v = presVariables[i];\n"
        << "    if (v.name.toLowerCase() !== name.toLowerCase()) continue;\n"
        << "    if (v.scopeSlideId && v.scopeSlideId === slideId) local = v;\n"
        << "    else if (!v.scopeSlideId) global = v;\n"
        << "  }\n"
        << "  return local || global;\n"
        << "}\n"
        << "function pad2(n) { return (n < 10 ? '0' : '') + n; }\n"
        << "function formatNum(n) {\n"
        << "  var r = Math.round(n);\n"
        << "  return (Math.abs(n - r) < 1e-9) ? String(r) : n.toFixed(2);\n"
        << "}\n"
        << "var MONTH_NAMES_EN = ['January','February','March','April','May','June','July','August','September','October','November','December'];\n"
        << "var DAY_NAMES_EN = ['Sunday','Monday','Tuesday','Wednesday','Thursday','Friday','Saturday'];\n"
        << "function isoWeekNumber(d) {\n"
        << "  var date = new Date(Date.UTC(d.getFullYear(), d.getMonth(), d.getDate()));\n"
        << "  var dayNum = date.getUTCDay() || 7;\n"
        << "  date.setUTCDate(date.getUTCDate() + 4 - dayNum);\n"
        << "  var yearStart = new Date(Date.UTC(date.getUTCFullYear(), 0, 1));\n"
        << "  return Math.ceil((((date - yearStart) / 86400000) + 1) / 7);\n"
        << "}\n"
        << "function isVarLetter(ch) { return /[A-Za-zÀ-ÖØ-öø-ÿ_]/.test(ch); }\n"
        << "function tokenizeExpr(src) {\n"
        << "  var tokens = [], i = 0, n = src.length;\n"
        << "  while (i < n) {\n"
        << "    var c = src[i];\n"
        << "    if (/\\s/.test(c)) { i++; continue; }\n"
        << "    if (/[0-9]/.test(c) || (c === '.' && i + 1 < n && /[0-9]/.test(src[i+1]))) {\n"
        << "      var start = i;\n"
        << "      while (i < n && /[0-9.]/.test(src[i])) i++;\n"
        << "      tokens.push({k:'num', v: parseFloat(src.slice(start, i))});\n"
        << "      continue;\n"
        << "    }\n"
        << "    if (c === '\"') {\n"
        << "      i++; var s2 = i;\n"
        << "      while (i < n && src[i] !== '\"') i++;\n"
        << "      tokens.push({k:'str', v: src.slice(s2, i)});\n"
        << "      if (i < n) i++;\n"
        << "      continue;\n"
        << "    }\n"
        << "    if (isVarLetter(c)) {\n"
        << "      var s3 = i;\n"
        << "      while (i < n && (isVarLetter(src[i]) || /[0-9]/.test(src[i]))) i++;\n"
        << "      tokens.push({k:'ident', v: src.slice(s3, i)});\n"
        << "      continue;\n"
        << "    }\n"
        << "    var two = src.substr(i, 2);\n"
        << "    if (['==','!=','<=','>='].indexOf(two) !== -1) { tokens.push({k:'op', v: two}); i += 2; continue; }\n"
        << "    if ('+-*/()<>'.indexOf(c) !== -1) { tokens.push({k:'op', v: c}); i++; continue; }\n"
        << "    throw new Error('unerwartetes Zeichen \"' + c + '\"');\n"
        << "  }\n"
        << "  tokens.push({k:'end', v:''});\n"
        << "  return tokens;\n"
        << "}\n"
        << "function evalExpr(expr, slideId) {\n"
        << "  var tokens = tokenizeExpr(expr.trim());\n"
        << "  var pos = 0;\n"
        << "  function cur() { return tokens[pos]; }\n"
        << "  function atOp(s) { return cur().k === 'op' && cur().v === s; }\n"
        << "  function advance() { if (pos + 1 < tokens.length) pos++; }\n"
        << "  function asNumber(v) {\n"
        << "    if (v.kind === 'num') return v.value;\n"
        << "    if (v.kind === 'bool') return v.value ? 1 : 0;\n"
        << "    throw new Error('erwarte eine Zahl');\n"
        << "  }\n"
        << "  function toDisplay(v) {\n"
        << "    if (v.kind === 'num') return formatNum(v.value);\n"
        << "    if (v.kind === 'bool') return v.value ? 'wahr' : 'falsch';\n"
        << "    return v.value;\n"
        << "  }\n"
        << "  function resolveIdent(name) {\n"
        << "    var lname = name.toLowerCase();\n"
        << "    if (lname === 'today' || lname === 'now') {\n"
        << "      var d = new Date();\n"
        << "      var s = pad2(d.getDate()) + '.' + pad2(d.getMonth()+1) + '.' + d.getFullYear();\n"
        << "      if (lname === 'now') s += ' ' + pad2(d.getHours()) + ':' + pad2(d.getMinutes());\n"
        << "      return {kind:'text', value: s};\n"
        << "    }\n"
        << "    if (lname === 'year') return {kind:'num', value: new Date().getFullYear()};\n"
        << "    if (lname === 'month') return {kind:'num', value: new Date().getMonth() + 1};\n"
        << "    if (lname === 'monthname') return {kind:'text', value: MONTH_NAMES_EN[new Date().getMonth()]};\n"
        << "    if (lname === 'day') return {kind:'num', value: new Date().getDate()};\n"
        << "    if (lname === 'weekday') return {kind:'text', value: DAY_NAMES_EN[new Date().getDay()]};\n"
        << "    if (lname === 'week') return {kind:'num', value: isoWeekNumber(new Date())};\n"
        << "    if (lname === 'time') { var d2 = new Date(); return {kind:'text', value: pad2(d2.getHours()) + ':' + pad2(d2.getMinutes())}; }\n"
        << "    if (lname === 'hour') return {kind:'num', value: new Date().getHours()};\n"
        << "    if (lname === 'minute') return {kind:'num', value: new Date().getMinutes()};\n"
        << "    if (lname === 'slidenumber') {\n"
        << "      var sn = currentSlideNumber();\n"
        << "      if (!sn) throw new Error('\"slideNumber\" ist hier nicht verfügbar');\n"
        << "      return {kind:'num', value: sn};\n"
        << "    }\n"
        << "    if (lname === 'totalslides') return {kind:'num', value: totalSlideCount()};\n"
        << "    var v = findVarByName(name, slideId);\n"
        << "    if (!v) throw new Error('unbekannte Variable \"' + name + '\"');\n"
        << "    if (v.type === 1) return {kind:'num', value: v.numberValue};\n"
        << "    if (v.type === 2) return {kind:'bool', value: v.boolValue};\n"
        << "    return {kind:'text', value: v.textValue};\n"
        << "  }\n"
        << "  function parsePrimary() {\n"
        << "    var t = cur();\n"
        << "    if (t.k === 'num') { advance(); return {kind:'num', value:t.v}; }\n"
        << "    if (t.k === 'str') { advance(); return {kind:'text', value:t.v}; }\n"
        << "    if (t.k === 'ident') { advance(); return resolveIdent(t.v); }\n"
        << "    if (atOp('(')) { advance(); var v2 = parseComparison(); if (!atOp(')')) throw new Error('schließende Klammer fehlt'); advance(); return v2; }\n"
        << "    throw new Error('Ausdruck erwartet');\n"
        << "  }\n"
        << "  function parseUnary() {\n"
        << "    if (atOp('-')) { advance(); return {kind:'num', value: -asNumber(parseUnary())}; }\n"
        << "    return parsePrimary();\n"
        << "  }\n"
        << "  function parseProduct() {\n"
        << "    var left = parseUnary();\n"
        << "    while (atOp('*') || atOp('/')) {\n"
        << "      var op = cur().v; advance();\n"
        << "      var right = parseUnary();\n"
        << "      var a = asNumber(left), b = asNumber(right);\n"
        << "      if (op === '*') left = {kind:'num', value: a*b};\n"
        << "      else { if (b === 0) throw new Error('Division durch 0'); left = {kind:'num', value: a/b}; }\n"
        << "    }\n"
        << "    return left;\n"
        << "  }\n"
        << "  function parseSum() {\n"
        << "    var left = parseProduct();\n"
        << "    while (atOp('+') || atOp('-')) {\n"
        << "      var op = cur().v; advance();\n"
        << "      var right = parseProduct();\n"
        << "      if (op === '+') {\n"
        << "        if (left.kind === 'num' && right.kind === 'num') left = {kind:'num', value: left.value + right.value};\n"
        << "        else left = {kind:'text', value: toDisplay(left) + toDisplay(right)};\n"
        << "      } else {\n"
        << "        left = {kind:'num', value: asNumber(left) - asNumber(right)};\n"
        << "      }\n"
        << "    }\n"
        << "    return left;\n"
        << "  }\n"
        << "  function parseComparison() {\n"
        << "    var left = parseSum();\n"
        << "    if (atOp('==')||atOp('!=')||atOp('<')||atOp('>')||atOp('<=')||atOp('>=')) {\n"
        << "      var op = cur().v; advance();\n"
        << "      var right = parseSum();\n"
        << "      var result;\n"
        << "      if (left.kind === 'num' && right.kind === 'num') {\n"
        << "        var a=left.value,b=right.value;\n"
        << "        result = op==='=='?a===b:op==='!='?a!==b:op==='<'?a<b:op==='>'?a>b:op==='<='?a<=b:a>=b;\n"
        << "      } else if (left.kind==='bool' && right.kind==='bool') {\n"
        << "        if (op==='==') result = left.value===right.value;\n"
        << "        else if (op==='!=') result = left.value!==right.value;\n"
        << "        else throw new Error('Wahr/Falsch-Werte lassen sich nur mit == oder != vergleichen');\n"
        << "      } else {\n"
        << "        var sa=toDisplay(left), sb=toDisplay(right);\n"
        << "        var c = sa<sb?-1:(sa>sb?1:0);\n"
        << "        result = op==='=='?c===0:op==='!='?c!==0:op==='<'?c<0:op==='>'?c>0:op==='<='?c<=0:c>=0;\n"
        << "      }\n"
        << "      return {kind:'bool', value: result};\n"
        << "    }\n"
        << "    return left;\n"
        << "  }\n"
        << "  var result = parseComparison();\n"
        << "  if (cur().k !== 'end') throw new Error('unerwartetes Zeichen am Ende');\n"
        << "  return toDisplay(result);\n"
        << "}\n"
        << "function substituteTemplate(raw, slideId) {\n"
        << "  if (raw.indexOf('{') === -1) return raw;\n"
        << "  var out = '', i = 0, n = raw.length;\n"
        << "  while (i < n) {\n"
        << "    var c = raw[i];\n"
        << "    if (c === '{') {\n"
        << "      var close = raw.indexOf('}', i + 1);\n"
        << "      if (close < 0) { out += raw.slice(i); break; }\n"
        << "      var expr = raw.slice(i + 1, close);\n"
        << "      try { out += evalExpr(expr, slideId); }\n"
        << "      catch (err) { out += '⟨?' + expr.trim() + '?⟩'; }\n"
        << "      i = close + 1;\n"
        << "    } else { out += c; i++; }\n"
        << "  }\n"
        << "  return out;\n"
        << "}\n"
        << "function escapeHtml(s) { return s.replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;'); }\n"
        << "function setTemplateContent(el, text) { el.innerHTML = text.split('\\n').map(escapeHtml).join('<br>'); }\n"
        << "function currentSlideId() {\n"
        << "  var active = document.querySelector('.step.active');\n"
        << "  return active ? active.id : (steps.length ? steps[0].id : '');\n"
        << "}\n"
        << "function currentSlideNumber() {\n"
        << "  var id = currentSlideId();\n"
        << "  for (var i = 0; i < steps.length; i++) if (steps[i].id === id) return i + 1;\n"
        << "  return 0;\n"
        << "}\n"
        << "function totalSlideCount() { return steps.length; }\n"
        << "function renderAll() {\n"
        << "  var slideId = currentSlideId();\n"
        << "  document.querySelectorAll('[data-var-template]').forEach(function(el) {\n"
        << "    setTemplateContent(el, substituteTemplate(el.dataset.varTemplate, slideId));\n"
        << "  });\n"
        << "  document.querySelectorAll('[data-type=\"slider\"]').forEach(function(wrap) {\n"
        << "    var input = wrap.querySelector('input[type=\"range\"]');\n"
        << "    if (!input) return;\n"
        << "    var v = findVarById(input.dataset.varId);\n"
        << "    var num = v ? v.numberValue : parseFloat(input.min);\n"
        << "    input.value = num;\n"
        << "    var labelSpan = wrap.querySelector('[data-slider-label]');\n"
        << "    if (labelSpan) {\n"
        << "      var tmpl = labelSpan.dataset.sliderLabel || '';\n"
        << "      var labelText = tmpl ? substituteTemplate(tmpl, slideId) + '  ' : '';\n"
        << "      labelSpan.textContent = labelText + formatNum(num);\n"
        << "    }\n"
        << "  });\n"
        << "  document.querySelectorAll('[data-type=\"checkbox\"] input[type=\"checkbox\"]').forEach(function(input) {\n"
        << "    var v = findVarById(input.dataset.varId);\n"
        << "    input.checked = !!(v && v.boolValue);\n"
        << "  });\n"
        << "  document.querySelectorAll('[data-timeline]').forEach(function(el) {\n"
        << "    var data = parseTimeline(el);\n"
        << "    if (!data || !data.track.visibilityVarId) return;\n"
        << "    var v = findVarById(data.track.visibilityVarId);\n"
        << "    el.style.display = (v && v.boolValue) ? '' : 'none';\n"
        << "  });\n"
        << "  renderAllCharts();\n"
        << "  renderAllMeshGradients();\n"
        << "}\n"
        << "function onSliderInput(input) {\n"
        << "  var v = findVarById(input.dataset.varId);\n"
        << "  if (v) v.numberValue = parseFloat(input.value);\n"
        << "  renderAll();\n"
        << "}\n"
        << "function onCheckboxChange(input) {\n"
        << "  var v = findVarById(input.dataset.varId);\n"
        << "  if (v) v.boolValue = input.checked;\n"
        << "  renderAll();\n"
        << "}\n"
        << "function runButtonAction(btn) {\n"
        << "  var v = findVarById(btn.dataset.varId);\n"
        << "  if (!v) return;\n"
        << "  var op = btn.dataset.varOp;\n"
        << "  if (v.type === 1) {\n"
        << "    var amt = parseFloat(btn.dataset.varNumber || '0');\n"
        << "    if (op === 'inc') v.numberValue += amt;\n"
        << "    else if (op === 'dec') v.numberValue -= amt;\n"
        << "    else if (op === 'set') v.numberValue = amt;\n"
        << "  } else if (v.type === 2) {\n"
        << "    if (op === 'toggle') v.boolValue = !v.boolValue;\n"
        << "    else if (op === 'set') v.boolValue = (btn.dataset.varBool === 'true');\n"
        << "  } else {\n"
        << "    if (op === 'set') v.textValue = btn.dataset.varText || '';\n"
        << "  }\n"
        << "  renderAll();\n"
        << "}\n"
        << "\n"
        << "// ── Live chart rendering (data-driven types only: bar/bar_h/line/area/\n"
        << "// pie/donut/scatter) — mirrors src/rendering/ChartRenderer.cpp closely\n"
        << "// enough to look right, not pixel-for-pixel. Structural/special chart\n"
        << "// types (flowchart, gantt, venn, ...) stay a static image; see VARIABLEN_PLAN.md.\n"
        << "var CHART_PALETTE = [\"#4e79a7\",\"#f28e2b\",\"#e15759\",\"#76b7b2\",\"#59a14f\","
        << "\"#edc948\",\"#b07aa1\",\"#ff9da7\",\"#9c755f\",\"#bab0ac\"];\n"
        << "function chartColor(hex, idx) {\n"
        << "  if (hex) return hex;\n"
        << "  var i = ((idx % CHART_PALETTE.length) + CHART_PALETTE.length) % CHART_PALETTE.length;\n"
        << "  return CHART_PALETTE[i];\n"
        << "}\n"
        << "function hexToRgba(hex, alpha) {\n"
        << "  var h = (hex || '#4e79a7').replace('#', '');\n"
        << "  if (h.length === 3) h = h.split('').map(function(c) { return c + c; }).join('');\n"
        << "  var r = parseInt(h.substr(0,2),16), g = parseInt(h.substr(2,2),16), b = parseInt(h.substr(4,2),16);\n"
        << "  return 'rgba(' + r + ',' + g + ',' + b + ',' + alpha + ')';\n"
        << "}\n"
        << "function b64DecodeUtf8(b64) {\n"
        << "  var binary = atob(b64);\n"
        << "  var bytes = new Uint8Array(binary.length);\n"
        << "  for (var i = 0; i < binary.length; i++) bytes[i] = binary.charCodeAt(i);\n"
        << "  return new TextDecoder('utf-8').decode(bytes);\n"
        << "}\n"
        << "function resolveChartData(raw, slideId) {\n"
        << "  var d = JSON.parse(JSON.stringify(raw));\n"
        << "  d.title = substituteTemplate(d.title || '', slideId);\n"
        << "  d.labels = (d.labels || []).map(function(l) { return substituteTemplate(l, slideId); });\n"
        << "  (d.series || []).forEach(function(s) {\n"
        << "    s.name = substituteTemplate(s.name || '', slideId);\n"
        << "    var exprs = s.valueExprs || [];\n"
        << "    for (var i = 0; i < exprs.length && i < s.values.length; i++) {\n"
        << "      var expr = exprs[i];\n"
        << "      if (!expr || !expr.trim()) continue;\n"
        << "      var num = parseFloat(substituteTemplate(expr, slideId));\n"
        << "      if (!isNaN(num)) s.values[i] = num;\n"
        << "    }\n"
        << "  });\n"
        << "  return d;\n"
        << "}\n"
        << "function drawChartLegend(ctx, lx, ly, lw, lh, d, sc) {\n"
        << "  if (!d.series.length) return;\n"
        << "  var itemH = Math.max(14, 16*sc), boxSz = itemH * 0.7;\n"
        << "  ctx.font = Math.max(5, 9.5*sc) + 'px Arial';\n"
        << "  var y = ly + 4;\n"
        << "  for (var i = 0; i < d.series.length && y + itemH <= ly + lh; i++) {\n"
        << "    ctx.fillStyle = chartColor(d.series[i].color, i);\n"
        << "    ctx.fillRect(lx, y + (itemH-boxSz)/2, boxSz, boxSz);\n"
        << "    ctx.fillStyle = '#000';\n"
        << "    ctx.textAlign = 'left'; ctx.textBaseline = 'middle';\n"
        << "    ctx.fillText(d.series[i].name, lx + boxSz + 4, y + itemH/2);\n"
        << "    y += itemH + 2;\n"
        << "  }\n"
        << "}\n"
        << "function drawBarChart(ctx, w, h, d, sc, horiz) {\n"
        << "  var nCats = d.labels.length, nSer = d.series.length;\n"
        << "  if (!nSer || !nCats) return;\n"
        << "  var maxVal = 1;\n"
        << "  d.series.forEach(function(s) { s.values.forEach(function(v) { if (v > maxVal) maxVal = v; }); });\n"
        << "  var titleH = d.title ? 22*sc : 0;\n"
        << "  var legendW = (d.showLegend && nSer > 1) ? 90*sc : 0;\n"
        << "  var axisL = horiz ? 80*sc : 40*sc, axisB = horiz ? 30*sc : 28*sc, pad = 8*sc;\n"
        << "  if (d.title) {\n"
        << "    ctx.fillStyle = '#282828'; ctx.font = 'bold ' + Math.max(5,12*sc) + 'px Arial';\n"
        << "    ctx.textAlign = 'center'; ctx.textBaseline = 'top'; ctx.fillText(d.title, w/2, 0);\n"
        << "  }\n"
        << "  var plotX = axisL, plotY = titleH + pad;\n"
        << "  var plotW = w - axisL - legendW - pad, plotH = h - titleH - axisB - pad*2;\n"
        << "  if (plotW < 10 || plotH < 10) return;\n"
        << "  if (d.showGrid) {\n"
        << "    ctx.strokeStyle = 'rgb(210,210,210)'; ctx.lineWidth = 0.8*sc;\n"
        << "    for (var g = 0; g <= 5; g++) {\n"
        << "      ctx.beginPath();\n"
        << "      if (horiz) { var x = plotX+g*plotW/5; ctx.moveTo(x,plotY); ctx.lineTo(x,plotY+plotH); }\n"
        << "      else       { var y = plotY+(5-g)*plotH/5; ctx.moveTo(plotX,y); ctx.lineTo(plotX+plotW,y); }\n"
        << "      ctx.stroke();\n"
        << "    }\n"
        << "  }\n"
        << "  var catSize = (horiz ? plotH : plotW) / nCats;\n"
        << "  var barSize = catSize * 0.75 / nSer, barGap = catSize * 0.25 / 2;\n"
        << "  for (var cat = 0; cat < nCats; cat++) {\n"
        << "    for (var ser = 0; ser < nSer; ser++) {\n"
        << "      var s = d.series[ser];\n"
        << "      var val = cat < s.values.length ? s.values[cat] : 0;\n"
        << "      var barLen = val / maxVal * (horiz ? plotW : plotH);\n"
        << "      ctx.fillStyle = (s.valueColors && s.valueColors[cat]) ? s.valueColors[cat] : chartColor(s.color, ser);\n"
        << "      if (horiz) { var y2 = plotY+cat*catSize+barGap+ser*barSize; ctx.fillRect(plotX, y2, barLen, barSize-1.5*sc); }\n"
        << "      else       { var x2 = plotX+cat*catSize+barGap+ser*barSize; ctx.fillRect(x2, plotY+plotH-barLen, barSize-1.5*sc, barLen); }\n"
        << "    }\n"
        << "    ctx.fillStyle = '#505050'; ctx.font = Math.max(5,8.5*sc) + 'px Arial';\n"
        << "    var lbl = d.labels[cat] || String(cat+1);\n"
        << "    if (horiz) { ctx.textAlign='right'; ctx.textBaseline='middle'; ctx.fillText(lbl, axisL-4, plotY+cat*catSize+catSize/2); }\n"
        << "    else       { ctx.textAlign='center'; ctx.textBaseline='top'; ctx.fillText(lbl, plotX+cat*catSize+catSize/2, plotY+plotH+2); }\n"
        << "  }\n"
        << "  ctx.fillStyle = '#646464'; ctx.font = Math.max(5,8*sc) + 'px Arial';\n"
        << "  for (var g2 = 0; g2 <= 5; g2++) {\n"
        << "    var val2 = maxVal*g2/5;\n"
        << "    var lbl2 = val2>=10000 ? Math.round(val2/1000)+'k' : (val2>=1000 ? String(Math.round(val2/100)*100) : String(Math.round(val2)));\n"
        << "    if (horiz) { ctx.textAlign='center'; ctx.textBaseline='top'; ctx.fillText(lbl2, plotX+g2*plotW/5, plotY+plotH+2); }\n"
        << "    else       { ctx.textAlign='right'; ctx.textBaseline='middle'; ctx.fillText(lbl2, axisL-4, plotY+(5-g2)*plotH/5); }\n"
        << "  }\n"
        << "  ctx.strokeStyle = '#646464'; ctx.lineWidth = 1.2*sc;\n"
        << "  ctx.beginPath(); ctx.moveTo(plotX,plotY+plotH); ctx.lineTo(horiz?plotX+plotW:plotX, horiz?plotY+plotH:plotY); ctx.stroke();\n"
        << "  ctx.beginPath(); ctx.moveTo(plotX,plotY+plotH); ctx.lineTo(horiz?plotX:plotX+plotW, horiz?plotY:plotY+plotH); ctx.stroke();\n"
        << "  if (d.showLegend && nSer > 1) drawChartLegend(ctx, w-legendW, titleH+pad, legendW, h-titleH-pad, d, sc);\n"
        << "}\n"
        << "function drawLineChart(ctx, w, h, d, sc, filled) {\n"
        << "  var nCats = d.labels.length, nSer = d.series.length;\n"
        << "  if (!nSer || !nCats) return;\n"
        << "  var maxVal = 1;\n"
        << "  d.series.forEach(function(s) { s.values.forEach(function(v) { if (v > maxVal) maxVal = v; }); });\n"
        << "  var titleH = d.title ? 22*sc : 0;\n"
        << "  var legendW = (d.showLegend && nSer > 1) ? 90*sc : 0;\n"
        << "  var axisL = 40*sc, axisB = 28*sc, pad = 8*sc;\n"
        << "  if (d.title) {\n"
        << "    ctx.fillStyle='#282828'; ctx.font='bold '+Math.max(5,12*sc)+'px Arial';\n"
        << "    ctx.textAlign='center'; ctx.textBaseline='top'; ctx.fillText(d.title, w/2, 0);\n"
        << "  }\n"
        << "  var plotX = axisL, plotY = titleH+pad;\n"
        << "  var plotW = w-axisL-legendW-pad, plotH = h-titleH-axisB-pad*2;\n"
        << "  if (plotW<10||plotH<10) return;\n"
        << "  if (d.showGrid) {\n"
        << "    ctx.strokeStyle='rgb(210,210,210)'; ctx.lineWidth=0.8*sc;\n"
        << "    for (var g=0; g<=5; g++) { var y=plotY+(5-g)*plotH/5; ctx.beginPath(); ctx.moveTo(plotX,y); ctx.lineTo(plotX+plotW,y); ctx.stroke(); }\n"
        << "  }\n"
        << "  var stepX = nCats>1 ? plotW/(nCats-1) : 0;\n"
        << "  for (var ser=0; ser<nSer; ser++) {\n"
        << "    var s = d.series[ser];\n"
        << "    var c = chartColor(s.color, ser);\n"
        << "    var pts = [];\n"
        << "    for (var cat=0; cat<nCats; cat++) {\n"
        << "      var v = cat<s.values.length ? s.values[cat] : 0;\n"
        << "      pts.push([plotX+cat*stepX, plotY+plotH-(v/maxVal*plotH)]);\n"
        << "    }\n"
        << "    if (filled && pts.length>=2) {\n"
        << "      ctx.beginPath(); ctx.moveTo(pts[0][0], plotY+plotH);\n"
        << "      pts.forEach(function(pt){ ctx.lineTo(pt[0],pt[1]); });\n"
        << "      ctx.lineTo(pts[pts.length-1][0], plotY+plotH); ctx.closePath();\n"
        << "      ctx.fillStyle = hexToRgba(c, 0.3); ctx.fill();\n"
        << "    }\n"
        << "    ctx.strokeStyle=c; ctx.lineWidth=2*sc; ctx.lineCap='round'; ctx.lineJoin='round';\n"
        << "    ctx.beginPath(); pts.forEach(function(pt,i){ if(i===0) ctx.moveTo(pt[0],pt[1]); else ctx.lineTo(pt[0],pt[1]); }); ctx.stroke();\n"
        << "    var r = 3.5*sc;\n"
        << "    pts.forEach(function(pt) {\n"
        << "      ctx.fillStyle=c; ctx.beginPath(); ctx.arc(pt[0],pt[1],r,0,2*Math.PI); ctx.fill();\n"
        << "      ctx.strokeStyle='#fff'; ctx.lineWidth=1*sc; ctx.stroke();\n"
        << "    });\n"
        << "  }\n"
        << "  ctx.fillStyle='#505050'; ctx.font=Math.max(5,8.5*sc)+'px Arial';\n"
        << "  ctx.textAlign='center'; ctx.textBaseline='top';\n"
        << "  for (var cat2=0; cat2<nCats; cat2++)\n"
        << "    ctx.fillText(d.labels[cat2] || String(cat2+1), plotX+cat2*stepX, plotY+plotH+2);\n"
        << "  ctx.fillStyle='#646464'; ctx.font=Math.max(5,8*sc)+'px Arial';\n"
        << "  ctx.textAlign='right'; ctx.textBaseline='middle';\n"
        << "  for (var g2=0; g2<=5; g2++) {\n"
        << "    var val=maxVal*g2/5;\n"
        << "    var lbl2 = val>=1000 ? Math.round(val/1000)+'k' : String(Math.round(val));\n"
        << "    ctx.fillText(lbl2, axisL-4, plotY+plotH-g2*plotH/5);\n"
        << "  }\n"
        << "  ctx.strokeStyle='#646464'; ctx.lineWidth=1.2*sc;\n"
        << "  ctx.beginPath(); ctx.moveTo(plotX,plotY+plotH); ctx.lineTo(plotX+plotW,plotY+plotH); ctx.stroke();\n"
        << "  ctx.beginPath(); ctx.moveTo(plotX,plotY+plotH); ctx.lineTo(plotX,plotY); ctx.stroke();\n"
        << "  if (d.showLegend && nSer>1) drawChartLegend(ctx, w-legendW, titleH+pad, legendW, h-titleH-pad, d, sc);\n"
        << "}\n"
        << "function drawPieChart(ctx, w, h, d, sc, donut) {\n"
        << "  if (!d.series.length || !d.series[0].values.length) return;\n"
        << "  var s0 = d.series[0];\n"
        << "  var titleH = d.title ? 22*sc : 0, pad = 8*sc;\n"
        << "  if (d.title) {\n"
        << "    ctx.fillStyle='#282828'; ctx.font='bold '+Math.max(5,12*sc)+'px Arial';\n"
        << "    ctx.textAlign='center'; ctx.textBaseline='top'; ctx.fillText(d.title, w/2, 0);\n"
        << "  }\n"
        << "  var contentY = titleH+pad, contentH = h-titleH-pad;\n"
        << "  var nSlices = Math.min(s0.values.length, d.labels.length);\n"
        << "  var total = 0; for (var i=0;i<nSlices;i++) total += s0.values[i];\n"
        << "  if (total <= 0) return;\n"
        << "  var legendW = d.showLegend ? Math.min(120*sc, w*0.35) : 0;\n"
        << "  var pieAreaW = w - legendW;\n"
        << "  var side = Math.max(10, Math.min(pieAreaW, contentH) - 20*sc);\n"
        << "  var cx = pieAreaW/2, cy = contentY+contentH/2, radius = side/2;\n"
        << "  var angle = -Math.PI/2;\n"
        << "  for (var i2=0; i2<nSlices; i2++) {\n"
        << "    var frac = s0.values[i2]/total;\n"
        << "    var endAngle = angle + frac*2*Math.PI;\n"
        << "    ctx.beginPath(); ctx.moveTo(cx,cy); ctx.arc(cx,cy,radius,angle,endAngle); ctx.closePath();\n"
        << "    ctx.fillStyle = (s0.valueColors && s0.valueColors[i2]) ? s0.valueColors[i2] : chartColor('', i2);\n"
        << "    ctx.fill(); ctx.strokeStyle='#fff'; ctx.lineWidth=1.5*sc; ctx.stroke();\n"
        << "    angle = endAngle;\n"
        << "  }\n"
        << "  if (donut) {\n"
        << "    var holeR = side*0.38/2;\n"
        << "    ctx.beginPath(); ctx.arc(cx,cy,holeR,0,2*Math.PI); ctx.fillStyle='#fff'; ctx.fill();\n"
        << "  }\n"
        << "  if (d.showLegend) {\n"
        << "    var lx = pieAreaW+4, ly = contentY+4;\n"
        << "    var itemH = Math.max(14,16*sc), boxSz = itemH*0.7;\n"
        << "    ctx.font = Math.max(5,9*sc)+'px Arial';\n"
        << "    var y = ly;\n"
        << "    for (var i3=0; i3<nSlices && y+itemH<=contentY+contentH; i3++) {\n"
        << "      ctx.fillStyle = (s0.valueColors && s0.valueColors[i3]) ? s0.valueColors[i3] : chartColor('', i3);\n"
        << "      ctx.fillRect(lx, y+(itemH-boxSz)/2, boxSz, boxSz);\n"
        << "      ctx.fillStyle = '#3c3c3c'; ctx.textAlign='left'; ctx.textBaseline='middle';\n"
        << "      var pct = (s0.values[i3]/total*100).toFixed(1);\n"
        << "      ctx.fillText((d.labels[i3]||'') + ' ' + pct + '%', lx+boxSz+3, y+itemH/2);\n"
        << "      y += itemH+2;\n"
        << "    }\n"
        << "  }\n"
        << "}\n"
        << "function drawScatterChart(ctx, w, h, d, sc) {\n"
        << "  if (!d.series.length) return;\n"
        << "  var minX=0,maxX=1,minY=0,maxY=1,first=true;\n"
        << "  d.series.forEach(function(s) {\n"
        << "    for (var i=0;i+1<s.values.length;i+=2) {\n"
        << "      var x=s.values[i], y=s.values[i+1];\n"
        << "      if (first) { minX=maxX=x; minY=maxY=y; first=false; }\n"
        << "      if (x<minX) minX=x; if (x>maxX) maxX=x;\n"
        << "      if (y<minY) minY=y; if (y>maxY) maxY=y;\n"
        << "    }\n"
        << "  });\n"
        << "  if (maxX<=minX) maxX=minX+1;\n"
        << "  if (maxY<=minY) maxY=minY+1;\n"
        << "  var titleH = d.title?22*sc:0;\n"
        << "  var legendW = (d.showLegend && d.series.length>1) ? 90*sc : 0;\n"
        << "  var axisL=40*sc, axisB=28*sc, pad=8*sc;\n"
        << "  if (d.title) {\n"
        << "    ctx.fillStyle='#282828'; ctx.font='bold '+Math.max(5,12*sc)+'px Arial';\n"
        << "    ctx.textAlign='center'; ctx.textBaseline='top'; ctx.fillText(d.title, w/2, 0);\n"
        << "  }\n"
        << "  var plotX=axisL, plotY=titleH+pad;\n"
        << "  var plotW=w-axisL-legendW-pad, plotH=h-titleH-axisB-pad*2;\n"
        << "  if (plotW<10||plotH<10) return;\n"
        << "  if (d.showGrid) {\n"
        << "    ctx.strokeStyle='rgb(210,210,210)'; ctx.lineWidth=0.8*sc;\n"
        << "    for (var g=0; g<=5; g++) {\n"
        << "      var y=plotY+(5-g)*plotH/5;\n"
        << "      ctx.beginPath(); ctx.moveTo(plotX,y); ctx.lineTo(plotX+plotW,y); ctx.stroke();\n"
        << "      var x=plotX+g*plotW/5;\n"
        << "      ctx.beginPath(); ctx.moveTo(x,plotY); ctx.lineTo(x,plotY+plotH); ctx.stroke();\n"
        << "    }\n"
        << "  }\n"
        << "  d.series.forEach(function(s, ser) {\n"
        << "    ctx.fillStyle = chartColor(s.color, ser);\n"
        << "    for (var i=0;i+1<s.values.length;i+=2) {\n"
        << "      var px = plotX + (s.values[i]-minX)/(maxX-minX)*plotW;\n"
        << "      var py = plotY+plotH - (s.values[i+1]-minY)/(maxY-minY)*plotH;\n"
        << "      ctx.beginPath(); ctx.arc(px,py,4*sc,0,2*Math.PI); ctx.fill();\n"
        << "    }\n"
        << "  });\n"
        << "  ctx.strokeStyle='#646464'; ctx.lineWidth=1.2*sc;\n"
        << "  ctx.beginPath(); ctx.moveTo(plotX,plotY+plotH); ctx.lineTo(plotX+plotW,plotY+plotH); ctx.stroke();\n"
        << "  ctx.beginPath(); ctx.moveTo(plotX,plotY+plotH); ctx.lineTo(plotX,plotY); ctx.stroke();\n"
        << "  if (d.showLegend && d.series.length>1) drawChartLegend(ctx, w-legendW, titleH+pad, legendW, h-titleH-pad, d, sc);\n"
        << "}\n"
        << "function renderChartCanvas(canvas) {\n"
        << "  var wrap = canvas.parentElement;\n"
        << "  var cssW = wrap.clientWidth, cssH = wrap.clientHeight;\n"
        << "  if (cssW < 4 || cssH < 4) return;\n"
        << "  var dpr = window.devicePixelRatio || 1;\n"
        << "  canvas.width = Math.round(cssW*dpr); canvas.height = Math.round(cssH*dpr);\n"
        << "  var ctx = canvas.getContext('2d');\n"
        << "  ctx.setTransform(dpr, 0, 0, dpr, 0, 0);\n"
        << "  ctx.clearRect(0, 0, cssW, cssH);\n"
        << "  if (!wrap._chartRaw) {\n"
        << "    try { wrap._chartRaw = JSON.parse(b64DecodeUtf8(wrap.dataset.chart)); }\n"
        << "    catch (e) { return; }\n"
        << "  }\n"
        << "  var d = resolveChartData(wrap._chartRaw, currentSlideId());\n"
        << "  var sc = Math.max(0.4, Math.min(4, Math.min(cssW, cssH) / 350));\n"
        << "  var t = d.type;\n"
        << "  if      (t === 'bar')     drawBarChart(ctx, cssW, cssH, d, sc, false);\n"
        << "  else if (t === 'bar_h')   drawBarChart(ctx, cssW, cssH, d, sc, true);\n"
        << "  else if (t === 'line')    drawLineChart(ctx, cssW, cssH, d, sc, false);\n"
        << "  else if (t === 'area')    drawLineChart(ctx, cssW, cssH, d, sc, true);\n"
        << "  else if (t === 'pie')     drawPieChart(ctx, cssW, cssH, d, sc, false);\n"
        << "  else if (t === 'donut')   drawPieChart(ctx, cssW, cssH, d, sc, true);\n"
        << "  else if (t === 'scatter') drawScatterChart(ctx, cssW, cssH, d, sc);\n"
        << "}\n"
        << "function renderAllCharts() {\n"
        << "  document.querySelectorAll('.chart-canvas').forEach(renderChartCanvas);\n"
        << "}\n"
        << "window.addEventListener('resize', renderAllCharts);\n"
        << "\n"
        << "// ── Live mesh-gradient rendering (per-vertex-color triangle mesh) ─────────\n"
        << "// Ported from src/MeshGradientRenderer.cpp — keep the two in sync.\n"
        << "function meshTriArea2(a, b, c) { return (b.x-a.x)*(c.y-a.y) - (c.x-a.x)*(b.y-a.y); }\n"
        << "function meshCircumcircleContains(a, b, c, p) {\n"
        << "  var ax=a.x-p.x, ay=a.y-p.y, bx=b.x-p.x, by=b.y-p.y, cx=c.x-p.x, cy=c.y-p.y;\n"
        << "  var det = (ax*ax+ay*ay)*(bx*cy-cx*by) - (bx*bx+by*by)*(ax*cy-cx*ay) + (cx*cx+cy*cy)*(ax*by-bx*ay);\n"
        << "  return meshTriArea2(a,b,c) > 0 ? det > 0 : det < 0;\n"
        << "}\n"
        << "function meshTriangulate(ptsIn) {\n"
        << "  if (ptsIn.length < 3) return [];\n"
        << "  var minX=ptsIn[0].x, maxX=ptsIn[0].x, minY=ptsIn[0].y, maxY=ptsIn[0].y;\n"
        << "  ptsIn.forEach(function(p){ minX=Math.min(minX,p.x); maxX=Math.max(maxX,p.x); minY=Math.min(minY,p.y); maxY=Math.max(maxY,p.y); });\n"
        << "  var eps = Math.max(Math.hypot(maxX-minX, maxY-minY) * 1e-4, 1e-6);\n"
        << "  var pts = [];\n"
        << "  ptsIn.forEach(function(p) {\n"
        << "    for (var i=0;i<pts.length;i++) if (Math.hypot(p.x-pts[i].x, p.y-pts[i].y) < eps) return;\n"
        << "    pts.push(p);\n"
        << "  });\n"
        << "  if (pts.length < 3) return [];\n"
        << "  minX=pts[0].x; maxX=pts[0].x; minY=pts[0].y; maxY=pts[0].y;\n"
        << "  pts.forEach(function(p){ minX=Math.min(minX,p.x); maxX=Math.max(maxX,p.x); minY=Math.min(minY,p.y); maxY=Math.max(maxY,p.y); });\n"
        << "  var deltaMax = Math.max(maxX-minX, maxY-minY)*10 + 10;\n"
        << "  var midX=(minX+maxX)/2, midY=(minY+maxY)/2;\n"
        << "  var stA=pts.length, stB=stA+1, stC=stA+2;\n"
        << "  var work = pts.slice();\n"
        << "  work.push({x: midX-2*deltaMax, y: midY-deltaMax});\n"
        << "  work.push({x: midX, y: midY+2*deltaMax});\n"
        << "  work.push({x: midX+2*deltaMax, y: midY-deltaMax});\n"
        << "  var tris = [{a:stA,b:stB,c:stC}];\n"
        << "  function edgeEq(e1,e2){ return (e1.a===e2.a&&e1.b===e2.b)||(e1.a===e2.b&&e1.b===e2.a); }\n"
        << "  for (var pi=0; pi<pts.length; pi++) {\n"
        << "    var p = work[pi];\n"
        << "    var bad = tris.filter(function(t){ return meshCircumcircleContains(work[t.a],work[t.b],work[t.c],p); });\n"
        << "    var polygon = [];\n"
        << "    for (var i=0;i<bad.length;i++) {\n"
        << "      var edges = [{a:bad[i].a,b:bad[i].b},{a:bad[i].b,b:bad[i].c},{a:bad[i].c,b:bad[i].a}];\n"
        << "      edges.forEach(function(e) {\n"
        << "        var shared = 0;\n"
        << "        for (var j=0;j<bad.length;j++) {\n"
        << "          if (j===i) continue;\n"
        << "          var e2s = [{a:bad[j].a,b:bad[j].b},{a:bad[j].b,b:bad[j].c},{a:bad[j].c,b:bad[j].a}];\n"
        << "          for (var k=0;k<e2s.length;k++) if (edgeEq(e,e2s[k])) { shared++; break; }\n"
        << "        }\n"
        << "        if (shared===0) polygon.push(e);\n"
        << "      });\n"
        << "    }\n"
        << "    tris = tris.filter(function(t) { return !bad.some(function(b){ return b.a===t.a&&b.b===t.b&&b.c===t.c; }); });\n"
        << "    polygon.forEach(function(e){ tris.push({a:e.a, b:e.b, c:pi}); });\n"
        << "  }\n"
        << "  var result = [];\n"
        << "  tris.forEach(function(t) {\n"
        << "    if (t.a>=stA || t.b>=stA || t.c>=stA) return;\n"
        << "    if (Math.abs(meshTriArea2(pts[t.a],pts[t.b],pts[t.c])) < 1e-9) return;\n"
        << "    result.push(t);\n"
        << "  });\n"
        << "  return result;\n"
        << "}\n"
        << "function meshFillTriangle(data, width, height, p0, p1, p2) {\n"
        << "  var minX = Math.max(0, Math.floor(Math.min(p0.x,p1.x,p2.x)));\n"
        << "  var maxX = Math.min(width-1, Math.ceil(Math.max(p0.x,p1.x,p2.x)));\n"
        << "  var minY = Math.max(0, Math.floor(Math.min(p0.y,p1.y,p2.y)));\n"
        << "  var maxY = Math.min(height-1, Math.ceil(Math.max(p0.y,p1.y,p2.y)));\n"
        << "  if (minX > maxX || minY > maxY) return;\n"
        << "  var denom = (p1.y-p2.y)*(p0.x-p2.x) + (p2.x-p1.x)*(p0.y-p2.y);\n"
        << "  if (Math.abs(denom) < 1e-9) return;\n"
        << "  for (var y=minY; y<=maxY; y++) {\n"
        << "    var py = y + 0.5;\n"
        << "    for (var x=minX; x<=maxX; x++) {\n"
        << "      var px = x + 0.5;\n"
        << "      var w0 = ((p1.y-p2.y)*(px-p2.x) + (p2.x-p1.x)*(py-p2.y)) / denom;\n"
        << "      var w1 = ((p2.y-p0.y)*(px-p2.x) + (p0.x-p2.x)*(py-p2.y)) / denom;\n"
        << "      var w2 = 1 - w0 - w1;\n"
        << "      if (w0 < -1e-6 || w1 < -1e-6 || w2 < -1e-6) continue;\n"
        << "      var r = w0*p0.r + w1*p1.r + w2*p2.r;\n"
        << "      var g = w0*p0.g + w1*p1.g + w2*p2.g;\n"
        << "      var b = w0*p0.b + w1*p1.b + w2*p2.b;\n"
        << "      var a = Math.max(0, Math.min(1, w0*p0.a + w1*p1.a + w2*p2.a));\n"
        << "      var idx = (y*width + x) * 4;\n"
        << "      data[idx]   = Math.max(0, Math.min(255, r));\n"
        << "      data[idx+1] = Math.max(0, Math.min(255, g));\n"
        << "      data[idx+2] = Math.max(0, Math.min(255, b));\n"
        << "      data[idx+3] = Math.round(a*255);\n"
        << "    }\n"
        << "  }\n"
        << "}\n"
        << "function renderMeshCanvas(canvas) {\n"
        << "  var wrap = canvas.parentElement;\n"
        << "  var cssW = wrap.clientWidth, cssH = wrap.clientHeight;\n"
        << "  if (cssW < 4 || cssH < 4) return;\n"
        << "  var dpr = window.devicePixelRatio || 1;\n"
        << "  canvas.width = Math.max(1, Math.round(cssW*dpr));\n"
        << "  canvas.height = Math.max(1, Math.round(cssH*dpr));\n"
        << "  var ctx = canvas.getContext('2d');\n"
        << "  ctx.clearRect(0, 0, canvas.width, canvas.height);\n"
        << "  if (!wrap._meshPts) {\n"
        << "    try { wrap._meshPts = JSON.parse(b64DecodeUtf8(wrap.dataset.mesh)).points; }\n"
        << "    catch (e) { return; }\n"
        << "  }\n"
        << "  var raw = wrap._meshPts;\n"
        << "  if (!raw || raw.length < 3) return;\n"
        << "  var pts = raw.map(function(p) { return {x: p.x*canvas.width, y: p.y*canvas.height, r:p.r, g:p.g, b:p.b, a:p.a}; });\n"
        << "  var tris = meshTriangulate(pts);\n"
        << "  if (!tris.length) return;\n"
        << "  var img = ctx.createImageData(canvas.width, canvas.height);\n"
        << "  tris.forEach(function(t) { meshFillTriangle(img.data, canvas.width, canvas.height, pts[t.a], pts[t.b], pts[t.c]); });\n"
        << "  ctx.putImageData(img, 0, 0);\n"
        << "}\n"
        << "function renderAllMeshGradients() {\n"
        << "  document.querySelectorAll('.mesh-canvas').forEach(renderMeshCanvas);\n"
        << "}\n"
        << "window.addEventListener('resize', renderAllMeshGradients);\n"
        << "\n"
        << "// Build per-slide visibility map from data-vis-overrides attributes\n"
        << "var slideVisibility = {};\n"
        << "steps.forEach(function(step) {\n"
        << "  var raw = step.dataset.visOverrides || '';\n"
        << "  if (!raw) return;\n"
        << "  var map = {};\n"
        << "  raw.split(',').forEach(function(pair) {\n"
        << "    var kv = pair.split(':');\n"
        << "    if (kv.length === 2) map[kv[0].trim()] = parseFloat(kv[1]);\n"
        << "  });\n"
        << "  slideVisibility[step.id] = map;\n"
        << "});\n"
        << "\n"
        << "// Set initial opacities\n"
        << "// Inactive steps get pointer-events:none so they can't intercept clicks meant\n"
        << "// for the active slide's content (e.g. navigation buttons) even when a\n"
        << "// neighboring step's 3D-transformed box visually overlaps it on screen.\n"
        << "steps.forEach(function(step) { step.style.opacity = defaultInactiveOpacity; step.style.pointerEvents = 'none'; });\n"
        << "renderAll();\n"
        << "\n"
        << "document.addEventListener('impress:stepenter', function(e) {\n"
        << "  var activeId = e.target.id;\n"
        << "  var overrides = slideVisibility[activeId] || {};\n"
        << "  steps.forEach(function(step) {\n"
        << "    if (step.id === activeId) {\n"
        << "      step.style.opacity = '1';\n"
        << "      step.style.pointerEvents = 'auto';\n"
        << "    } else {\n"
        << "      var opa = (step.id in overrides) ? overrides[step.id] : defaultInactiveOpacity;\n"
        << "      step.style.opacity = String(opa);\n"
        << "      step.style.pointerEvents = 'none';\n"
        << "    }\n"
        << "  });\n"
        << "  var el = document.getElementById('slide-counter');\n"
        << "  var i = steps.indexOf(e.target) + 1;\n"
        << "  if (el) el.textContent = i + ' / ' + steps.length;\n"
        << "  renderAll();\n"
        << "});\n"
        << "\n"
        << "// ── Timeline animation player (entry/exit/loop/click-trigger/variable-gated visibility) ──\n"
        << "// Mirrors src/models/TimelineEngine.cpp's interpolation — keep both in sync.\n"
        << "// Per-element base property values (for exactly the keys used by its keyframes) are\n"
        << "// embedded alongside the override values in data-timeline at export time (see\n"
        << "// HtmlExporter::elementToHtml), so this player never needs to re-derive them from CSS.\n"
        << "function parseTimeline(el) {\n"
        << "  if (!el.dataset.timeline) return null;\n"
        << "  if (el._tl === undefined) {\n"
        << "    try { el._tl = JSON.parse(b64DecodeUtf8(el.dataset.timeline)); }\n"
        << "    catch (err) { el._tl = null; }\n"
        << "  }\n"
        << "  return el._tl;\n"
        << "}\n"
        << "function tlIsColor(k) { return k === 'color' || k === 'backgroundColor' || k === 'borderColor'; }\n"
        << "function tlHexToRgba(hex) {\n"
        << "  var h = hex.replace('#', '');\n"
        << "  if (h.length === 8) { // AARRGGBB\n"
        << "    return { a: parseInt(h.substr(0,2),16)/255, r: parseInt(h.substr(2,2),16), g: parseInt(h.substr(4,2),16), b: parseInt(h.substr(6,2),16) };\n"
        << "  }\n"
        << "  return { a: 1, r: parseInt(h.substr(0,2),16), g: parseInt(h.substr(2,2),16), b: parseInt(h.substr(4,2),16) };\n"
        << "}\n"
        << "function tlLerpColor(hexA, hexB, t) {\n"
        << "  var a = tlHexToRgba(hexA), b = tlHexToRgba(hexB);\n"
        << "  var r = Math.round(a.r + (b.r - a.r) * t), g = Math.round(a.g + (b.g - a.g) * t), bl = Math.round(a.b + (b.b - a.b) * t);\n"
        << "  var al = a.a + (b.a - a.a) * t;\n"
        << "  return 'rgba(' + r + ',' + g + ',' + bl + ',' + al.toFixed(3) + ')';\n"
        << "}\n"
        << "// hasEntry/hasExit with no keyframe authored yet (added by dragging the timeline bar\n"
        << "// directly, never opened via \"Start\\u25B8\"/\"\\u25C2End\") would otherwise interpolate\n"
        << "// against an empty override and never visibly change -- falls back to a plain opacity\n"
        << "// fade. Mirrors TimelineEngine.cpp's fadeKeyframeOrDefault -- keep both in sync.\n"
        << "function tlFadeKfOrDefault(kf) {\n"
        << "  return (kf && Object.keys(kf).length > 0) ? kf : { opacity: 0 };\n"
        << "}\n"
        << "// Applies `kf` (override values) interpolated against `base` (base values) at\n"
        << "// fraction t: t=0 -> kf values, t=1 -> base values (same convention as C++).\n"
        << "function tlApplyFrame(el, kf, base, t) {\n"
        << "  Object.keys(kf).forEach(function(key) {\n"
        << "    var ov = kf[key], bv = base[key];\n"
        << "    if (bv === undefined) return;\n"
        << "    if (tlIsColor(key)) {\n"
        << "      var css = tlLerpColor(ov, bv, t);\n"
        << "      if (key === 'color') el.style.color = css;\n"
        << "      else if (key === 'backgroundColor') el.style.backgroundColor = css;\n"
        << "      else if (key === 'borderColor') el.style.borderColor = css;\n"
        << "      return;\n"
        << "    }\n"
        << "    var v = ov + (bv - ov) * t;\n"
        << "    if (key === 'x') el.style.left = v + 'px';\n"
        << "    else if (key === 'y') el.style.top = v + 'px';\n"
        << "    else if (key === 'width') el.style.width = v + 'px';\n"
        << "    else if (key === 'height') el.style.height = v + 'px';\n"
        << "    else if (key === 'cornerRadius') el.style.borderRadius = v + 'px';\n"
        << "    else if (key === 'borderWidth') el.style.borderWidth = v + 'px';\n"
        << "    else if (key === 'fontSize') el.style.fontSize = v + 'px';\n"
        << "    else if (key === 'opacity') el.style.opacity = String(v);\n"
        << "    else if (key === 'rotation') el.style.transform = (v !== 0) ? ('rotate(' + v + 'deg)') : 'none';\n"
        << "  });\n"
        << "}\n"
        << "var timelineTimers = [];    // setTimeout ids for the currently active step, cleared on stepleave\n"
        << "var timelineClickQueue = []; // pending click-triggered fire-functions for the currently active step, in order\n"
        << "var timelineAutoDeadline = 0; // ms after stepenter by which all AUTO-triggered entry/exit should be done\n"
        << "var timelineStepEnterAt = 0;\n"
        << "function clearTimelineState() {\n"
        << "  timelineTimers.forEach(function(id) { clearTimeout(id); });\n"
        << "  timelineTimers = [];\n"
        << "  timelineClickQueue = [];\n"
        << "  timelineAutoDeadline = 0;\n"
        << "}\n"
        << "// Runs one element's entry -> hold -> exit -> (optional loop) sequence. Only the FIRST\n"
        << "// entry/exit cycle counts toward timelineAutoDeadline — later loop iterations run in the\n"
        << "// background so a looping element can never permanently block slide advancement.\n"
        << "function runElementTimeline(el, data, countsTowardDeadline) {\n"
        << "  var track = data.track, base = data.base;\n"
        << "  function schedule(hasSide, delay, duration, trigger, kf, tFrom, tTo, cb) {\n"
        << "    if (!hasSide) { cb(); return; }\n"
        << "    var fire = function() {\n"
        << "      if (duration > 0) {\n"
        << "        el.style.transitionDuration = duration + 's';\n"
        << "        requestAnimationFrame(function() { requestAnimationFrame(function() { tlApplyFrame(el, kf, base, tTo); }); });\n"
        << "        var doneId = setTimeout(cb, duration * 1000);\n"
        << "        timelineTimers.push(doneId);\n"
        << "      } else {\n"
        << "        el.style.transitionDuration = '0s';\n"
        << "        tlApplyFrame(el, kf, base, tTo);\n"
        << "        cb();\n"
        << "      }\n"
        << "    };\n"
        << "    // Reset any transition-duration left over from a previous loop iteration's\n"
        << "    // animated leg before this instantaneous jump to the wait-state frame — otherwise\n"
        << "    // the element smoothly (and wrongly) tweens back to it instead of snapping.\n"
        << "    el.style.transitionDuration = '0s';\n"
        << "    tlApplyFrame(el, kf, base, tFrom);\n"
        << "    // Click-gating only applies to the FIRST cycle (countsTowardDeadline): a\n"
        << "    // click-triggered stage inside a loop would otherwise re-enter\n"
        << "    // timelineClickQueue every iteration forever, permanently stealing every\n"
        << "    // future \"next\" press instead of ever letting the presentation advance —\n"
        << "    // repeat iterations fall through to the timed wait instead, using `delay`\n"
        << "    // as an ordinary pause.\n"
        << "    if (trigger === 'click' && countsTowardDeadline) {\n"
        << "      timelineClickQueue.push(fire);\n"
        << "    } else {\n"
        << "      // Measured from \"now\", not from stepenter: if this stage only started\n"
        << "      // once an earlier click-gated stage fired, elapsed time already spent\n"
        << "      // waiting for that click must count too, or \"next\" could be let through\n"
        << "      // before this stage — timed from stepenter as if it had started at 0 —\n"
        << "      // has actually finished.\n"
        << "      if (countsTowardDeadline)\n"
        << "        timelineAutoDeadline = Math.max(timelineAutoDeadline, (Date.now() - timelineStepEnterAt) + (delay + duration) * 1000);\n"
        << "      var id = setTimeout(fire, delay * 1000);\n"
        << "      timelineTimers.push(id);\n"
        << "    }\n"
        << "  }\n"
        << "  schedule(track.hasEntry, track.entryDelay, track.entryDuration, track.entryTrigger, tlFadeKfOrDefault(track.entryStart), 0, 1, function() {\n"
        << "    schedule(track.hasExit, track.exitDelay, track.exitDuration, track.exitTrigger, tlFadeKfOrDefault(track.exitEnd), 1, 0, function() {\n"
        << "      if (track.loop) {\n"
        << "        var id = setTimeout(function() { runElementTimeline(el, data, false); }, Math.max(0, track.loopPause) * 1000);\n"
        << "        timelineTimers.push(id);\n"
        << "      }\n"
        << "    });\n"
        << "  });\n"
        << "}\n"
        << "function timelineInitStep(step) {\n"
        << "  clearTimelineState();\n"
        << "  timelineStepEnterAt = Date.now();\n"
        << "  Array.from(step.querySelectorAll('[data-timeline]')).forEach(function(el) {\n"
        << "    var data = parseTimeline(el);\n"
        << "    if (data) runElementTimeline(el, data, true);\n"
        << "  });\n"
        << "}\n"
        << "document.addEventListener('impress:stepenter', function(e) { timelineInitStep(e.target); });\n"
        << "document.addEventListener('impress:stepleave', function() { clearTimelineState(); });\n"
        << "// Gate slide advancement: consume pending click-triggers one at a time (like the bundled\n"
        << "// substep plugin), and otherwise hold \"next\" until every auto-timed entry/exit is done.\n"
        << "impress.addPreStepLeavePlugin(function(event) {\n"
        << "  if (!event || !event.detail || event.detail.reason !== 'next') return;\n"
        << "  if (timelineClickQueue.length > 0) {\n"
        << "    var fire = timelineClickQueue.shift();\n"
        << "    fire();\n"
        << "    return false;\n"
        << "  }\n"
        << "  if (Date.now() - timelineStepEnterAt < timelineAutoDeadline) return false;\n"
        << "}, 1);\n"
        << "\n"
        << "// ── iframe \"portal\" workaround ──────────────────────────────────\n"
        << "// Browsers render <iframe> elements inside a CSS 3D-transformed\n"
        << "// ancestor (how impress.js positions every step) as a flattened,\n"
        << "// non-interactive image. We clone each iframe into a position:fixed\n"
        << "// element appended to <body> (outside the 3D context) and keep it\n"
        << "// aligned to the on-screen position of the original while active.\n"
        << "var portals = {};\n"
        << "function syncPortal(wrap) {\n"
        << "  var id = wrap.dataset.portalId;\n"
        << "  var srcIframe = wrap.querySelector('iframe');\n"
        << "  if (!id || !srcIframe) return;\n"
        << "  var rect = wrap.getBoundingClientRect();\n"
        << "  var portal = portals[id];\n"
        << "  if (!portal) {\n"
        << "    portal = document.createElement('iframe');\n"
        << "    portal.src = srcIframe.src;\n"
        << "    portal.allow = srcIframe.getAttribute('allow') || '';\n"
        << "    portal.allowFullscreen = true;\n"
        << "    portal.loading = 'lazy';\n"
        << "    portal.style.position = 'fixed';\n"
        << "    portal.style.border = 'none';\n"
        << "    portal.style.zIndex = '5000';\n"
        << "    document.body.appendChild(portal);\n"
        << "    portals[id] = portal;\n"
        << "  }\n"
        << "  portal.style.display = 'block';\n"
        << "  portal.style.left   = rect.left + 'px';\n"
        << "  portal.style.top    = rect.top + 'px';\n"
        << "  portal.style.width  = rect.width + 'px';\n"
        << "  portal.style.height = rect.height + 'px';\n"
        << "}\n"
        << "function activeIframeWraps() {\n"
        << "  var active = document.querySelector('.step.active');\n"
        << "  return active ? Array.from(active.querySelectorAll('[data-type=\"iframe-wrap\"]')) : [];\n"
        << "}\n"
        << "function updateActivePortals() {\n"
        << "  var activeWraps = activeIframeWraps();\n"
        << "  var activeIds = activeWraps.map(function(w) { return w.dataset.portalId; });\n"
        << "  Object.keys(portals).forEach(function(id) {\n"
        << "    if (activeIds.indexOf(id) === -1) portals[id].style.display = 'none';\n"
        << "  });\n"
        << "  activeWraps.forEach(syncPortal);\n"
        << "}\n"
        << "document.addEventListener('impress:stepenter', function() {\n"
        << "  var frames = 0;\n"
        << "  var raf = function() {\n"
        << "    updateActivePortals();\n"
        << "    frames++;\n"
        << "    if (frames < 80) requestAnimationFrame(raf);\n"
        << "  };\n"
        << "  requestAnimationFrame(raf);\n"
        << "});\n"
        << "window.addEventListener('resize', updateActivePortals);\n"
        << "\n"
        << "// ── Video/Audio playback ──────────────────────────────────────────\n"
        << "// data-autoplay=\"true\": play() is (re)triggered every time this element's\n"
        << "// slide becomes the active impress.js step, instead of the native HTML\n"
        << "// autoplay attribute — by the time any step beyond the first is reached the\n"
        << "// user has already interacted with the page (click/key to navigate), so\n"
        << "// browsers' autoplay-blocking policy doesn't apply. Elements without it rely\n"
        << "// solely on their own native controls / waveform play button.\n"
        << "document.addEventListener('impress:stepenter', function(e) {\n"
        << "  var media = e.target.querySelectorAll('video[data-autoplay=\"true\"], [data-type=\"audio\"][data-autoplay=\"true\"] audio');\n"
        << "  Array.prototype.forEach.call(media, function(m) { m.currentTime = 0; var p = m.play(); if (p && p.catch) p.catch(function(){}); });\n"
        << "});\n"
        << "document.addEventListener('impress:stepleave', function(e) {\n"
        << "  var media = e.target.querySelectorAll('video, [data-type=\"audio\"] audio');\n"
        << "  Array.prototype.forEach.call(media, function(m) { m.pause(); });\n"
        << "});\n"
        << "\n"
        << "// Waveform-mode audio: decode peaks once via Web Audio API, draw a\n"
        << "// WhatsApp-style bar waveform, and drive play/pause + progress from clicks\n"
        << "// on the pill itself (there's no native scrubber in this mode).\n"
        << "var mediaAudioCtx = null;\n"
        << "function initWaveformAudio(wrap) {\n"
        << "  var audio = wrap.querySelector('audio');\n"
        << "  var canvas = wrap.querySelector('.audio-wave');\n"
        << "  var timeEl = wrap.querySelector('.audio-time');\n"
        << "  if (!audio || !canvas) return;\n"
        << "  var peaks = null;\n"
        << "  function fmt(s) {\n"
        << "    if (!isFinite(s) || s < 0) s = 0;\n"
        << "    var m = Math.floor(s / 60), r = Math.floor(s % 60);\n"
        << "    return m + ':' + (r < 10 ? '0' : '') + r;\n"
        << "  }\n"
        << "  function draw() {\n"
        << "    if (!peaks || !canvas.width) return;\n"
        << "    var ctx = canvas.getContext('2d');\n"
        << "    var w = canvas.width, h = canvas.height;\n"
        << "    ctx.clearRect(0, 0, w, h);\n"
        << "    var barW = w / peaks.length;\n"
        << "    var progress = (audio.duration > 0) ? (audio.currentTime / audio.duration) : 0;\n"
        << "    for (var i = 0; i < peaks.length; i++) {\n"
        << "      var bh = Math.max(2, peaks[i] * h);\n"
        << "      var x = i * barW;\n"
        << "      ctx.fillStyle = (x / w <= progress) ? '#4ade80' : '#64748b';\n"
        << "      ctx.fillRect(x, (h - bh) / 2, Math.max(1, barW - 2), bh);\n"
        << "    }\n"
        << "  }\n"
        << "  function resize() {\n"
        << "    var rect = canvas.getBoundingClientRect();\n"
        << "    if (rect.width <= 0 || rect.height <= 0) return;\n"
        << "    canvas.width = Math.max(1, Math.round(rect.width));\n"
        << "    canvas.height = Math.max(1, Math.round(rect.height));\n"
        << "    draw();\n"
        << "  }\n"
        << "  window.addEventListener('resize', resize);\n"
        << "  audio.addEventListener('loadedmetadata', function() { timeEl.textContent = fmt(audio.duration); resize(); });\n"
        << "  audio.addEventListener('timeupdate', function() {\n"
        << "    timeEl.textContent = fmt(audio.duration - audio.currentTime);\n"
        << "    draw();\n"
        << "  });\n"
        << "  audio.addEventListener('play',  function() { wrap.classList.add('playing'); });\n"
        << "  audio.addEventListener('pause', function() { wrap.classList.remove('playing'); });\n"
        << "  audio.addEventListener('ended', function() { wrap.classList.remove('playing'); draw(); });\n"
        << "  wrap.addEventListener('click', function() {\n"
        << "    if (audio.paused) { var p = audio.play(); if (p && p.catch) p.catch(function(){}); } else audio.pause();\n"
        << "  });\n"
        << "  fetch(audio.currentSrc || audio.src).then(function(r) { return r.arrayBuffer(); }).then(function(buf) {\n"
        << "    if (!mediaAudioCtx) mediaAudioCtx = new (window.AudioContext || window.webkitAudioContext)();\n"
        << "    return mediaAudioCtx.decodeAudioData(buf);\n"
        << "  }).then(function(decoded) {\n"
        << "    var data = decoded.getChannelData(0);\n"
        << "    var barCount = 46;\n"
        << "    var step = Math.max(1, Math.floor(data.length / barCount));\n"
        << "    peaks = [];\n"
        << "    for (var i = 0; i < barCount; i++) {\n"
        << "      var maxAmp = 0;\n"
        << "      var end = Math.min(data.length, (i + 1) * step);\n"
        << "      for (var j = i * step; j < end; j++) maxAmp = Math.max(maxAmp, Math.abs(data[j]));\n"
        << "      peaks.push(maxAmp);\n"
        << "    }\n"
        << "    resize();\n"
        << "  }).catch(function() {});\n"
        << "}\n"
        << "Array.prototype.forEach.call(document.querySelectorAll('[data-type=\"audio\"][data-audio-mode=\"waveform\"]'), initWaveformAudio);\n"
        << "\n"
        << "// Compact-mode audio: same click-to-play/pause icon toggle as the\n"
        << "// waveform pill, just without amplitude bars (a static filename label\n"
        << "// takes the canvas's place — see the Audio branch in HtmlExporter).\n"
        << "function initCompactAudio(wrap) {\n"
        << "  var audio = wrap.querySelector('audio');\n"
        << "  if (!audio) return;\n"
        << "  audio.addEventListener('play',  function() { wrap.classList.add('playing'); });\n"
        << "  audio.addEventListener('pause', function() { wrap.classList.remove('playing'); });\n"
        << "  audio.addEventListener('ended', function() { wrap.classList.remove('playing'); });\n"
        << "  wrap.addEventListener('click', function() {\n"
        << "    if (audio.paused) { var p = audio.play(); if (p && p.catch) p.catch(function(){}); } else audio.pause();\n"
        << "  });\n"
        << "}\n"
        << "Array.prototype.forEach.call(document.querySelectorAll('[data-type=\"audio\"][data-audio-mode=\"compact\"]'), initCompactAudio);\n"
        << "\n"
        << "document.addEventListener('keydown', function(e) {\n"
        << "  if ((e.key === 'f' || e.key === 'F') && !e.ctrlKey && !e.metaKey) {\n"
        << "    if (!document.fullscreenElement) {\n"
        << "      document.documentElement.requestFullscreen();\n"
        << "    } else {\n"
        << "      document.exitFullscreen();\n"
        << "    }\n"
        << "  }\n"
        << "});\n"
        << "</script>\n"
        << "<div id=\"slide-counter\" style=\"position:fixed;bottom:20px;right:24px;"
        << "font-family:sans-serif;font-size:14px;color:rgba(255,255,255,0.5);"
        << "z-index:9999;pointer-events:none;\">1 / "
        << pres.slides.size() << "</div>\n"
        << "</body>\n"
        << "</html>\n";

    return html;
}

QString HtmlExporter::slideToHtml(const Slide& s, int index, int slideCount,
                                   const QMap<QString, QString>& uuidToHtmlId,
                                   const QMap<QString, QString>& uuidToVisString,
                                   const VariableSet& vars) {
    QString html;
    QTextStream out(&html);

    QString bg = colorToCss(s.backgroundColor);
    QString slideId = QString("slide-%1").arg(index);

    QString stepStyle = "background:" + bg + ";";
    if (s.slideWidth > 0 && s.slideHeight > 0)
        stepStyle += QString("width:%1px;height:%2px;").arg(int(s.slideWidth)).arg(int(s.slideHeight));

    // Camera center = slide position + view offset
    int camX = int(s.posX + s.viewOffsetX);
    int camY = int(-(s.posY + s.viewOffsetY));

    out << "  <div class=\"step\"\n"
        << "       id=\""             << slideId << "\"\n"
        << "       data-name=\""      << s.name.toHtmlEscaped() << "\"\n"
        << "       data-x=\""         << camX   << "\"\n"
        << "       data-y=\""         << camY   << "\"\n"
        << "       data-z=\""         << int(s.posZ) << "\"\n"
        << "       data-rotate-x=\""  << s.rotX << "\"\n"
        << "       data-rotate-y=\""  << s.rotY << "\"\n"
        << "       data-rotate=\""    << s.rotZ << "\"\n"
        << "       data-scale=\""     << s.scale << "\"\n"
        << "       data-view-offset-x=\"" << s.viewOffsetX << "\"\n"
        << "       data-view-offset-y=\"" << s.viewOffsetY << "\"\n";

    // Per-slide visibility overrides (written as data-vis-overrides="id:opa,id:opa")
    if (uuidToVisString.contains(s.id))
        out << "       data-vis-overrides=\"" << uuidToVisString[s.id] << "\"\n";

    // Editor-only ruler guides — no visual/CSS effect, purely round-tripped so
    // HtmlImporter can restore them. "v:<x>"/"h:<y>" for lines, "c:<cx>:<cy>:<r>" for circles.
    if (!s.guides.isEmpty() || !s.guideCircles.isEmpty()) {
        QStringList tokens;
        for (const GuideLine& g : s.guides)
            tokens << QString("%1:%2").arg(g.vertical ? "v" : "h").arg(double(g.pos), 0, 'f', 2);
        for (const GuideCircle& c : s.guideCircles)
            tokens << QString("c:%1:%2:%3").arg(double(c.cx), 0, 'f', 2)
                                            .arg(double(c.cy), 0, 'f', 2)
                                            .arg(double(c.radius), 0, 'f', 2);
        out << "       data-guides=\"" << tokens.join(',') << "\"\n";
    }

    out << "       style=\""          << stepStyle << "\">\n";

    for (const auto& elem : s.elements)
        out << "    " << elementToHtml(elem, uuidToHtmlId, vars, s.id, index, slideCount) << "\n";

    out << "  </div>";
    return html;
}

QString HtmlExporter::elementToHtml(const SlideElement& e,
                                    const QMap<QString, QString>& uuidToHtmlId,
                                    const VariableSet& vars,
                                    const QString& currentSlideId,
                                    int slideNumber, int slideCount) {
    QString base = QString("position:absolute;left:%1px;top:%2px;width:%3px;height:%4px;")
                       .arg(int(e.x)).arg(int(e.y)).arg(int(e.width)).arg(int(e.height));
    if (e.rotation != 0.f)
        base += QString("transform:rotate(%1deg);transform-origin:center;")
                    .arg(double(e.rotation), 0, 'f', 2);
    if (e.opacity < 0.999f)
        base += QString("opacity:%1;").arg(double(e.opacity), 0, 'f', 3);

    // Timeline animation (entry/exit/loop/trigger/variable-gated visibility),
    // consumed by the TimelinePlayer JS registered in HtmlExporter::generateHtml().
    // Alongside the keyframe overrides, embed the element's *base* values for
    // exactly the properties its keyframes touch, so the JS player can
    // interpolate without re-deriving numbers back out of rendered CSS.
    QString timelineAttr;
    if (!e.timeline.isDefault()) {
        QStringList keys = e.timeline.entryStart.props.keys();
        for (const QString& k : e.timeline.exitEnd.props.keys())
            if (!keys.contains(k)) keys << k;
        // hasEntry/hasExit with an empty keyframe falls back to a plain opacity
        // fade at playback time (tlFadeKfOrDefault in the JS below) — make sure
        // "opacity" is embedded as a base value for that fallback to have
        // something to interpolate against, same as if it were a real keyframe key.
        if ((e.timeline.hasEntry && e.timeline.entryStart.isEmpty()) ||
            (e.timeline.hasExit  && e.timeline.exitEnd.isEmpty()))
            if (!keys.contains("opacity")) keys << "opacity";
        QJsonObject payload;
        payload["track"] = e.timeline.toJson();
        payload["base"]  = TimelineEngine::baseSnapshot(e, keys);
        QByteArray tlJson = QJsonDocument(payload).toJson(QJsonDocument::Compact);
        timelineAttr = QString(" data-timeline=\"%1\"").arg(QString::fromLatin1(tlJson.toBase64()));
    }

    // Editor-only grouping metadata (see FEATURES_TODO.md "Gruppenbildung") —
    // no visual/CSS effect, purely round-tripped so HtmlImporter can
    // reconstruct which elements were grouped together in the editor.
    // Appended to timelineAttr since every type branch below already splices
    // that string into its returned tag's attribute list.
    if (!e.groupId.isEmpty())
        timelineAttr += QString(" data-group=\"%1\"").arg(e.groupId.toHtmlEscaped());

    if (e.type == SlideElement::Text) {
        QString justifyContent = "flex-start";
        if (e.verticalAlignment == "middle") justifyContent = "center";
        else if (e.verticalAlignment == "bottom") justifyContent = "flex-end";

        QString style = base
            + QString("font-family:'%1';font-size:%2px;color:%3;")
                  .arg(e.fontFamily).arg(e.fontSize).arg(colorToCss(e.color))
            + QString("text-align:%1;overflow:hidden;word-wrap:break-word;line-height:1.2;")
                  .arg(e.textAlignment)
            + QString("display:flex;flex-direction:column;justify-content:%1;").arg(justifyContent);
        if (e.bold)          style += "font-weight:bold;";
        if (e.italic)        style += "font-style:italic;";
        if (e.backgroundColor != Qt::transparent)
            style += "background:" + colorToCss(e.backgroundColor) + ";";
        QStringList decos;
        if (e.underline)     decos << "underline";
        if (e.strikethrough) decos << "line-through";
        if (!decos.isEmpty()) {
            style += "text-decoration:" + decos.join(' ') + ";";
            if (e.underline) {
                QColor ulCol = e.underlineColor.isValid() ? e.underlineColor : e.color;
                style += "text-decoration-color:" + colorToCss(ulCol) + ";";
                static const char* ulStyles[] = {"solid","dashed","dotted","wavy"};
                style += QString("text-decoration-style:%1;")
                             .arg(ulStyles[qBound(0, e.underlineStyle, 3)]);
            }
        }

        if (e.listStyle != SlideElement::NoList) {
            QString listTag  = (e.listStyle == SlideElement::Bullets) ? "ul" : "ol";
            QString listType = (e.listStyle == SlideElement::Bullets) ? "disc" : "decimal";
            QString dataList = (e.listStyle == SlideElement::Bullets) ? "bullets" : "numbered";
            QString items;
            for (const QString& line : e.content.split('\n'))
                items += "<li>" + line.toHtmlEscaped() + "</li>";
            return QString("<div data-type=\"text\" data-list=\"%1\"%2 style=\"%3\">"
                           "<%4 style=\"margin:0;padding:0 0 0 1.3em;list-style:%5;\">%6</%4></div>")
                       .arg(dataList, timelineAttr, style, listTag, listType, items);
        }

        QString text = e.codeSpans.isEmpty()
                           ? e.content.toHtmlEscaped().replace("\n", "<br>")
                           : buildTextWithCodeSpans(e);
        // Live {var} substitution at presentation time: only for plain text (no
        // hyperlink wrapper to rebuild, no list markup, no code spans) — see
        // VARIABLEN_PLAN.md. Code spans are mutually exclusive with {var} templating
        // since the runtime rebuild would overwrite the <code> markup.
        QString varAttr = (e.hyperlink.trimmed().isEmpty() && e.content.contains('{') && e.codeSpans.isEmpty())
                               ? QString(" data-var-template=\"%1\"").arg(e.content.toHtmlEscaped())
                               : QString();
        if (!e.hyperlink.trimmed().isEmpty()) {
            QString href = e.hyperlink.trimmed().toHtmlEscaped();
            text = QString("<a href=\"%1\" target=\"_blank\" rel=\"noopener noreferrer\" "
                           "style=\"color:inherit;text-decoration:inherit;\">%2</a>")
                       .arg(href, text);
        }
        return QString("<div data-type=\"text\"%1%2 style=\"%3\">%4</div>")
                   .arg(varAttr, timelineAttr, style, text);

    } else if (e.type == SlideElement::Shape) {
        QString style = base + "background:" + colorToCss(e.backgroundColor) + ";";
        if (e.borderWidth > 0)
            style += QString("border:%1px solid %2;")
                         .arg(int(e.borderWidth)).arg(colorToCss(e.borderColor));
        {
            QString css = ShapeUtils::shapeToCssStyle(e.content);
            if (!css.isEmpty())
                style += css + ";";
            else if (e.cornerRadius > 0)
                style += QString("border-radius:%1px;").arg(int(e.cornerRadius));
        }

        // Mesh gradient: embed points as base64 JSON (mirrors data-chart) —
        // this blob is also the sole persistence of the mesh points across
        // save/reopen, since the exported HTML *is* the project file. The
        // live <canvas> is clipped by the exact same CSS (clip-path above,
        // or overflow:hidden+border-radius) that clips the plain-color
        // fallback, so the two can never visually drift apart.
        QString meshAttr, meshCanvas;
        if (e.useMeshGradient && e.meshGradient.isUsable()) {
            QByteArray meshJson = QJsonDocument(e.meshGradient.toJson()).toJson(QJsonDocument::Compact);
            meshAttr = QString(" data-mesh=\"%1\"").arg(QString::fromLatin1(meshJson.toBase64()));
            style += "overflow:hidden;";
            meshCanvas = "<canvas class=\"mesh-canvas\" style=\"position:absolute;inset:0;"
                         "width:100%;height:100%;display:block;pointer-events:none;\"></canvas>";
        }

        // Inner text overlay
        QString innerText;
        if (!e.shapeText.isEmpty()) {
            QString textStyle = QString(
                "position:absolute;inset:0;display:flex;align-items:center;"
                "justify-content:center;text-align:center;padding:4px;"
                "font-family:'%1';font-size:%2px;color:%3;"
                "word-wrap:break-word;overflow:hidden;pointer-events:none;")
                    .arg(e.fontFamily).arg(e.fontSize).arg(colorToCss(e.color));
            if (e.bold)   textStyle += "font-weight:bold;";
            if (e.italic) textStyle += "font-style:italic;";
            QString varAttr = e.shapeText.contains('{')
                                   ? QString(" data-var-template=\"%1\"").arg(e.shapeText.toHtmlEscaped())
                                   : QString();
            innerText = QString("<span%1 style=\"%2\">%3</span>")
                            .arg(varAttr, textStyle, e.shapeText.toHtmlEscaped().replace("\n", "<br>"));
        }
        return QString("<div data-type=\"shape\" data-shape=\"%1\"%2%3 style=\"%4\">%5%6</div>")
                   .arg(e.content, timelineAttr, meshAttr, style, meshCanvas, innerText);

    } else if (e.type == SlideElement::Icon) {
        int iw = qMax(4, int(e.width));
        int ih = qMax(4, int(e.height));
        QImage img(iw, ih, QImage::Format_ARGB32);
        img.fill(Qt::transparent);
        QPainter painter(&img);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setPen(Qt::NoPen);
        painter.setBrush(e.color.isValid() ? e.color : Qt::black);
        painter.drawPath(IconUtils::iconToPath(e.content, QRectF(0, 0, iw, ih)));
        painter.end();

        QByteArray imgBa;
        QBuffer buf(&imgBa);
        buf.open(QIODevice::WriteOnly);
        img.save(&buf, "PNG");
        QString b64img = imgBa.toBase64();
        QString iconStyle = base + "color:" + colorToCss(e.color.isValid() ? e.color : Qt::black) + ";";

        return QString(
            "<div data-type=\"icon\" data-icon=\"%1\"%2 style=\"%3\">"
            "<img src=\"data:image/png;base64,%4\" "
            "style=\"width:100%;height:100%;object-fit:contain;\" alt=\"\">"
            "</div>")
            .arg(e.content, timelineAttr, iconStyle, b64img);

    } else if (e.type == SlideElement::Image) {
        QFileInfo fi(e.content);
        QString src = fi.exists() ? "assets/" + fi.fileName() : e.content;
        return QString("<img data-type=\"image\" src=\"%1\"%2 style=\"%3object-fit:contain;\" alt=\"\">")
                   .arg(src, timelineAttr, base);

    } else if (e.type == SlideElement::Table) {
        QString tableStyle = base
            + QString("font-family:'%1';font-size:%2px;overflow:hidden;")
                  .arg(e.tableFontFamily).arg(e.tableFontSize);

        QString borderStyle = e.tableBorderWidth > 0
            ? QString("%1px solid %2")
                  .arg(int(e.tableBorderWidth))
                  .arg(colorToCss(e.tableBorderColor))
            : "none";

        QStringList colWidths;
        for (double f : e.tableColFracs)
            colWidths << QString::number(f * 100.0, 'f', 4) + "%";

        QStringList rowHeights;
        for (double f : e.tableRowFracs)
            rowHeights << QString::number(f * e.height, 'f', 0) + "px";

        QString html = QString("<div data-type=\"table\"%1 style=\"%2\">").arg(timelineAttr, tableStyle);
        html += QString("<table style=\"width:100%;height:100%;border-collapse:collapse;"
                        "table-layout:fixed;border:%1;\">").arg(borderStyle);

        html += "<colgroup>";
        for (const QString& w : colWidths)
            html += QString("<col style=\"width:%1;\">").arg(w);
        html += "</colgroup>";

        for (int r = 0; r < e.tableRows && r < e.tableCells.size(); ++r) {
            const QString rowH = (r < rowHeights.size()) ? rowHeights[r] : "auto";
            html += QString("<tr style=\"height:%1;\">").arg(rowH);

            for (int c = 0; c < e.tableCols && c < e.tableCells[r].size(); ++c) {
                const TableCell& cell = e.tableCells[r][c];

                if (cell.merged) continue; // covered by a spanning cell

                QColor bg = cell.bgColor.isValid() ? cell.bgColor : e.tableDefaultBg;
                QColor fg = cell.textColor.isValid() ? cell.textColor : e.tableDefaultText;

                if (r == 0 && e.tableHasHeader) {
                    if (!cell.bgColor.isValid() && e.tableHeaderBg.isValid())
                        bg = e.tableHeaderBg;
                    if (!cell.textColor.isValid() && e.tableHeaderText.isValid())
                        fg = e.tableHeaderText;
                }

                QString cellStyle = "padding:4px 6px;overflow:hidden;"
                                    "word-wrap:break-word;vertical-align:middle;";
                cellStyle += "background:" + colorToCss(bg) + ";";
                cellStyle += "color:" + colorToCss(fg) + ";";
                cellStyle += "border:" + borderStyle + ";";

                QString align = cell.textAlign.isEmpty() ? "left" : cell.textAlign;
                cellStyle += "text-align:" + align + ";";

                if (cell.bold || (r == 0 && e.tableHasHeader)) cellStyle += "font-weight:bold;";
                if (cell.italic)                                 cellStyle += "font-style:italic;";

                QString tag = (r == 0 && e.tableHasHeader) ? "th" : "td";

                // colspan / rowspan attributes
                QString spanAttrs;
                int cs = qMax(1, qMin(cell.colspan, e.tableCols - c));
                int rs = qMax(1, qMin(cell.rowspan, e.tableRows - r));
                if (cs > 1) spanAttrs += QString(" colspan=\"%1\"").arg(cs);
                if (rs > 1) spanAttrs += QString(" rowspan=\"%1\"").arg(rs);
                if (cell.text.contains('{'))
                    spanAttrs += QString(" data-var-template=\"%1\"").arg(cell.text.toHtmlEscaped());

                html += QString("<%1%2 style=\"%3\">%4</%1>")
                            .arg(tag, spanAttrs, cellStyle,
                                 cell.text.toHtmlEscaped().replace("\n", "<br>"));
            }
            html += "</tr>";
        }

        html += "</table></div>";
        return html;

    } else if (e.type == SlideElement::Chart) {
        // Embed the full chart data as base64-encoded JSON — used both for
        // lossless reload (HtmlImporter) and, for data-driven chart types,
        // by the JS canvas renderer so the chart redraws live when a bound
        // variable changes (see VARIABLEN_PLAN.md).
        QByteArray jsonBa = QJsonDocument(e.chartData.toJson()).toJson(QJsonDocument::Compact);
        QString chartDataB64 = jsonBa.toBase64();

        if (e.chartData.isDataChart()) {
            return QString(
                "<div data-type=\"chart\" data-chart-type=\"%1\" data-chart=\"%2\"%3 style=\"%4\">"
                "<canvas class=\"chart-canvas\" style=\"width:100%;height:100%;display:block;\"></canvas>"
                "</div>")
                .arg(e.chartData.type, chartDataB64, timelineAttr, base);
        }

        // Structural/special chart types (flowchart, mindmap, orgchart, uml,
        // timeline, gantt, venn): rendered as a static image baked at export
        // time — no JS reimplementation of these layouts, so no live updates.
        int iw = qMax(4, int(e.width));
        int ih = qMax(4, int(e.height));
        QImage img(iw, ih, QImage::Format_ARGB32);
        img.fill(Qt::white);
        QPainter painter(&img);
        painter.setRenderHint(QPainter::Antialiasing);
        ChartRenderer::paint(painter, QRectF(0, 0, iw, ih), e.chartData, &vars, currentSlideId,
                             slideNumber, slideCount);
        painter.end();

        QByteArray imgBa;
        QBuffer buf(&imgBa);
        buf.open(QIODevice::WriteOnly);
        img.save(&buf, "PNG");

        QString b64img = imgBa.toBase64();
        QString title  = e.chartData.title.isEmpty() ? ChartRenderer::typeName(e.chartData.type)
                                                      : e.chartData.title;
        return QString(
            "<div data-type=\"chart\" data-chart-type=\"%1\" data-chart=\"%2\"%3 style=\"%4\">"
            "<img src=\"data:image/png;base64,%5\" "
            "style=\"width:100%;height:100%;object-fit:contain;\" alt=\"%6\">"
            "</div>")
            .arg(e.chartData.type, chartDataB64, timelineAttr, base, b64img, title.toHtmlEscaped());

    } else if (e.type == SlideElement::Formula) {
        QString style = base
            + QString("display:flex;align-items:center;justify-content:center;"
                       "overflow:hidden;font-size:%1px;").arg(e.fontSize);
        if (e.color.isValid())
            style += "color:" + colorToCss(e.color) + ";";
        if (e.backgroundColor != Qt::transparent)
            style += "background:" + colorToCss(e.backgroundColor) + ";";

        QString latex = e.content.toHtmlEscaped();
        return QString(
            "<div data-type=\"formula\" data-latex=\"%1\"%2 style=\"%3\">"
            "<div class=\"math\">$$%4$$</div></div>")
            .arg(latex, timelineAttr, style, latex);

    } else if (e.type == SlideElement::IFrame) {
        if (e.content.trimmed().isEmpty()) return {};
        QString url = e.content.toHtmlEscaped();
        // data-portal-id: browsers render iframes inside a CSS 3D-transformed
        // ancestor (how impress.js positions every step) as a flattened,
        // non-interactive image. The exported JS "portals" a live clone of
        // this iframe to a position:fixed element outside the 3D context
        // while its slide is active, keyed by this id.
        return QString(
            "<div data-type=\"iframe-wrap\" data-portal-id=\"%3\"%4 style=\"%1overflow:hidden;\">"
            "<iframe data-type=\"iframe\" src=\"%2\" "
            "style=\"position:absolute;inset:0;width:100%;height:100%;border:none;\" "
            "allow=\"accelerometer; autoplay; clipboard-write; encrypted-media; gyroscope; picture-in-picture\" "
            "allowfullscreen loading=\"lazy\"></iframe>"
            "<a href=\"%2\" target=\"_blank\" rel=\"noopener\" "
            "style=\"position:absolute;top:4px;right:4px;z-index:2;background:rgba(17,17,17,.75);"
            "color:#fff;font:11px sans-serif;padding:3px 7px;border-radius:4px;text-decoration:none;\" "
            "title=\"If the page blocks embedding: open here in a new tab\">↗ Open</a>"
            "</div>")
            .arg(base, url, e.id, timelineAttr);

    } else if (e.type == SlideElement::Video) {
        if (e.content.trimmed().isEmpty()) return {};
        QFileInfo fi(e.content);
        QString src = fi.exists() ? "assets/" + fi.fileName() : e.content;
        // autoplay is driven by JS on impress:stepenter (see generateHtml), not the
        // native autoplay attribute — by the time a non-first slide is reached a
        // user gesture has already happened, so play() isn't blocked, and this
        // way the video also (re)starts every time its slide is re-entered.
        QString autoAttr = e.mediaAutoplay ? " data-autoplay=\"true\"" : "";
        return QString(
            "<video data-type=\"video\"%1%2 src=\"%3\" controls playsinline "
            "style=\"%4object-fit:contain;background:#000;\"></video>")
            .arg(timelineAttr, autoAttr, src, base);

    } else if (e.type == SlideElement::Audio) {
        if (e.content.trimmed().isEmpty()) return {};
        QFileInfo fi(e.content);
        QString src = fi.exists() ? "assets/" + fi.fileName() : e.content;
        QString autoAttr = e.mediaAutoplay ? " data-autoplay=\"true\"" : "";
        QString audioBg = colorToCss(e.backgroundColor.isValid() && e.backgroundColor != Qt::transparent
                                      ? e.backgroundColor : QColor(30, 41, 59));
        QString audioFg = colorToCss(e.color.isValid() ? e.color : QColor(203, 213, 225));

        if (!e.audioShowWaveform) {
            // Same pill shell as the waveform player below (icon + click-to-play,
            // see initCompactAudio in the script), but with the filename as a
            // static label instead of a canvas — matches the editor's Audio
            // branch, which shows the same icon+filename look for this mode.
            QString label = QFileInfo(src).fileName().toHtmlEscaped();
            return QString(
                "<div data-type=\"audio\" data-audio-mode=\"compact\"%1%2 style=\"%3"
                "display:flex;align-items:center;gap:10px;background:%5;color:%6;border-radius:999px;"
                "padding:0 14px;cursor:pointer;box-sizing:border-box;overflow:hidden;\">"
                "<audio src=\"%4\" preload=\"auto\" style=\"display:none;\"></audio>"
                "<span class=\"audio-playbtn\">"
                "<span class=\"ico-play\"></span>"
                "<span class=\"ico-pause\"><span></span><span></span></span>"
                "</span>"
                "<span class=\"audio-label\">%7</span>"
                "</div>")
                .arg(timelineAttr, autoAttr, base, src, audioBg, audioFg, label);
        }

        // WhatsApp-style waveform player: the bars/progress/play-pause icon are
        // drawn and driven entirely by the JS in generateHtml() (initWaveformAudio) —
        // amplitude peaks are decoded client-side from the file via Web Audio API,
        // there's no per-sample data baked in at export time. Background/text
        // color are per-element (matches the editor's Audio rendering); text
        // color is set on the wrapper div (not the .audio-time span directly)
        // so it round-trips through the same "style=... left/top/background/
        // color" attribute the importer already reads for every other type —
        // .audio-time has no color of its own, so it just inherits it.
        return QString(
            "<div data-type=\"audio\" data-audio-mode=\"waveform\"%1%2 style=\"%3"
            "display:flex;align-items:center;gap:10px;background:%5;color:%6;border-radius:999px;"
            "padding:0 14px;cursor:pointer;box-sizing:border-box;overflow:hidden;\">"
            "<audio src=\"%4\" preload=\"auto\" style=\"display:none;\"></audio>"
            "<span class=\"audio-playbtn\">"
            "<span class=\"ico-play\"></span>"
            "<span class=\"ico-pause\"><span></span><span></span></span>"
            "</span>"
            "<canvas class=\"audio-wave\" style=\"flex:1 1 auto;height:60%;display:block;\"></canvas>"
            "<span class=\"audio-time\">0:00</span>"
            "</div>")
            .arg(timelineAttr, autoAttr, base, src, audioBg, audioFg);

    } else if (e.type == SlideElement::Button) {
        QString style = base
            + QString("display:flex;align-items:center;justify-content:center;"
                       "font-family:'%1';font-size:%2px;text-align:center;"
                       "cursor:pointer;user-select:none;text-decoration:none;overflow:hidden;box-sizing:border-box;")
                  .arg(e.fontFamily).arg(e.fontSize);
        style += "color:" + colorToCss(e.color.isValid() ? e.color : Qt::white) + ";";
        style += "background:" + colorToCss(e.backgroundColor.isValid() && e.backgroundColor != Qt::transparent
                                             ? e.backgroundColor : QColor(37, 99, 235)) + ";";
        if (e.borderWidth > 0)
            style += QString("border:%1px solid %2;")
                         .arg(int(e.borderWidth)).arg(colorToCss(e.borderColor));
        if (e.cornerRadius > 0)
            style += QString("border-radius:%1px;").arg(int(e.cornerRadius));
        if (e.bold) style += "font-weight:bold;";

        QString rawLabel = e.content.isEmpty() ? "Next" : e.content;
        QString label    = rawLabel.toHtmlEscaped();
        QString varAttr  = rawLabel.contains('{')
                                ? QString(" data-var-template=\"%1\"").arg(label)
                                : QString();

        if (e.buttonAction == "changeVariable") {
            QString attrs = QString(" data-var-id=\"%1\" data-var-op=\"%2\" data-var-number=\"%3\" "
                                    "data-var-bool=\"%4\" data-var-text=\"%5\"")
                                 .arg(e.boundVariableId, e.varOp)
                                 .arg(e.varOpNumber, 0, 'f', 4)
                                 .arg(e.varOpBool ? "true" : "false", e.varOpText.toHtmlEscaped());
            return QString("<a data-type=\"button\"%1%2%5 href=\"javascript:void(0)\" "
                           "onclick=\"runButtonAction(this);return false;\" style=\"%3\">%4</a>")
                       .arg(varAttr, attrs, style, label, timelineAttr);
        }

        QString targetHtmlId = uuidToHtmlId.value(e.targetSlideId);
        if (targetHtmlId.isEmpty())
            return QString("<div data-type=\"button\"%1%4 style=\"%2opacity:.5;\">%3</div>")
                       .arg(varAttr, style, label, timelineAttr);

        return QString("<a data-type=\"button\"%1%5 data-target=\"%2\" href=\"#%2\" "
                       "onclick=\"api.goto('%2');return false;\" style=\"%3\">%4</a>")
                   .arg(varAttr, targetHtmlId, style, label, timelineAttr);

    } else if (e.type == SlideElement::Checkbox) {
        QString style = base
            + QString("display:flex;align-items:center;gap:%1px;cursor:pointer;user-select:none;"
                       "font-family:'%2';font-size:%3px;color:%4;overflow:hidden;box-sizing:border-box;")
                  .arg(qMax(4, e.fontSize / 3)).arg(e.fontFamily).arg(e.fontSize)
                  .arg(colorToCss(e.color.isValid() ? e.color : Qt::black));

        QString rawLabel = e.content;
        QString varAttr  = rawLabel.contains('{')
                                ? QString(" data-var-template=\"%1\"").arg(rawLabel.toHtmlEscaped())
                                : QString();

        return QString(
            "<label data-type=\"checkbox\"%5 style=\"%1\">"
            "<input type=\"checkbox\" data-var-id=\"%2\" onchange=\"onCheckboxChange(this)\" "
            "style=\"width:1.3em;height:1.3em;flex:none;\">"
            "<span%3>%4</span>"
            "</label>")
            .arg(style, e.boundVariableId, varAttr, rawLabel.toHtmlEscaped(), timelineAttr);

    } else if (e.type == SlideElement::Slider) {
        QString style = base
            + QString("display:flex;flex-direction:column;justify-content:center;gap:6px;"
                       "font-family:'%1';font-size:%2px;color:%3;overflow:hidden;box-sizing:border-box;")
                  .arg(e.fontFamily).arg(e.fontSize).arg(colorToCss(e.color.isValid() ? e.color : Qt::black));

        QString sliderLabelAttr = e.content.isEmpty()
                                      ? QString()
                                      : QString(" data-slider-label=\"%1\"").arg(e.content.toHtmlEscaped());

        return QString(
            "<div data-type=\"slider\"%7 style=\"%1\">"
            "<span%2></span>"
            "<input type=\"range\" data-var-id=\"%3\" min=\"%4\" max=\"%5\" step=\"%6\" "
            "oninput=\"onSliderInput(this)\" style=\"width:100%;\">"
            "</div>")
            .arg(style, sliderLabelAttr, e.boundVariableId)
            .arg(e.sliderMin, 0, 'f', 4).arg(e.sliderMax, 0, 'f', 4).arg(e.sliderStep, 0, 'f', 4)
            .arg(timelineAttr);
    }
    return {};
}

QString HtmlExporter::colorToCss(const QColor& c) {
    if (!c.isValid() || c == Qt::transparent) return "transparent";
    if (c.alpha() == 255)
        return QString("#%1%2%3")
                   .arg(c.red(),  2, 16, QChar('0'))
                   .arg(c.green(),2, 16, QChar('0'))
                   .arg(c.blue(), 2, 16, QChar('0'));
    return QString("rgba(%1,%2,%3,%4)")
               .arg(c.red()).arg(c.green()).arg(c.blue())
               .arg(QString::number(c.alphaF(), 'f', 2));
}

// Despite the name (kept from when only Image existed), this also copies
// Video/Audio source files — all three reference their file the same way
// (elem.content = absolute path, exported as "assets/<filename>").
bool HtmlExporter::copyImages(const Presentation& pres,
                               const QString& assetsDir, QStringList& errors) {
    bool ok = true;
    for (const auto& slide : pres.slides) {
        for (const auto& elem : slide.elements) {
            bool isMediaFile = elem.type == SlideElement::Image
                             || elem.type == SlideElement::Video
                             || elem.type == SlideElement::Audio;
            if (!isMediaFile || elem.content.isEmpty()) continue;
            QFileInfo fi(elem.content);
            if (!fi.exists()) {
                errors << "Not found: " + elem.content; ok = false; continue;
            }
            QString dest = QDir(assetsDir).filePath(fi.fileName());
            if (!QFile::exists(dest) && !QFile::copy(elem.content, dest)) {
                errors << "Copy failed: " + elem.content; ok = false;
            }
        }
    }
    return ok;
}

// Copies each WorldObject's model file into its own assets/models/<id>/
// subfolder (a per-object subfolder avoids filename collisions between
// different glTF files that happen to share a sidecar name like "scene.bin").
// For .gltf files, any buffer/image URIs referenced by the file are copied
// alongside it too (data: URIs are embedded and skipped); .glb files are a
// single self-contained blob and need only the one copy.
bool HtmlExporter::copyModels(const Presentation& pres,
                               const QString& assetsDir, QStringList& errors) {
    bool ok = true;
    for (const auto& w : pres.worldObjects) {
        if (w.modelPath.isEmpty()) continue;
        QFileInfo fi(w.modelPath);
        if (!fi.exists()) {
            errors << "Not found: " + w.modelPath; ok = false; continue;
        }
        QString destDir = QDir(assetsDir).filePath("models/" + w.id);
        QDir().mkpath(destDir);
        QString destFile = QDir(destDir).filePath(fi.fileName());
        if (!QFile::exists(destFile) && !QFile::copy(w.modelPath, destFile)) {
            errors << "Copy failed: " + w.modelPath; ok = false; continue;
        }

        if (fi.suffix().compare("gltf", Qt::CaseInsensitive) != 0) continue;

        tinygltf::TinyGLTF loader;
        tinygltf::Model model;
        std::string gltfErr, gltfWarn;
        if (!loader.LoadASCIIFromFile(&model, &gltfErr, &gltfWarn, w.modelPath.toStdString()))
            continue; // already reported as a load error elsewhere; nothing more to copy

        auto copyUri = [&](const std::string& uriStd) {
            if (uriStd.empty()) return;
            QString uri = QString::fromStdString(uriStd);
            if (uri.startsWith("data:")) return; // embedded, nothing to copy
            QString decoded = QUrl::fromPercentEncoding(uri.toUtf8());
            QString srcPath = QDir(fi.absolutePath()).filePath(decoded);
            QFileInfo srcFi(srcPath);
            if (!srcFi.exists()) {
                errors << "Not found: " + srcPath; ok = false; return;
            }
            QString dstPath = QDir(destDir).filePath(decoded);
            QDir().mkpath(QFileInfo(dstPath).absolutePath());
            if (!QFile::exists(dstPath) && !QFile::copy(srcPath, dstPath)) {
                errors << "Copy failed: " + srcPath; ok = false;
            }
        };
        for (const auto& buf : model.buffers) copyUri(buf.uri);
        for (const auto& img : model.images)  copyUri(img.uri);
    }
    return ok;
}

// Removes files under assets/ that no longer correspond to any image or
// world-object model in the presentation. Re-exporting to the same output
// folder only ever adds files (copyImages/copyModels skip existing
// destinations), so without this pass, images deleted in the editor would
// linger in assets/ forever across re-exports.
void HtmlExporter::cleanupOrphanedAssets(const Presentation& pres,
                                          const QString& assetsDir) {
    QSet<QString> liveImageNames;
    for (const auto& slide : pres.slides)
        for (const auto& elem : slide.elements) {
            bool isMediaFile = elem.type == SlideElement::Image
                             || elem.type == SlideElement::Video
                             || elem.type == SlideElement::Audio;
            if (isMediaFile && !elem.content.isEmpty())
                liveImageNames.insert(QFileInfo(elem.content).fileName());
        }

    QSet<QString> liveModelIds;
    for (const auto& w : pres.worldObjects)
        if (!w.modelPath.isEmpty())
            liveModelIds.insert(w.id);

    QDir dir(assetsDir);
    for (const QFileInfo& fi : dir.entryInfoList(QDir::Files))
        if (!liveImageNames.contains(fi.fileName()))
            QFile::remove(fi.absoluteFilePath());

    QDir modelsDir(dir.filePath("models"));
    if (modelsDir.exists()) {
        for (const QFileInfo& fi : modelsDir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot))
            if (!liveModelIds.contains(fi.fileName()))
                QDir(fi.absoluteFilePath()).removeRecursively();
    }
}

// WorldObjects are emitted as plain (non-.step) sibling divs inside #impress
// — impress.js's init() moves ALL of #impress's direct children into the
// shared navigation canvas, so this div rides the same camera transform as
// the slides without impress.js needing to know about it. Since it's outside
// impress.js's own per-step transform machinery, the transform is written
// out literally here; the Y-axis negation mirrors slideToHtml()'s data-y
// convention so the object sits in the same coordinate space as the slides.
// position:absolute is required here: every .step sibling gets it (via
// initStep()'s JS), and mixing this one static box into a canvas of
// otherwise-absolute siblings shifts its in-flow layout position, which
// showed up as the object rendering lower in the browser than in the
// editor's OpenGL preview.
QString HtmlExporter::worldObjectToHtml(const WorldObject& w) {
    QString html;
    QTextStream out(&html);

    QFileInfo fi(w.modelPath);
    QString src = "assets/models/" + w.id + "/" + fi.fileName();

    QString xform = QString(
        "translate(-50%,-50%) translate3d(%1px,%2px,%3px) "
        "rotateX(%4deg) rotateY(%5deg) rotateZ(%6deg) scale(%7)")
        .arg(w.posX).arg(-w.posY).arg(w.posZ)
        .arg(w.rotX).arg(w.rotY).arg(w.rotZ)
        .arg(w.scale);

    out << "  <div class=\"world-object\" data-wid=\"" << w.id << "\"\n"
        << "       style=\"position:absolute;transform:" << xform << ";opacity:" << w.opacity
        << ";width:600px;height:600px;transform-style:preserve-3d;\">\n"
        << "    <model-viewer src=\"" << src.toHtmlEscaped() << "\"\n"
        << "                  camera-controls=\"false\" auto-rotate=\"false\" disable-zoom\n"
        << "                  interaction-prompt=\"none\" style=\"width:100%;height:100%;\">\n"
        << "    </model-viewer>\n"
        << "  </div>";
    return html;
}
