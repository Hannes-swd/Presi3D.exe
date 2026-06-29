#include "HtmlImporter.h"
#include <QFile>
#include <QDir>
#include <QTextStream>
#include <QRegularExpression>

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

// Read a CSS block for a given selector, e.g. "body" or ".step"
static QString cssBlock(const QString& css, const QString& selector) {
    QRegularExpression re(QRegularExpression::escape(selector) +
                          R"(\s*\{([^}]*)\})",
                          QRegularExpression::DotMatchesEverythingOption);
    auto m = re.match(css);
    return m.hasMatch() ? m.captured(1) : QString();
}

// ── Public API ────────────────────────────────────────────────────────────────

Presentation* HtmlImporter::importFrom(const QString& folderPath, QString& errorMsg) {
    QDir dir(folderPath);

    if (!QFile::exists(dir.filePath("index.html"))) {
        errorMsg = "Kein gültiger Präsentationsordner.\n"
                   "index.html nicht gefunden in:\n" + folderPath;
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
        errorMsg = "index.html konnte nicht gelesen werden.";
        return nullptr;
    }

    auto* pres = new Presentation();
    pres->exportPath = folderPath;

    // ── Parse CSS ─────────────────────────────────────────────────────────────
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
            float opa = cssProp(stepBlk, "opacity").toFloat();
            if (opa > 0) pres->inactiveOpacity = opa;
        }

        // .step.active / .step.present opacity
        QRegularExpression reAct(R"(\.step\.(?:active|present)[^{]*\{([^}]*)\})",
                                  QRegularExpression::DotMatchesEverythingOption);
        auto mAct = reAct.match(css);
        if (mAct.hasMatch()) {
            float opa = cssProp(mAct.captured(1), "opacity").toFloat();
            if (opa > 0) pres->activeOpacity = opa;
        }

        // noDimming: generated CSS has ".step { opacity: 1 !important; }"
        if (css.contains("opacity: 1 !important") || css.contains("opacity:1 !important"))
            pres->noDimming = true;

        // lastSlideShowAll: generated CSS has "#impress.all-visible"
        if (css.contains("all-visible"))
            pres->lastSlideShowAll = true;
    }

    // ── data-width / data-height from #impress div ────────────────────────────
    {
        QRegularExpression re(R"(<div\s+id="impress"[^>]*>)",
                               QRegularExpression::DotMatchesEverythingOption);
        auto m = re.match(html);
        if (m.hasMatch()) {
            float dw = attrVal(m.captured(0), "data-width").toFloat();
            float dh = attrVal(m.captured(0), "data-height").toFloat();
            if (dw > 0) pres->slideWidth  = dw;
            if (dh > 0) pres->slideHeight = dh;
        }
    }

    // ── Parse step blocks ─────────────────────────────────────────────────────
    QStringList steps = extractStepBlocks(html);
    if (steps.isEmpty()) {
        errorMsg = "Keine Folien (class=\"step\") in index.html gefunden.";
        delete pres;
        return nullptr;
    }

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

        // Slide name: prefer data-name, fall back to stripping "slide-" from id
        QString dname = attrVal(tag, "data-name");
        QString sid   = attrVal(tag, "id");
        if (!dname.isEmpty())
            slide.name = dname;
        else if (!sid.isEmpty())
            slide.name = sid.startsWith("slide-") ? sid.mid(6) : sid;
        else
            slide.name = QString("Folie %1").arg(pres->slides.size() + 1);

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
                QString danim  = attrVal(dTag, "data-anim");

                // Classify: prefer explicit data-type, fall back to style inspection
                bool isText = (dtype == "text") ||
                              (dtype.isEmpty() && style.contains("font-family"));

                SlideElement e;
                e.x      = cssProp(style, "left").remove("px").toFloat();
                e.y      = cssProp(style, "top").remove("px").toFloat();
                e.width  = cssProp(style, "width").remove("px").toFloat();
                e.height = cssProp(style, "height").remove("px").toFloat();

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
                        // Restore text: unescape HTML entities and <br> → \n
                        elemTxt.replace("<br>", "\n");
                        elemTxt.replace("&amp;", "&").replace("&lt;", "<")
                               .replace("&gt;", ">").replace("&quot;", "\"")
                               .replace("&#39;", "'");
                        e.content = elemTxt;
                    }
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
                }
                // Entrance animation
                if (!danim.isEmpty()) {
                    e.entranceAnim = danim;
                    QString rawDelay = cssProp(style, "--anim-delay");
                    QString rawDur   = cssProp(style, "--anim-dur");
                    if (!rawDelay.isEmpty()) e.animDelay    = rawDelay.remove('s').toFloat();
                    if (!rawDur.isEmpty())   e.animDuration = rawDur.remove('s').toFloat();
                }

                slide.elements.append(e);
            }
        }

        pres->slides.append(slide);
    }

    if (pres->slides.isEmpty()) {
        errorMsg = "Keine Folien geladen.";
        delete pres;
        return nullptr;
    }

    return pres;
}
