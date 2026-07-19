#include "HtmlImporter.h"
#include "models/ChartData.h"
#include "models/TimelineTrack.h"
#include <QFile>
#include <QDir>
#include <QMap>
#include <QTextStream>
#include <QRegularExpression>
#include <QUuid>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

// ── Internal helpers ──────────────────────────────────────────────────────────

// Extract value of attribute attr="..." from an HTML tag string
static QString attrVal(const QString& src, const QString& attr) {
    QRegularExpression re(QRegularExpression::escape(attr) + "=\"([^\"]*)\"");
    auto m = re.match(src);
    return m.hasMatch() ? m.captured(1) : QString();
}

// Extract value of a CSS property from an inline style string
static QString cssProp(const QString& style, const QString& prop) {
    QRegularExpression re(QRegularExpression::escape(prop) + R"(:\s*([^;]+))");
    auto m = re.match(style);
    return m.hasMatch() ? m.captured(1).trimmed() : QString();
}

// Undo HtmlExporter's text encoding for one plain-text segment: <br> → \n,
// then HTML entity unescape. Used both for plain text and for the inner
// text of <code> spans — see the Text-element import block below.
static QString unescapeTextSegment(QString s) {
    s.replace("<br>", "\n");
    s.replace("&amp;", "&").replace("&lt;", "<")
     .replace("&gt;", ">").replace("&quot;", "\"")
     .replace("&#39;", "'");
    return s;
}

// Parse a CSS color string to QColor
static QColor parseCssColor(const QString& raw) {
    QString c = raw.trimmed();
    if (c.isEmpty() || c == "transparent") return Qt::transparent;
    if (c.startsWith('#')) { QColor col(c); return col.isValid() ? col : Qt::transparent; }
    QRegularExpression re(R"(rgba?\(\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)(?:\s*,\s*([\d.]+))?\s*\))");
    auto m = re.match(c);
    if (m.hasMatch()) {
        int r = m.captured(1).toInt(), g = m.captured(2).toInt(), b = m.captured(3).toInt();
        float a = m.captured(4).isEmpty() ? 1.f : m.captured(4).toFloat();
        return QColor(r, g, b, int(a * 255.f));
    }
    QColor col(c);
    return col.isValid() ? col : Qt::transparent;
}

// Restores SlideElement::timeline and ::opacity from the data-timeline
// attribute (base64 JSON, {"track":{...},"base":{...}} — see
// HtmlExporter::elementToHtml) and the generic CSS opacity property.
// Applies to every element type, so every per-type parse branch calls this
// once its `style` string and element tag are known.
static void parseTimelineAndOpacity(const QString& tag, const QString& style, SlideElement& e) {
    QString opa = cssProp(style, "opacity");
    if (!opa.isEmpty()) e.opacity = opa.toFloat();

    // Editor-only grouping metadata (see FEATURES_TODO.md "Gruppenbildung" and
    // HtmlExporter::elementToHtml's data-group) — no rendering effect, purely
    // round-tripped so elements dragged/deleted/styled together in the editor
    // stay grouped across a save+reopen. Element ids regenerate on every
    // import (see below), but matching data-group strings among siblings is
    // enough to reconstruct membership.
    e.groupId = attrVal(tag, "data-group");

    QString tlB64 = attrVal(tag, "data-timeline");
    if (tlB64.isEmpty()) return;
    QJsonDocument doc = QJsonDocument::fromJson(QByteArray::fromBase64(tlB64.toLatin1()));
    if (doc.isObject())
        e.timeline = TimelineTrack::fromJson(doc.object()["track"].toObject());
}

// Finds the "var presVariables = [...];" JSON array embedded in the bootstrap
// <script> by HtmlExporter::buildVariablesJson() and parses it back into
// Presentation::variables. Uses a manual bracket-depth scan (like
// extractStepBlocks below) rather than a regex, so a "]" inside a text
// variable's value can't be mistaken for the end of the array.
static void parseVariablesJson(const QString& html, Presentation* pres,
                               const QMap<QString, QString>& htmlIdToUuid) {
    int markerPos = html.indexOf("var presVariables = ");
    if (markerPos < 0) return;
    int arrStart = html.indexOf('[', markerPos);
    if (arrStart < 0) return;

    int depth = 0, i = arrStart;
    bool inString = false;
    int arrEnd = -1;
    while (i < html.size()) {
        QChar c = html[i];
        if (inString) {
            if (c == '\\') { i += 2; continue; } // skip escaped character
            if (c == '"') inString = false;
        } else {
            if (c == '"') inString = true;
            else if (c == '[') ++depth;
            else if (c == ']') { --depth; if (depth == 0) { arrEnd = i; break; } }
        }
        ++i;
    }
    if (arrEnd < 0) return;

    // JSON's "\/" escape (used at export time so a value can't close </script>)
    // is decoded back to "/" by QJsonDocument automatically.
    QByteArray jsonBa = html.mid(arrStart, arrEnd - arrStart + 1).toUtf8();
    QJsonParseError perr;
    QJsonDocument doc = QJsonDocument::fromJson(jsonBa, &perr);
    if (perr.error != QJsonParseError::NoError || !doc.isArray()) return;

    for (const auto& item : doc.array()) {
        QJsonObject o = item.toObject();
        Variable v;
        v.id          = o["id"].toString();
        v.name        = o["name"].toString();
        v.type        = Variable::Type(o["type"].toInt(int(Variable::Text)));
        v.textValue   = o["textValue"].toString();
        v.numberValue = o["numberValue"].toDouble();
        v.boolValue   = o["boolValue"].toBool();
        QString scopeHtmlId = o["scopeSlideId"].toString();
        v.scopeSlideId = scopeHtmlId.isEmpty() ? QString() : htmlIdToUuid.value(scopeHtmlId);
        pres->variables.items << v;
    }
}

// Extract all <div class="step" ...>...</div> blocks from HTML (improved version)
// Now more robust to malformed HTML and nested structures
static QStringList extractStepBlocks(const QString& html) {
    QStringList out;
    int pos = 0;
    while (true) {
        // Find the opening <div class="step"
        int start = html.indexOf("<div class=\"step\"", pos);
        if (start < 0) break;
        
        // Find the closing > of the opening tag
        int openingTagEnd = html.indexOf('>', start);
        if (openingTagEnd < 0) {
            pos = start + 1;
            continue;  // Malformed opening tag, skip
        }
        
        // Now look for the matching closing </div>
        int depth = 0, i = start;
        bool foundClosing = false;
        
        while (i < html.size()) {
            // Look for <div tag (must be followed by space, >, or newline)
            if (i + 4 <= html.size() && html.mid(i, 4) == "<div") {
                // Check if it's a real tag opening
                if (i + 4 < html.size()) {
                    QChar next = html[i + 4];
                    if (next == ' ' || next == '>' || next == '\n' || next == '\t') {
                        ++depth;
                        i += 4;
                        continue;
                    }
                }
            }
            // Look for </div> closing
            if (i + 6 <= html.size() && html.mid(i, 6) == "</div>") {
                --depth;
                if (depth <= 0) {
                    out.append(html.mid(start, i + 6 - start));
                    pos = i + 6;
                    foundClosing = true;
                    break;
                }
                i += 6;
                continue;
            }
            ++i;
        }
        
        if (!foundClosing) {
            // Could not find matching closing tag, stop processing
            break;
        }
    }
    return out;
}

