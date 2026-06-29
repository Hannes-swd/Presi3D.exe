#include "HtmlExporter.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>

// Bundled impress.js — copied from local installation
static const char* IMPRESS_JS_SRC =
    "C:/Users/hanne/Web/impress.js lernen/impress.js";

HtmlExporter::Result HtmlExporter::exportTo(const Presentation& pres,
                                             const QString& outputDir) {
    QDir dir(outputDir);
    if (!dir.exists() && !dir.mkpath("."))
        return {false, "Ordner konnte nicht erstellt werden:\n" + outputDir};

    dir.mkpath("assets");
    QString assetsPath = dir.filePath("assets");

    // Copy impress.js
    QString jsDest = dir.filePath("impress.js");
    if (!QFile::exists(jsDest)) {
        if (!QFile::copy(QString::fromUtf8(IMPRESS_JS_SRC), jsDest))
            return {false, QString("impress.js konnte nicht kopiert werden.\n"
                                   "Erwartet unter: %1").arg(IMPRESS_JS_SRC)};
    }

    QStringList imgErrors;
    copyImages(pres, assetsPath, imgErrors);

    // styles.css
    {
        QFile f(dir.filePath("styles.css"));
        if (!f.open(QIODevice::WriteOnly | QIODevice::Text))
            return {false, "Kann styles.css nicht schreiben."};
        QTextStream s(&f);
        s << generateCss(pres);
    }

    // index.html
    {
        QFile f(dir.filePath("index.html"));
        if (!f.open(QIODevice::WriteOnly | QIODevice::Text))
            return {false, "Kann index.html nicht schreiben."};
        QTextStream s(&f);
        s.setEncoding(QStringConverter::Utf8);
        s << generateHtml(pres);
    }

    QString warn;
    if (!imgErrors.isEmpty())
        warn = "\n\nWarnung – folgende Bilder konnten nicht kopiert werden:\n"
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

QString HtmlExporter::generateHtml(const Presentation& pres) {
    QMap<QString, QString> uuidToHtmlId;
    QMap<QString, QString> uuidToVisString;
    buildVisibilityData(pres, uuidToHtmlId, uuidToVisString);

    QString html;
    QTextStream out(&html);

    double defOpa = qBound(0.0, double(pres.defaultInactiveOpacity), 1.0);

    out << "<!DOCTYPE html>\n"
        << "<html lang=\"de\">\n"
        << "<head>\n"
        << "  <meta charset=\"UTF-8\">\n"
        << "  <title>Präsentation</title>\n"
        << "  <link rel=\"stylesheet\" href=\"styles.css\">\n"
        << "</head>\n"
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
        << "steps.forEach(function(step) { step.style.opacity = defaultInactiveOpacity; });\n"
        << "\n"
        << "document.addEventListener('impress:stepenter', function(e) {\n"
        << "  var activeId = e.target.id;\n"
        << "  var overrides = slideVisibility[activeId] || {};\n"
        << "  steps.forEach(function(step) {\n"
        << "    if (step.id === activeId) {\n"
        << "      step.style.opacity = '1';\n"
        << "    } else {\n"
        << "      var opa = (step.id in overrides) ? overrides[step.id] : defaultInactiveOpacity;\n"
        << "      step.style.opacity = String(opa);\n"
        << "    }\n"
        << "  });\n"
        << "  var el = document.getElementById('slide-counter');\n"
        << "  var i = steps.indexOf(e.target) + 1;\n"
        << "  if (el) el.textContent = i + ' / ' + steps.length;\n"
        << "});\n"
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
        out << "    " << elementToHtml(elem) << "\n";

    out << "  </div>";
    return html;
}

QString HtmlExporter::elementToHtml(const SlideElement& e) {
    QString base = QString("position:absolute;left:%1px;top:%2px;width:%3px;height:%4px;")
                       .arg(int(e.x)).arg(int(e.y)).arg(int(e.width)).arg(int(e.height));

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
        if (e.content == "circle")
            style += "border-radius:50%;";
        else if (e.cornerRadius > 0)
            style += QString("border-radius:%1px;").arg(int(e.cornerRadius));
        if (!e.entranceAnim.isEmpty()) {
            style += QString("--anim-delay:%1s;--anim-dur:%2s;")
                         .arg(e.animDelay, 0, 'f', 2).arg(e.animDuration, 0, 'f', 2);
            return QString("<div data-type=\"shape\" data-shape=\"%1\" data-anim=\"%2\" style=\"%3\"></div>")
                       .arg(e.content, e.entranceAnim, style);
        }
        return QString("<div data-type=\"shape\" data-shape=\"%1\" style=\"%2\"></div>")
                   .arg(e.content, style);

    } else if (e.type == SlideElement::Image) {
        QFileInfo fi(e.content);
        QString src = fi.exists() ? "assets/" + fi.fileName() : e.content;
        return QString("<img data-type=\"image\" src=\"%1\" style=\"%2object-fit:contain;\" alt=\"\">")
                   .arg(src, base);
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
                errors << "Nicht gefunden: " + elem.content; ok = false; continue;
            }
            QString dest = QDir(assetsDir).filePath(fi.fileName());
            if (!QFile::exists(dest) && !QFile::copy(elem.content, dest)) {
                errors << "Kopieren fehlgeschlagen: " + elem.content; ok = false;
            }
        }
    }
    return ok;
}
