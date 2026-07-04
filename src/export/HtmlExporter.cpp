#include "HtmlExporter.h"
#include "ShapeUtils.h"
#include "rendering/ChartRenderer.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QImage>
#include <QPainter>
#include <QBuffer>
#include <QJsonDocument>

// Bundled impress.js — copied from local installation
static const char* IMPRESS_JS_SRC =
    "C:/Users/hanne/Web/impress.js lernen/impress.js";

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

/* Entrance animations */
.step [data-anim] { opacity: 0; }
.step.present [data-anim="fadeIn"]    { animation: impressFadeIn    var(--anim-dur,0.5s) var(--anim-delay,0.3s) both; }
.step.present [data-anim="slideLeft"] { animation: impressSlideLeft var(--anim-dur,0.5s) var(--anim-delay,0.3s) both; }
.step.present [data-anim="slideRight"]{ animation: impressSlideRight var(--anim-dur,0.5s) var(--anim-delay,0.3s) both; }
.step.present [data-anim="slideUp"]   { animation: impressSlideUp   var(--anim-dur,0.5s) var(--anim-delay,0.3s) both; }
.step.present [data-anim="slideDown"] { animation: impressSlideDown var(--anim-dur,0.5s) var(--anim-delay,0.3s) both; }
.step.present [data-anim="zoomIn"]    { animation: impressZoomIn    var(--anim-dur,0.5s) var(--anim-delay,0.3s) both; }

@keyframes impressFadeIn    { from{opacity:0}               to{opacity:1} }
@keyframes impressSlideLeft { from{transform:translateX(-60px);opacity:0} to{transform:none;opacity:1} }
@keyframes impressSlideRight{ from{transform:translateX(60px);opacity:0}  to{transform:none;opacity:1} }
@keyframes impressSlideUp   { from{transform:translateY(-60px);opacity:0} to{transform:none;opacity:1} }
@keyframes impressSlideDown { from{transform:translateY(60px);opacity:0}  to{transform:none;opacity:1} }
@keyframes impressZoomIn    { from{transform:scale(0.4);opacity:0}        to{transform:none;opacity:1} }
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