// Same bracket-depth <div>/</div> matching as extractStepBlocks(), generalized
// to any class name — used for "world-object" divs, which sit as siblings of
// .step divs rather than nested inside them.
static QStringList extractDivBlocksByClass(const QString& html, const QString& className) {
    QStringList out;
    QString marker = "<div class=\"" + className + "\"";
    int pos = 0;
    while (true) {
        int start = html.indexOf(marker, pos);
        if (start < 0) break;

        int openingTagEnd = html.indexOf('>', start);
        if (openingTagEnd < 0) { pos = start + 1; continue; }

        int depth = 0, i = start;
        bool foundClosing = false;
        while (i < html.size()) {
            if (i + 4 <= html.size() && html.mid(i, 4) == "<div") {
                if (i + 4 < html.size()) {
                    QChar next = html[i + 4];
                    if (next == ' ' || next == '>' || next == '\n' || next == '\t') {
                        ++depth; i += 4; continue;
                    }
                }
            }
            if (i + 6 <= html.size() && html.mid(i, 6) == "</div>") {
                --depth;
                if (depth <= 0) {
                    out.append(html.mid(start, i + 6 - start));
                    pos = i + 6;
                    foundClosing = true;
                    break;
                }
                i += 6; continue;
            }
            ++i;
        }
        if (!foundClosing) break;
    }
    return out;
}

// Read a CSS block for a given selector, e.g. "body" or ".step"
static QString cssBlock(const QString& css, const QString& selector) {
    QRegularExpression re(QRegularExpression::escape(selector) +
                          R"(\s*\{([^}]*)\})",
                          QRegularExpression::DotMatchesEverythingOption);
    auto m = re.match(css);
    return m.hasMatch() ? m.captured(1) : QString();
}

// ── Table element parser ──────────────────────────────────────────────────────
// The exporter writes each table element as a single long line:
//   <div data-type="table" style="..."><table ...><colgroup>...</colgroup><tr ...><td ...>text</td></tr>...</table></div>
static SlideElement parseTableDiv(const QString& line) {
    SlideElement e;
    e.type = SlideElement::Table;

    // Outer div: position / size / font
    int divEnd = line.indexOf('>');
    if (divEnd < 0) return e;
    QString divTag    = line.left(divEnd + 1);
    QString outerSt   = attrVal(divTag, "style");
    e.x      = cssProp(outerSt, "left"  ).remove("px").toFloat();
    e.y      = cssProp(outerSt, "top"   ).remove("px").toFloat();
    e.width  = cssProp(outerSt, "width" ).remove("px").toFloat();
    e.height = cssProp(outerSt, "height").remove("px").toFloat();
    {
        QString ff = cssProp(outerSt, "font-family"); ff.remove('\'').remove('"');
        if (!ff.isEmpty()) e.tableFontFamily = ff;
        QString fs = cssProp(outerSt, "font-size").remove("px");
        if (!fs.isEmpty()) e.tableFontSize = fs.toInt();
    }

    // <table> tag: border
    {
        QRegularExpression re(R"(<table\s([^>]*)>)");
        auto m = re.match(line);
        if (m.hasMatch()) {
            QString ts  = attrVal("<table " + m.captured(1) + ">", "style");
            QString bdr = cssProp(ts, "border");
            QRegularExpression bRe(R"(([\d.]+)px\s+solid\s+(\S+))");
            auto bm = bRe.match(bdr);
            if (bm.hasMatch()) {
                e.tableBorderWidth = bm.captured(1).toFloat();
                e.tableBorderColor = parseCssColor(bm.captured(2));
            }
        }
    }

    // <colgroup>: column fracs
    {
        QRegularExpression re(R"(<colgroup>(.*?)</colgroup>)");
        auto m = re.match(line);
        if (m.hasMatch()) {
            QRegularExpression cRe(R"(width:([\d.]+)%)");
            auto it = cRe.globalMatch(m.captured(1));
            while (it.hasNext())
                e.tableColFracs.append(it.next().captured(1).toFloat() / 100.f);
        }
    }
    e.tableCols = e.tableColFracs.size();
    if (e.tableCols == 0) return e;

    // <tr> rows — track spans with an occupancy grid
    QVector<QVector<bool>> occ; // occ[row][col] = covered by a span
    auto ensureOcc = [&](int r) {
        while (occ.size() <= r) occ.append(QVector<bool>(e.tableCols, false));
    };

    QRegularExpression trRe(R"(<tr\b([^>]*)>(.*?)</tr>)");
    auto trIt = trRe.globalMatch(line);
    int rowIdx = 0;
    while (trIt.hasNext()) {
        auto trm = trIt.next();
        QString trAttrs  = trm.captured(1);
        QString trContent = trm.captured(2);

        // Row height → fraction of element height
        QString trStyle;
        { QRegularExpression sr("style=\"([^\"]*)\""); auto sm = sr.match(trAttrs); if (sm.hasMatch()) trStyle = sm.captured(1); }
        float rowH   = cssProp(trStyle, "height").remove("px").toFloat();
        float rowFrac = (e.height > 0 && rowH > 0) ? rowH / e.height : 0.f;
        e.tableRowFracs.append(rowFrac);

        ensureOcc(rowIdx);
        QVector<TableCell> row(e.tableCols); // default-constructed cells
        // Pre-mark cells occupied by rowspan from earlier rows
        for (int c = 0; c < e.tableCols; ++c)
            if (occ[rowIdx][c]) row[c].merged = true;

        // Parse cells
        QRegularExpression cellRe(R"(<(th|td)((?:\s+[^>]*)?)>(.*?)</(th|td)>)");
        auto cellIt = cellRe.globalMatch(trContent);
        int colCursor = 0;
        while (cellIt.hasNext()) {
            auto cm  = cellIt.next();
            bool isth = (cm.captured(1) == "th");
            QString cellAttrs   = cm.captured(2);
            QString cellContent = cm.captured(3);

            // Advance past already-occupied columns
            while (colCursor < e.tableCols && occ[rowIdx][colCursor]) ++colCursor;
            if (colCursor >= e.tableCols) break;

            // Style
            QString cellStyle;
            { QRegularExpression sr("style=\"([^\"]*)\""); auto sm = sr.match(cellAttrs); if (sm.hasMatch()) cellStyle = sm.captured(1); }

            // Colspan / rowspan
            int cs = 1, rs = 1;
            { QRegularExpression r2("\\bcolspan=\"(\\d+)\""); auto m2 = r2.match(cellAttrs); if (m2.hasMatch()) cs = m2.captured(1).toInt(); }
            { QRegularExpression r2("\\browspan=\"(\\d+)\""); auto m2 = r2.match(cellAttrs); if (m2.hasMatch()) rs = m2.captured(1).toInt(); }
            cs = qMax(1, qMin(cs, e.tableCols - colCursor));

            // Mark occupancy grid
            for (int dr = 0; dr < rs; ++dr) {
                ensureOcc(rowIdx + dr);
                for (int dc = 0; dc < cs; ++dc) {
                    if (dr == 0 && dc == 0) continue;
                    int nc = colCursor + dc;
                    if (nc < e.tableCols) occ[rowIdx + dr][nc] = true;
                }
            }

            TableCell cell;
            cell.colspan = cs;
            cell.rowspan = rs;
            cell.merged  = false;

            QColor bg = parseCssColor(cssProp(cellStyle, "background"));
            QColor fg = parseCssColor(cssProp(cellStyle, "color"));

            if (rowIdx == 0 && isth) {
                if (!e.tableHasHeader) {
                    e.tableHasHeader = true;
                    if (bg.isValid() && bg != Qt::transparent) e.tableHeaderBg   = bg;
                    if (fg.isValid())                          e.tableHeaderText  = fg;
                }
                // Don't store header colors as cell overrides
            } else {
                if (bg.isValid() && bg != Qt::transparent) cell.bgColor   = bg;
                if (fg.isValid())                          cell.textColor = fg;
            }

            cell.textAlign = cssProp(cellStyle, "text-align").trimmed();
            if (cell.textAlign.isEmpty()) cell.textAlign = "left";
            cell.bold   = cssProp(cellStyle, "font-weight").trimmed() == "bold";
            cell.italic = cssProp(cellStyle, "font-style").trimmed()  == "italic";

            // Decode HTML entities and <br>
            cellContent.replace("<br>", "\n");
            cellContent.replace("&amp;","&").replace("&lt;","<").replace("&gt;",">")
                       .replace("&quot;","\"").replace("&#39;","'");
            cell.text = cellContent;

            row[colCursor] = cell;
            // Mark same-row merged cells (colspan)
            for (int dc = 1; dc < cs && colCursor + dc < e.tableCols; ++dc)
                row[colCursor + dc].merged = true;

            colCursor += cs;
        }
        e.tableCells.append(row);
        ++rowIdx;
    }
    e.tableRows = e.tableCells.size();

    // Normalize fracs to sum = 1
    auto norm = [](QVector<float>& v) {
        float s = 0; for (float f : v) s += f;
        if (s > 0.f && qAbs(s - 1.f) > 0.01f) for (float& f : v) f /= s;
    };
    norm(e.tableColFracs);
    norm(e.tableRowFracs);

    // If fracs are all zero (export had no explicit heights), distribute evenly
    auto evenIfZero = [](QVector<float>& v, int n) {
        float s = 0; for (float f : v) s += f;
        if (s < 0.001f && n > 0) { v.fill(1.f / n); }
    };
    evenIfZero(e.tableColFracs, e.tableCols);
    evenIfZero(e.tableRowFracs, e.tableRows);

    return e;
}

// ── Public API ────────────────────────────────────────────────────────────────

Presentation* HtmlImporter::importFrom(const QString& folderPath, QString& errorMsg) {
    QDir dir(folderPath);

    if (!QFile::exists(dir.filePath("index.html"))) {
        errorMsg = "Not a valid presentation folder.\n"
                   "index.html not found in:\n" + folderPath;
        return nullptr;
    }

    auto readFile = [](const QString& path) -> QString {
        QFile f(path);
        return f.open(QIODevice::ReadOnly | QIODevice::Text)
               ? QTextStream(&f).readAll() : QString();
    };

    QString html = readFile(dir.filePath("index.html"));
    QString css  = readFile(dir.filePath("styles.css"));

    if (html.isEmpty()) {
        errorMsg = "index.html could not be read.";
        return nullptr;
    }

    auto* pres = new Presentation();
    pres->exportPath = folderPath;

    // ── Parse <title> ──────────────────────────────────────────────────────────
    {
        QRegularExpression re(R"(<title>([^<]*)</title>)");
        auto m = re.match(html);
        if (m.hasMatch())
            pres->title = m.captured(1).trimmed()
                              .replace("&lt;", "<").replace("&gt;", ">")
                              .replace("&quot;", "\"").replace("&#39;", "'")
                              .replace("&amp;", "&");
    }

    // ── Parse CSS: scene background and slide dimensions ──────────────────────
    if (!css.isEmpty()) {
        QString bodyBlk = cssBlock(css, "body");
        if (!bodyBlk.isEmpty()) {
            QColor bg = parseCssColor(cssProp(bodyBlk, "background"));
            if (bg.isValid() && bg != Qt::transparent)
                pres->sceneBackground = bg;
        }

        QString stepBlk = cssBlock(css, ".step");
        if (!stepBlk.isEmpty()) {
            float w = cssProp(stepBlk, "width").remove("px").toFloat();
            float h = cssProp(stepBlk, "height").remove("px").toFloat();
            if (w > 0) pres->slideWidth  = w;
            if (h > 0) pres->slideHeight = h;
        }
    }

    // ── data-width / data-height / data-default-inactive-opacity from #impress ─
    {
        QRegularExpression re(R"(<div\s+id="impress"[^>]*>)",
                               QRegularExpression::DotMatchesEverythingOption);
        auto m = re.match(html);
        if (m.hasMatch()) {
            QString tag = m.captured(0);
            float dw = attrVal(tag, "data-width").toFloat();
            float dh = attrVal(tag, "data-height").toFloat();
            if (dw > 0) pres->slideWidth  = dw;
            if (dh > 0) pres->slideHeight = dh;
            QString defOpa = attrVal(tag, "data-default-inactive-opacity");
            if (!defOpa.isEmpty()) pres->defaultInactiveOpacity = defOpa.toFloat();
        }
    }

    // ── Parse step blocks ─────────────────────────────────────────────────────
    QStringList steps = extractStepBlocks(html);

    // Pre-pass: assign UUIDs for each HTML step id so that visibility cross-
    // references between steps can be resolved after the main parse loop.
    QMap<QString, QString> htmlIdToUuid; // "slide-N" → uuid
    for (const QString& stepBlock : steps) {
        int tagEnd = stepBlock.indexOf('>');
        if (tagEnd < 0) continue;
        QString sid = attrVal(stepBlock.left(tagEnd + 1), "id");
        if (!sid.isEmpty() && !htmlIdToUuid.contains(sid))
            htmlIdToUuid[sid] = QUuid::createUuid().toString(QUuid::WithoutBraces);
    }
    if (steps.isEmpty()) {
        errorMsg = "No slides (class=\"step\") found in index.html.";
        delete pres;
        return nullptr;
    }

    parseVariablesJson(html, pres, htmlIdToUuid);

    QString assetsDir = dir.filePath("assets");

    for (const QString& stepBlock : steps) {
        Slide slide;

        // Opening tag = up to first ">"
        int tagEnd  = stepBlock.indexOf('>');
        
        // CRITICAL FIX: Validate opening tag is properly formed
        if (tagEnd < 0) {
            // Malformed step block without closing >, add empty slide and skip
            pres->slides.append(slide);
            continue;
        }
        
        QString tag = stepBlock.left(tagEnd + 1);

        // Assign the pre-computed UUID so visibility cross-references are stable
        QString sid = attrVal(tag, "id");
        if (!sid.isEmpty() && htmlIdToUuid.contains(sid))
            slide.id = htmlIdToUuid[sid];

        // Slide name: prefer data-name, fall back to stripping "slide-" from id
        QString dname = attrVal(tag, "data-name");
        if (!dname.isEmpty())
            slide.name = dname;
        else if (!sid.isEmpty())
            slide.name = sid.startsWith("slide-") ? sid.mid(6) : sid;
        else
            slide.name = QString("Slide %1").arg(pres->slides.size() + 1);

        // Camera view offset (our custom attributes)
        float offX = attrVal(tag, "data-view-offset-x").toFloat();
        float offY = attrVal(tag, "data-view-offset-y").toFloat();
        slide.viewOffsetX = offX;
        slide.viewOffsetY = offY;

        // Position: data-x/y store (posX+offX) and -(posY+offY)
        float camX = attrVal(tag, "data-x").toFloat();
        float camY = attrVal(tag, "data-y").toFloat();
        slide.posX = camX - offX;
        slide.posY = -camY - offY;
        slide.posZ = attrVal(tag, "data-z").toFloat();

        // Rotation
        slide.rotX = attrVal(tag, "data-rotate-x").toFloat();
        slide.rotY = attrVal(tag, "data-rotate-y").toFloat();
        slide.rotZ = attrVal(tag, "data-rotate").toFloat();

        // Zoom/scale
        QString sc = attrVal(tag, "data-scale");
        slide.scale = sc.isEmpty() ? 1.f : sc.toFloat();

        // Slide background + optional per-slide dimensions from inline style
        QString stepStyle = attrVal(tag, "style");
        slide.backgroundColor = parseCssColor(cssProp(stepStyle, "background"));
        QString sw = cssProp(stepStyle, "width");
        QString sh = cssProp(stepStyle, "height");
        if (!sw.isEmpty()) slide.slideWidth  = sw.remove("px").toFloat();
        if (!sh.isEmpty()) slide.slideHeight = sh.remove("px").toFloat();

        // Per-slide visibility overrides: "slide-2:0.50,slide-3:0.00"
        QString visRaw = attrVal(tag, "data-vis-overrides");
        if (!visRaw.isEmpty()) {
            for (const QString& pair : visRaw.split(',')) {
                QStringList kv = pair.split(':');
                if (kv.size() == 2) {
                    QString otherId = kv[0].trimmed();
                    float   opa     = kv[1].trimmed().toFloat();
                    if (htmlIdToUuid.contains(otherId))
                        slide.visibilityOverrides[htmlIdToUuid[otherId]] = opa;
                }
            }
        }

        // Editor-only ruler guides: "v:960.00,h:540.00,c:500.00:400.00:150.00"
        QString guidesRaw = attrVal(tag, "data-guides");
        if (!guidesRaw.isEmpty()) {
            for (const QString& tok : guidesRaw.split(',')) {
                QStringList parts = tok.split(':');
                if (parts.size() == 2 && (parts[0] == "v" || parts[0] == "h")) {
                    GuideLine g;
                    g.vertical = (parts[0] == "v");
                    g.pos      = parts[1].toFloat();
                    slide.guides.append(g);
                } else if (parts.size() == 4 && parts[0] == "c") {
                    GuideCircle c;
                    c.cx     = parts[1].toFloat();
                    c.cy     = parts[2].toFloat();
                    c.radius = parts[3].toFloat();
                    slide.guideCircles.append(c);
                }
            }
        }

        // ── Child elements ────────────────────────────────────────────────────
        // CRITICAL FIX: Validate closing tag exists and calculate safe length
        int closingPos = stepBlock.lastIndexOf("</div>");
        if (closingPos < 0 || closingPos <= tagEnd) {
            // Malformed step block: missing or misplaced closing tag
            pres->slides.append(slide);
            continue;
        }
        
        int contentLen = closingPos - tagEnd - 1;
        if (contentLen < 0) {
            // Defensive check: should not happen, but prevents negative length
            pres->slides.append(slide);
            continue;
        }
        
        QString content = stepBlock.mid(tagEnd + 1, contentLen);

        for (const QString& rawLine : content.split('\n')) {
            QString line = rawLine.trimmed();
            if (line.isEmpty()) continue;

            if (line.startsWith("<img")) {
                // ── Image element ─────────────────────────────────────────────
                SlideElement e;
                e.type    = SlideElement::Image;
                QString src   = attrVal(line, "src");
                QString style = attrVal(line, "style");
                e.content = src.startsWith("assets/")
                            ? QDir(assetsDir).filePath(src.mid(7)) : src;
                e.x      = cssProp(style, "left").remove("px").toFloat();
                e.y      = cssProp(style, "top").remove("px").toFloat();
                e.width  = cssProp(style, "width").remove("px").toFloat();
                e.height = cssProp(style, "height").remove("px").toFloat();
                parseTimelineAndOpacity(line, style, e);
                slide.elements.append(e);

            } else if (line.startsWith("<video")) {
                // ── Video element ───────────────────────────────────────────
                SlideElement e;
                e.type    = SlideElement::Video;
                QString src   = attrVal(line, "src");
                QString style = attrVal(line, "style");
                e.content = src.startsWith("assets/")
                            ? QDir(assetsDir).filePath(src.mid(7)) : src;
                e.x      = cssProp(style, "left").remove("px").toFloat();
                e.y      = cssProp(style, "top").remove("px").toFloat();
                e.width  = cssProp(style, "width").remove("px").toFloat();
                e.height = cssProp(style, "height").remove("px").toFloat();
                QString transform = cssProp(style, "transform");
                if (transform.contains("rotate")) {
                    QRegularExpression rotRe(R"(rotate\(([-\d.]+)deg\))");
                    auto rotM = rotRe.match(transform);
                    if (rotM.hasMatch()) e.rotation = rotM.captured(1).toFloat();
                }
                e.mediaAutoplay = attrVal(line, "data-autoplay") == "true";
                parseTimelineAndOpacity(line, style, e);
                slide.elements.append(e);

            } else if (line.startsWith("<iframe")) {
                // ── IFrame element ───────────────────────────────────────────
                SlideElement e;
                e.type    = SlideElement::IFrame;
                QString style = attrVal(line, "style");
                QString src   = attrVal(line, "src");
                src.replace("&amp;", "&").replace("&lt;", "<").replace("&gt;", ">")
                   .replace("&quot;", "\"").replace("&#39;", "'");
                e.content = src;
                e.x      = cssProp(style, "left").remove("px").toFloat();
                e.y      = cssProp(style, "top").remove("px").toFloat();
                e.width  = cssProp(style, "width").remove("px").toFloat();
                e.height = cssProp(style, "height").remove("px").toFloat();
                parseTimelineAndOpacity(line, style, e);
                slide.elements.append(e);

            } else if (line.startsWith("<div")) {
                // ── Text or Shape element ──────────────────────────────────────
                int dTagEnd    = line.indexOf('>');
                
                // CRITICAL FIX: Validate element div tag is properly formed
                if (dTagEnd < 0) {
                    // Malformed element tag, skip this element
                    continue;
                }
                
                QString dTag   = line.left(dTagEnd + 1);
                int cEnd       = line.lastIndexOf("</div>");
                
                // CRITICAL FIX: Validate and safely extract element content
                QString elemTxt;
                if (cEnd > dTagEnd) {
                    int elemLen = cEnd - dTagEnd - 1;
                    if (elemLen >= 0) {
                        elemTxt = line.mid(dTagEnd + 1, elemLen);
                    }
                } else if (cEnd >= 0 && cEnd <= dTagEnd) {
                    // Closing tag before opening tag - malformed
                    elemTxt = QString();
                }

                QString style  = attrVal(dTag, "style");
                QString dtype  = attrVal(dTag, "data-type");
                QString dshape = attrVal(dTag, "data-shape");

                // Table element: parse the full line (exporter writes it on one line)
                if (dtype == "table") {
                    SlideElement te = parseTableDiv(line);
                    parseTimelineAndOpacity(dTag, style, te);
                    if (te.tableRows > 0 && te.tableCols > 0)
                        slide.elements.append(te);
                    continue;
                }

                // Chart element: restore from base64-encoded JSON in data-chart attr
                if (dtype == "chart") {
                    QString chartB64 = attrVal(dTag, "data-chart");
                    if (!chartB64.isEmpty()) {
                        QByteArray jsonBa = QByteArray::fromBase64(chartB64.toLatin1());
                        QJsonDocument doc = QJsonDocument::fromJson(jsonBa);
                        if (doc.isObject()) {
                            SlideElement ce;
                            ce.type      = SlideElement::Chart;
                            ce.x         = cssProp(style, "left").remove("px").toFloat();
                            ce.y         = cssProp(style, "top").remove("px").toFloat();
                            ce.width     = cssProp(style, "width").remove("px").toFloat();
                            ce.height    = cssProp(style, "height").remove("px").toFloat();
                            ce.chartData = ChartData::fromJson(doc.object());
                            // Chart rotation
                            QString chartTransform = cssProp(style, "transform");
                            if (chartTransform.contains("rotate")) {
                                QRegularExpression rotRe(R"(rotate\(([-\d.]+)deg\))");
                                auto rotM = rotRe.match(chartTransform);
                                if (rotM.hasMatch()) ce.rotation = rotM.captured(1).toFloat();
                            }
                            parseTimelineAndOpacity(dTag, style, ce);
                            slide.elements.append(ce);
                        }
                    }
                    continue;
                }

                // IFrame element: wrapped in a div with a nested <iframe src="...">
                if (dtype == "iframe-wrap") {
                    QRegularExpression srcRe("<iframe[^>]*\\ssrc=\"([^\"]*)\"");
                    auto srcM = srcRe.match(line);
                    if (srcM.hasMatch()) {
                        SlideElement ie;
                        ie.type   = SlideElement::IFrame;
                        ie.x      = cssProp(style, "left").remove("px").toFloat();
                        ie.y      = cssProp(style, "top").remove("px").toFloat();
                        ie.width  = cssProp(style, "width").remove("px").toFloat();
                        ie.height = cssProp(style, "height").remove("px").toFloat();
                        QString transform = cssProp(style, "transform");
                        if (transform.contains("rotate")) {
                            QRegularExpression rotRe(R"(rotate\(([-\d.]+)deg\))");
                            auto rotM = rotRe.match(transform);
                            if (rotM.hasMatch()) ie.rotation = rotM.captured(1).toFloat();
                        }
                        QString src = srcM.captured(1);
                        src.replace("&amp;", "&").replace("&lt;", "<").replace("&gt;", ">")
                           .replace("&quot;", "\"").replace("&#39;", "'");
                        ie.content = src;
                        parseTimelineAndOpacity(dTag, style, ie);
                        slide.elements.append(ie);
                    }
                    continue;
                }

                // Audio element: wrapped in a div with a nested <audio src="...">
                if (dtype == "audio") {
                    QRegularExpression srcRe("<audio[^>]*\\ssrc=\"([^\"]*)\"");
                    auto srcM = srcRe.match(line);
                    if (srcM.hasMatch()) {
                        SlideElement ae;
                        ae.type   = SlideElement::Audio;
                        ae.x      = cssProp(style, "left").remove("px").toFloat();
                        ae.y      = cssProp(style, "top").remove("px").toFloat();
                        ae.width  = cssProp(style, "width").remove("px").toFloat();
                        ae.height = cssProp(style, "height").remove("px").toFloat();
                        QString transform = cssProp(style, "transform");
                        if (transform.contains("rotate")) {
                            QRegularExpression rotRe(R"(rotate\(([-\d.]+)deg\))");
                            auto rotM = rotRe.match(transform);
                            if (rotM.hasMatch()) ae.rotation = rotM.captured(1).toFloat();
                        }
                        QString src = srcM.captured(1);
                        ae.content = src.startsWith("assets/")
                                     ? QDir(assetsDir).filePath(src.mid(7)) : src;
                        ae.audioShowWaveform = attrVal(dTag, "data-audio-mode") == "waveform";
                        ae.mediaAutoplay     = attrVal(dTag, "data-autoplay") == "true";
                        parseTimelineAndOpacity(dTag, style, ae);
                        slide.elements.append(ae);
                    }
                    continue;
                }

                // Formula element: LaTeX source stored in data-latex attribute
                if (dtype == "formula") {
                    SlideElement fe;
                    fe.type   = SlideElement::Formula;
                    fe.x      = cssProp(style, "left").remove("px").toFloat();
                    fe.y      = cssProp(style, "top").remove("px").toFloat();
                    fe.width  = cssProp(style, "width").remove("px").toFloat();
                    fe.height = cssProp(style, "height").remove("px").toFloat();
                    QString fs = cssProp(style, "font-size").remove("px");
                    if (!fs.isEmpty()) fe.fontSize = fs.toInt();
                    QColor fc = parseCssColor(cssProp(style, "color"));
                    if (fc.isValid()) fe.color = fc;
                    QColor bg = parseCssColor(cssProp(style, "background"));
                    if (bg.isValid()) fe.backgroundColor = bg;
                    QString transform = cssProp(style, "transform");
                    if (transform.contains("rotate")) {
                        QRegularExpression rotRe(R"(rotate\(([-\d.]+)deg\))");
                        auto rotM = rotRe.match(transform);
                        if (rotM.hasMatch()) fe.rotation = rotM.captured(1).toFloat();
                    }
                    QString latex = attrVal(dTag, "data-latex");
                    latex.replace("&amp;", "&").replace("&lt;", "<").replace("&gt;", ">")
                         .replace("&quot;", "\"").replace("&#39;", "'");
                    fe.content = latex;
                    parseTimelineAndOpacity(dTag, style, fe);
                    slide.elements.append(fe);
                    continue;
                }

                // Slider element: range input bound to a Number variable
                if (dtype == "slider") {
                    SlideElement se;
                    se.type   = SlideElement::Slider;
                    se.x      = cssProp(style, "left").remove("px").toFloat();
                    se.y      = cssProp(style, "top").remove("px").toFloat();
                    se.width  = cssProp(style, "width").remove("px").toFloat();
                    se.height = cssProp(style, "height").remove("px").toFloat();
                    QString ff = cssProp(style, "font-family");
                    ff.remove('\'').remove('"');
                    if (!ff.isEmpty()) se.fontFamily = ff;
                    QString fs = cssProp(style, "font-size").remove("px");
                    if (!fs.isEmpty()) se.fontSize = fs.toInt();
                    QColor fc = parseCssColor(cssProp(style, "color"));
                    if (fc.isValid()) se.color = fc;

                    QRegularExpression labelRe("<span[^>]*\\sdata-slider-label=\"([^\"]*)\"");
                    auto labelM = labelRe.match(line);
                    if (labelM.hasMatch()) {
                        QString lbl = labelM.captured(1);
                        lbl.replace("&amp;", "&").replace("&lt;", "<").replace("&gt;", ">")
                           .replace("&quot;", "\"").replace("&#39;", "'");
                        se.content = lbl;
                    }
                    QRegularExpression inputRe("<input[^>]*type=\"range\"[^>]*>");
                    auto inputM = inputRe.match(line);
                    if (inputM.hasMatch()) {
                        QString inputTag = inputM.captured(0);
                        se.boundVariableId = attrVal(inputTag, "data-var-id");
                        QString minS = attrVal(inputTag, "min");
                        QString maxS = attrVal(inputTag, "max");
                        QString stepS = attrVal(inputTag, "step");
                        if (!minS.isEmpty())  se.sliderMin  = minS.toDouble();
                        if (!maxS.isEmpty())  se.sliderMax  = maxS.toDouble();
                        if (!stepS.isEmpty()) se.sliderStep = stepS.toDouble();
                    }
                    parseTimelineAndOpacity(dTag, style, se);
                    slide.elements.append(se);
                    continue;
                }

                // Classify: prefer explicit data-type, fall back to style inspection
                bool isText = (dtype == "text") ||
                              (dtype.isEmpty() && style.contains("font-family"));

                SlideElement e;
                e.x      = cssProp(style, "left").remove("px").toFloat();
                e.y      = cssProp(style, "top").remove("px").toFloat();
                e.width  = cssProp(style, "width").remove("px").toFloat();
                e.height = cssProp(style, "height").remove("px").toFloat();

                // Element rotation: transform:rotate(Ndeg)
                {
                    QString transform = cssProp(style, "transform");
                    if (transform.contains("rotate")) {
                        QRegularExpression rotRe(R"(rotate\(([-\d.]+)deg\))");
                        auto rotM = rotRe.match(transform);
                        if (rotM.hasMatch())
                            e.rotation = rotM.captured(1).toFloat();
                    }
                }

                if (isText) {
                    e.type = SlideElement::Text;
                    QString ff = cssProp(style, "font-family");
                    ff.remove('\'').remove('"');
                    if (!ff.isEmpty()) e.fontFamily = ff;
                    QString fs = cssProp(style, "font-size").remove("px");
                    if (!fs.isEmpty()) e.fontSize = fs.toInt();
                    e.color = parseCssColor(cssProp(style, "color"));
                    QString align = cssProp(style, "text-align");
                    if (!align.isEmpty()) e.textAlignment = align;
                    QString justify = cssProp(style, "justify-content").trimmed();
                    if      (justify == "center")   e.verticalAlignment = "middle";
                    else if (justify == "flex-end")  e.verticalAlignment = "bottom";
                    else if (!justify.isEmpty())     e.verticalAlignment = "top";
                    QString ebg = cssProp(style, "background");
                    if (!ebg.isEmpty()) e.backgroundColor = parseCssColor(ebg);
                    e.bold         = (cssProp(style, "font-weight").trimmed() == "bold");
                    e.italic       = (cssProp(style, "font-style").trimmed()  == "italic");
                    QString deco   = cssProp(style, "text-decoration");
                    e.underline    = deco.contains("underline");
                    e.strikethrough = deco.contains("line-through");
                    if (e.underline) {
                        QColor ulc = parseCssColor(cssProp(style, "text-decoration-color"));
                        if (ulc.isValid() && ulc != Qt::transparent) e.underlineColor = ulc;
                        QString ulst = cssProp(style, "text-decoration-style").trimmed();
                        if      (ulst == "dashed") e.underlineStyle = 1;
                        else if (ulst == "dotted") e.underlineStyle = 2;
                        else if (ulst == "wavy")   e.underlineStyle = 3;
                    }
                    // Hyperlink: content wrapped in <a href="...">...</a> by the exporter
                    {
                        QRegularExpression linkRe("^<a href=\"([^\"]*)\"[^>]*>(.*)</a>$",
                                                   QRegularExpression::DotMatchesEverythingOption);
                        auto linkM = linkRe.match(elemTxt);
                        if (linkM.hasMatch()) {
                            QString href = linkM.captured(1);
                            href.replace("&amp;", "&").replace("&lt;", "<").replace("&gt;", ">")
                                .replace("&quot;", "\"").replace("&#39;", "'");
                            e.hyperlink = href;
                            elemTxt = linkM.captured(2);
                        }
                    }

                    // Check for list style
                    QString dlist = attrVal(dTag, "data-list");
                    if (!dlist.isEmpty()) {
                        e.listStyle = (dlist == "bullets") ? SlideElement::Bullets
                                                           : SlideElement::Numbered;
                        QRegularExpression liRe("<li>([^<]*)</li>");
                        QStringList items;
                        auto liIt = liRe.globalMatch(elemTxt);
                        while (liIt.hasNext()) items << liIt.next().captured(1);
                        e.content = items.join('\n');
                    } else {
                        // Restore text: unescape HTML entities and <br> → \n, and pull
                        // <code class="language-xxx">…</code> segments back out into
                        // e.codeSpans (mirror of HtmlExporter::buildTextWithCodeSpans).
                        // Offsets must be computed AFTER per-segment unescaping, since
                        // entity unescaping changes character counts (e.g. "&amp;" → "&") —
                        // so each segment (plain gap or code chunk) is unescaped first,
                        // then appended to the running `content`, with the span's start
                        // recorded at that point.
                        QRegularExpression codeRe(
                            "<code class=\"language-([^\"]*)\">(.*?)</code>",
                            QRegularExpression::DotMatchesEverythingOption);
                        QString content;
                        QVector<CodeSpan> spans;
                        int last = 0;
                        auto codeIt = codeRe.globalMatch(elemTxt);
                        while (codeIt.hasNext()) {
                            auto m = codeIt.next();
                            if (m.capturedStart() > last)
                                content += unescapeTextSegment(elemTxt.mid(last, m.capturedStart() - last));
                            QString chunk = unescapeTextSegment(m.captured(2));
                            CodeSpan sp;
                            sp.start    = content.length();
                            sp.length   = chunk.length();
                            sp.language = m.captured(1);
                            spans << sp;
                            content += chunk;
                            last = m.capturedEnd();
                        }
                        if (last < elemTxt.length())
                            content += unescapeTextSegment(elemTxt.mid(last));
                        e.content   = content;
                        e.codeSpans = spans;
                    }
                } else if (dtype == "icon") {
                    e.type = SlideElement::Icon;
                    QString dicon = attrVal(dTag, "data-icon");
                    e.content = dicon;
                    QColor fc = parseCssColor(cssProp(style, "color"));
                    if (fc.isValid()) e.color = fc;
                    parseTimelineAndOpacity(dTag, style, e);
                    slide.elements.append(e);
                    continue;
                } else {
                    e.type = SlideElement::Shape;
                    if (!dshape.isEmpty())
                        e.content = dshape;
                    else if (style.contains("border-radius:50%") || cssProp(style, "border-radius") == "50%")
                        e.content = "circle";
                    else
                        e.content = "rect";
                    e.backgroundColor = parseCssColor(cssProp(style, "background"));
                    // Parse border shorthand: "2px solid #888888"
                    QString border = cssProp(style, "border");
                    QRegularExpression bRe(R"(([\d.]+)px\s+solid\s+(.+))");
                    auto bm = bRe.match(border);
                    if (bm.hasMatch()) {
                        e.borderWidth = bm.captured(1).toFloat();
                        e.borderColor = parseCssColor(bm.captured(2).trimmed());
                    }
                    // Corner radius (skip 50% = circle)
                    QString brad = cssProp(style, "border-radius");
                    if (!brad.isEmpty() && brad != "50%")
                        e.cornerRadius = brad.remove("px").toFloat();
                    // Mesh gradient points (see HtmlExporter's data-mesh, mirrors data-chart)
                    QString meshB64 = attrVal(dTag, "data-mesh");
                    if (!meshB64.isEmpty()) {
                        QJsonDocument mdoc = QJsonDocument::fromJson(QByteArray::fromBase64(meshB64.toLatin1()));
                        if (mdoc.isObject()) {
                            e.meshGradient    = MeshGradientData::fromJson(mdoc.object());
                            e.useMeshGradient = e.meshGradient.isUsable();
                        }
                    }
                    // Shape text from inner <span>
                    if (!elemTxt.isEmpty()) {
                        QRegularExpression spanRe(
                            R"rx(<span[^>]*style="([^"]*)"[^>]*>(.*?)</span>)rx",
                            QRegularExpression::DotMatchesEverythingOption);
                        auto sm = spanRe.match(elemTxt);
                        if (sm.hasMatch()) {
                            QString spanStyle   = sm.captured(1);
                            QString spanContent = sm.captured(2);
                            // Font properties from span style
                            QString ff2 = cssProp(spanStyle, "font-family");
                            ff2.remove('\'').remove('"').remove(' ');
                            if (!ff2.isEmpty()) e.fontFamily = ff2;
                            QString fs2 = cssProp(spanStyle, "font-size").remove("px");
                            if (!fs2.isEmpty()) e.fontSize = fs2.toInt();
                            QColor fc2 = parseCssColor(cssProp(spanStyle, "color"));
                            if (fc2.isValid()) e.color = fc2;
                            e.bold   = cssProp(spanStyle, "font-weight").trimmed() == "bold";
                            e.italic = cssProp(spanStyle, "font-style").trimmed() == "italic";
                            // Decode text
                            spanContent.replace("<br>", "\n")
                                       .replace("&amp;", "&").replace("&lt;", "<")
                                       .replace("&gt;",  ">").replace("&quot;", "\"")
                                       .replace("&#39;", "'");
                            e.shapeText = spanContent;
                        }
                    }
                }
                parseTimelineAndOpacity(dTag, style, e);

                slide.elements.append(e);

            } else if (line.startsWith("<a") && attrVal(line, "data-type") == "button") {
                // ── Navigation button ───────────────────────────────────────────
                SlideElement e;
                e.type = SlideElement::Button;
                QString style = attrVal(line, "style");
                e.x      = cssProp(style, "left").remove("px").toFloat();
                e.y      = cssProp(style, "top").remove("px").toFloat();
                e.width  = cssProp(style, "width").remove("px").toFloat();
                e.height = cssProp(style, "height").remove("px").toFloat();
                QString transform = cssProp(style, "transform");
                if (transform.contains("rotate")) {
                    QRegularExpression rotRe(R"(rotate\(([-\d.]+)deg\))");
                    auto rotM = rotRe.match(transform);
                    if (rotM.hasMatch()) e.rotation = rotM.captured(1).toFloat();
                }
                QString ff = cssProp(style, "font-family");
                ff.remove('\'').remove('"');
                if (!ff.isEmpty()) e.fontFamily = ff;
                QString fs = cssProp(style, "font-size").remove("px");
                if (!fs.isEmpty()) e.fontSize = fs.toInt();
                QColor fc = parseCssColor(cssProp(style, "color"));
                if (fc.isValid()) e.color = fc;
                QColor bg = parseCssColor(cssProp(style, "background"));
                if (bg.isValid()) e.backgroundColor = bg;
                e.bold = cssProp(style, "font-weight").trimmed() == "bold";
                QString border = cssProp(style, "border");
                QRegularExpression bRe(R"(([\d.]+)px\s+solid\s+(.+))");
                auto bm = bRe.match(border);
                if (bm.hasMatch()) {
                    e.borderWidth = bm.captured(1).toFloat();
                    e.borderColor = parseCssColor(bm.captured(2).trimmed());
                }
                QString brad = cssProp(style, "border-radius");
                if (!brad.isEmpty()) e.cornerRadius = brad.remove("px").toFloat();

                QString varId = attrVal(line, "data-var-id");
                if (!varId.isEmpty()) {
                    e.buttonAction    = "changeVariable";
                    e.boundVariableId = varId;
                    QString op = attrVal(line, "data-var-op");
                    if (!op.isEmpty()) e.varOp = op;
                    e.varOpNumber = attrVal(line, "data-var-number").toDouble();
                    e.varOpBool   = attrVal(line, "data-var-bool") == "true";
                    QString varText = attrVal(line, "data-var-text");
                    varText.replace("&amp;", "&").replace("&lt;", "<").replace("&gt;", ">")
                            .replace("&quot;", "\"").replace("&#39;", "'");
                    e.varOpText = varText;
                } else {
                    QString targetId = attrVal(line, "data-target");
                    if (!targetId.isEmpty() && htmlIdToUuid.contains(targetId))
                        e.targetSlideId = htmlIdToUuid[targetId];
                }

                int aTagEnd = line.indexOf('>');
                int aEnd    = line.lastIndexOf("</a>");
                if (aTagEnd >= 0 && aEnd > aTagEnd) {
                    QString label = line.mid(aTagEnd + 1, aEnd - aTagEnd - 1);
                    label.replace("&amp;", "&").replace("&lt;", "<").replace("&gt;", ">")
                         .replace("&quot;", "\"").replace("&#39;", "'");
                    e.content = label;
                }
                parseTimelineAndOpacity(line, style, e);
                slide.elements.append(e);

            } else if (line.startsWith("<label") && attrVal(line, "data-type") == "checkbox") {
                // ── Checkbox element ─────────────────────────────────────────────
                SlideElement e;
                e.type = SlideElement::Checkbox;
                QString style = attrVal(line, "style");
                e.x      = cssProp(style, "left").remove("px").toFloat();
                e.y      = cssProp(style, "top").remove("px").toFloat();
                e.width  = cssProp(style, "width").remove("px").toFloat();
                e.height = cssProp(style, "height").remove("px").toFloat();
                QString ff = cssProp(style, "font-family");
                ff.remove('\'').remove('"');
                if (!ff.isEmpty()) e.fontFamily = ff;
                QString fs = cssProp(style, "font-size").remove("px");
                if (!fs.isEmpty()) e.fontSize = fs.toInt();
                QColor fc = parseCssColor(cssProp(style, "color"));
                if (fc.isValid()) e.color = fc;

                QRegularExpression inputRe("<input[^>]*type=\"checkbox\"[^>]*>");
                auto inputM = inputRe.match(line);
                if (inputM.hasMatch())
                    e.boundVariableId = attrVal(inputM.captured(0), "data-var-id");

                QRegularExpression spanRe("<span[^>]*>(.*)</span>");
                auto spanM = spanRe.match(line);
                if (spanM.hasMatch()) {
                    QString lbl = spanM.captured(1);
                    lbl.replace("&amp;", "&").replace("&lt;", "<").replace("&gt;", ">")
                       .replace("&quot;", "\"").replace("&#39;", "'");
                    e.content = lbl;
                }
                parseTimelineAndOpacity(line, style, e);
                slide.elements.append(e);
            }
        }

        pres->slides.append(slide);
    }

    if (pres->slides.isEmpty()) {
        errorMsg = "No slides loaded.";
        delete pres;
        return nullptr;
    }

    // ── Parse world-object blocks ─────────────────────────────────────────────
    static const QString kAssetsModelsPrefix = QStringLiteral("assets/models/");
    for (const QString& block : extractDivBlocksByClass(html, "world-object")) {
        int tagEnd = block.indexOf('>');
        if (tagEnd < 0) continue;
        QString tag = block.left(tagEnd + 1);

        WorldObject w;
        QString wid = attrVal(tag, "data-wid");
        if (!wid.isEmpty()) w.id = wid;

        QString style = attrVal(tag, "style");

        QRegularExpression reT(R"(translate3d\(\s*(-?[\d.]+)px\s*,\s*(-?[\d.]+)px\s*,\s*(-?[\d.]+)px\s*\))");
        auto mT = reT.match(style);
        if (mT.hasMatch()) {
            w.posX =  mT.captured(1).toFloat();
            w.posY = -mT.captured(2).toFloat(); // undo worldObjectToHtml()'s Y negation
            w.posZ =  mT.captured(3).toFloat();
        }
        QRegularExpression reRX(R"(rotateX\(\s*(-?[\d.]+)deg\s*\))");
        QRegularExpression reRY(R"(rotateY\(\s*(-?[\d.]+)deg\s*\))");
        QRegularExpression reRZ(R"(rotateZ\(\s*(-?[\d.]+)deg\s*\))");
        if (auto m = reRX.match(style); m.hasMatch()) w.rotX = m.captured(1).toFloat();
        if (auto m = reRY.match(style); m.hasMatch()) w.rotY = m.captured(1).toFloat();
        if (auto m = reRZ.match(style); m.hasMatch()) w.rotZ = m.captured(1).toFloat();

        QRegularExpression reS(R"(\bscale\(\s*(-?[\d.]+)\s*\))");
        if (auto m = reS.match(style); m.hasMatch()) w.scale = m.captured(1).toFloat();

        QString opaStr = cssProp(style, "opacity");
        if (!opaStr.isEmpty()) w.opacity = opaStr.toFloat();

        QRegularExpression reSrc(R"RX(<model-viewer\s+src="([^"]*)")RX",
                                  QRegularExpression::DotMatchesEverythingOption);
        auto mSrc = reSrc.match(block);
        QString src = mSrc.hasMatch() ? mSrc.captured(1) : QString();
        w.modelPath = src.startsWith(kAssetsModelsPrefix)
            ? QDir(assetsDir).filePath("models/" + src.mid(kAssetsModelsPrefix.size()))
            : src;

        pres->worldObjects.append(w);
    }

    return pres;
}