// Does any slide contain a formula element? Only then load MathJax.
static bool hasFormulaElement(const Presentation& pres) {
    for (const Slide& s : pres.slides)
        for (const SlideElement& e : s.elements)
            if (e.type == SlideElement::Formula) return true;
    return false;
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
        << "  <title>Presentation</title>\n"
        << "  <link rel=\"stylesheet\" href=\"styles.css\">\n";
    if (hasFormulaElement(pres))
        out << "  <script id=\"MathJax-script\" async "
               "src=\"https://cdn.jsdelivr.net/npm/mathjax@3/es5/tex-mml-chtml.js\"></script>\n";
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
        out << slideToHtml(pres.slides[i], i + 1, uuidToHtmlId, uuidToVisString) << "\n\n";

    out << "</div>\n\n"
        << "<script src=\"impress.js\"></script>\n"
        << "<script>\n"
        << "var api = impress();\n"
        << "api.init();\n"
        << "var steps = Array.from(document.querySelectorAll('.step'));\n"
        << "var impEl = document.getElementById('impress');\n"
        << "var defaultInactiveOpacity = parseFloat(impEl.dataset.defaultInactiveOpacity || 0.3);\n"
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
        << "});\n"
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

QString HtmlExporter::slideToHtml(const Slide& s, int index,
                                   const QMap<QString, QString>& uuidToHtmlId,
                                   const QMap<QString, QString>& uuidToVisString) {
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

    out << "       style=\""          << stepStyle << "\">\n";

    for (const auto& elem : s.elements)
        out << "    " << elementToHtml(elem, uuidToHtmlId) << "\n";

    out << "  </div>";
    return html;
}

QString HtmlExporter::elementToHtml(const SlideElement& e,
                                    const QMap<QString, QString>& uuidToHtmlId) {
    QString base = QString("position:absolute;left:%1px;top:%2px;width:%3px;height:%4px;")
                       .arg(int(e.x)).arg(int(e.y)).arg(int(e.width)).arg(int(e.height));
    if (e.rotation != 0.f)
        base += QString("transform:rotate(%1deg);transform-origin:center;")
                    .arg(double(e.rotation), 0, 'f', 2);

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
            return QString("<div data-type=\"text\" data-list=\"%1\" style=\"%2\">"
                           "<%3 style=\"margin:0;padding:0 0 0 1.3em;list-style:%4;\">%5</%3></div>")
                       .arg(dataList, style, listTag, listType, items);
        }

        QString text = e.content.toHtmlEscaped().replace("\n", "<br>");
        if (!e.hyperlink.trimmed().isEmpty()) {
            QString href = e.hyperlink.trimmed().toHtmlEscaped();
            text = QString("<a href=\"%1\" target=\"_blank\" rel=\"noopener noreferrer\" "
                           "style=\"color:inherit;text-decoration:inherit;\">%2</a>")
                       .arg(href, text);
        }
        if (!e.entranceAnim.isEmpty()) {
            style += QString("--anim-delay:%1s;--anim-dur:%2s;")
                         .arg(e.animDelay, 0, 'f', 2).arg(e.animDuration, 0, 'f', 2);
            return QString("<div data-type=\"text\" data-anim=\"%1\" style=\"%2\">%3</div>")
                       .arg(e.entranceAnim, style, text);
        }
        return QString("<div data-type=\"text\" style=\"%1\">%2</div>").arg(style, text);

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
            innerText = QString("<span style=\"%1\">%2</span>")
                            .arg(textStyle, e.shapeText.toHtmlEscaped().replace("\n", "<br>"));
        }
        if (!e.entranceAnim.isEmpty()) {
            style += QString("--anim-delay:%1s;--anim-dur:%2s;")
                         .arg(e.animDelay, 0, 'f', 2).arg(e.animDuration, 0, 'f', 2);
            return QString("<div data-type=\"shape\" data-shape=\"%1\" data-anim=\"%2\" style=\"%3\">%4</div>")
                       .arg(e.content, e.entranceAnim, style, innerText);
        }
        return QString("<div data-type=\"shape\" data-shape=\"%1\" style=\"%2\">%3</div>")
                   .arg(e.content, style, innerText);

    } else if (e.type == SlideElement::Image) {
        QFileInfo fi(e.content);
        QString src = fi.exists() ? "assets/" + fi.fileName() : e.content;
        return QString("<img data-type=\"image\" src=\"%1\" style=\"%2object-fit:contain;\" alt=\"\">")
                   .arg(src, base);

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

        QString html = QString("<div data-type=\"table\" style=\"%1\">").arg(tableStyle);
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

                html += QString("<%1%2 style=\"%3\">%4</%1>")
                            .arg(tag, spanAttrs, cellStyle,
                                 cell.text.toHtmlEscaped().replace("\n", "<br>"));
            }
            html += "</tr>";
        }

        html += "</table></div>";
        return html;

    } else if (e.type == SlideElement::Chart) {
        // Render preview image
        int iw = qMax(4, int(e.width));
        int ih = qMax(4, int(e.height));
        QImage img(iw, ih, QImage::Format_ARGB32);
        img.fill(Qt::white);
        QPainter painter(&img);
        painter.setRenderHint(QPainter::Antialiasing);
        ChartRenderer::paint(painter, QRectF(0, 0, iw, ih), e.chartData);
        painter.end();

        QByteArray imgBa;
        QBuffer buf(&imgBa);
        buf.open(QIODevice::WriteOnly);
        img.save(&buf, "PNG");

        // Embed the full chart data as base64-encoded JSON for lossless reload
        QByteArray jsonBa = QJsonDocument(e.chartData.toJson()).toJson(QJsonDocument::Compact);
        QString chartDataB64 = jsonBa.toBase64();

        QString b64img = imgBa.toBase64();
        QString title  = e.chartData.title.isEmpty() ? ChartRenderer::typeName(e.chartData.type)
                                                      : e.chartData.title;
        return QString(
            "<div data-type=\"chart\" data-chart-type=\"%1\" data-chart=\"%2\" style=\"%3\">"
            "<img src=\"data:image/png;base64,%4\" "
            "style=\"width:100%;height:100%;object-fit:contain;\" alt=\"%5\">"
            "</div>")
            .arg(e.chartData.type, chartDataB64, base, b64img, title.toHtmlEscaped());

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
            "<div data-type=\"formula\" data-latex=\"%1\" style=\"%2\">"
            "<div class=\"math\">$$%3$$</div></div>")
            .arg(latex, style, latex);

    } else if (e.type == SlideElement::IFrame) {
        if (e.content.trimmed().isEmpty()) return {};
        QString url = e.content.toHtmlEscaped();
        // data-portal-id: browsers render iframes inside a CSS 3D-transformed
        // ancestor (how impress.js positions every step) as a flattened,
        // non-interactive image. The exported JS "portals" a live clone of
        // this iframe to a position:fixed element outside the 3D context
        // while its slide is active, keyed by this id.
        return QString(
            "<div data-type=\"iframe-wrap\" data-portal-id=\"%3\" style=\"%1overflow:hidden;\">"
            "<iframe data-type=\"iframe\" src=\"%2\" "
            "style=\"position:absolute;inset:0;width:100%;height:100%;border:none;\" "
            "allow=\"accelerometer; autoplay; clipboard-write; encrypted-media; gyroscope; picture-in-picture\" "
            "allowfullscreen loading=\"lazy\"></iframe>"
            "<a href=\"%2\" target=\"_blank\" rel=\"noopener\" "
            "style=\"position:absolute;top:4px;right:4px;z-index:2;background:rgba(17,17,17,.75);"
            "color:#fff;font:11px sans-serif;padding:3px 7px;border-radius:4px;text-decoration:none;\" "
            "title=\"If the page blocks embedding: open here in a new tab\">↗ Open</a>"
            "</div>")
            .arg(base, url, e.id);

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

        QString label = e.content.isEmpty() ? "Next" : e.content.toHtmlEscaped();
        QString targetHtmlId = uuidToHtmlId.value(e.targetSlideId);
        if (targetHtmlId.isEmpty())
            return QString("<div data-type=\"button\" style=\"%1opacity:.5;\">%2</div>").arg(style, label);

        return QString("<a data-type=\"button\" data-target=\"%1\" href=\"#%1\" "
                       "onclick=\"api.goto('%1');return false;\" style=\"%2\">%3</a>")
                   .arg(targetHtmlId, style, label);
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

bool HtmlExporter::copyImages(const Presentation& pres,
                               const QString& assetsDir, QStringList& errors) {
    bool ok = true;
    for (const auto& slide : pres.slides) {
        for (const auto& elem : slide.elements) {
            if (elem.type != SlideElement::Image || elem.content.isEmpty()) continue;
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
