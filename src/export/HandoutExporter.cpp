#include "HandoutExporter.h"
#include "ZipWriter.h"
#include <QImage>
#include <QBuffer>
#include <QFileInfo>
#include <QDir>
#include <QStringList>
#include <algorithm>
#include <cmath>

namespace {

// One piece of on-slide content, gathered according to the enabled
// checkboxes, before ordering/heading selection is applied.
struct ContentBlock {
    float   x = 0.f, y = 0.f;
    int     fontSize  = 0;
    bool    isImage   = false;
    bool    isHeading = false;
    QString text;      // text blocks
    QString imagePath; // image blocks
};

QString xmlEscape(QString s) {
    s.replace('&', "&amp;");
    s.replace('<', "&lt;");
    s.replace('>', "&gt;");
    s.replace('"', "&quot;");
    return s;
}

QString tableToText(const SlideElement& e) {
    QStringList rowLines;
    for (const auto& row : e.tableCells) {
        QStringList cells;
        for (const auto& cell : row)
            if (!cell.merged) cells << cell.text;
        rowLines << cells.join(" | ");
    }
    return rowLines.join('\n');
}

QVector<ContentBlock> collectBlocks(const Slide& slide, const HandoutExporter::Options& opts) {
    QVector<ContentBlock> blocks;
    for (const SlideElement& e : slide.elements) {
        switch (e.type) {
        case SlideElement::Text:
            if (opts.includeText && !e.content.trimmed().isEmpty())
                blocks.append({ e.x, e.y, e.fontSize, false, false, e.content, {} });
            break;
        case SlideElement::Shape:
            if (opts.includeShapeText && !e.shapeText.trimmed().isEmpty())
                blocks.append({ e.x, e.y, e.fontSize, false, false, e.shapeText, {} });
            break;
        case SlideElement::Image:
            if (opts.includeImages && !e.content.isEmpty())
                blocks.append({ e.x, e.y, 0, true, false, {}, e.content });
            break;
        case SlideElement::Table: {
            if (!opts.includeTables) break;
            QString text = tableToText(e);
            if (!text.trimmed().isEmpty())
                blocks.append({ e.x, e.y, e.tableFontSize, false, false, text, {} });
            break;
        }
        case SlideElement::Button:
            if (opts.includeIconsButtons && !e.content.trimmed().isEmpty())
                blocks.append({ e.x, e.y, e.fontSize, false, false, "Button: " + e.content, {} });
            break;
        case SlideElement::Checkbox:
            if (opts.includeIconsButtons && !e.content.trimmed().isEmpty())
                blocks.append({ e.x, e.y, e.fontSize, false, false, "Checkbox: " + e.content, {} });
            break;
        case SlideElement::Icon:
            if (opts.includeIconsButtons && !e.content.trimmed().isEmpty())
                blocks.append({ e.x, e.y, e.fontSize, false, false, "Icon: " + e.content, {} });
            break;
        default:
            break; // Chart/Formula/IFrame/Slider/Video/Audio: out of scope for v1
        }
    }
    return blocks;
}

void markHeading(QVector<ContentBlock>& blocks, HandoutExporter::HeadingMode mode) {
    int bestIdx = -1;
    for (int i = 0; i < blocks.size(); ++i) {
        if (blocks[i].isImage) continue;
        if (bestIdx == -1) { bestIdx = i; continue; }
        if (mode == HandoutExporter::HeadingMode::LargestFont) {
            if (blocks[i].fontSize > blocks[bestIdx].fontSize ||
                (blocks[i].fontSize == blocks[bestIdx].fontSize && blocks[i].y < blocks[bestIdx].y))
                bestIdx = i;
        } else { // Topmost
            if (blocks[i].y < blocks[bestIdx].y ||
                (blocks[i].y == blocks[bestIdx].y && blocks[i].x < blocks[bestIdx].x))
                bestIdx = i;
        }
    }
    if (bestIdx != -1) blocks[bestIdx].isHeading = true;
}

void sortBlocks(QVector<ContentBlock>& blocks, HandoutExporter::OrderMode mode) {
    if (mode == HandoutExporter::OrderMode::ByPosition) {
        std::stable_sort(blocks.begin(), blocks.end(), [](const ContentBlock& a, const ContentBlock& b) {
            if (a.y != b.y) return a.y < b.y;
            return a.x < b.x;
        });
    } else { // ByFontSize, largest first
        std::stable_sort(blocks.begin(), blocks.end(), [](const ContentBlock& a, const ContentBlock& b) {
            if (a.fontSize != b.fontSize) return a.fontSize > b.fontSize;
            return a.y < b.y;
        });
    }
}

// Text formatting is driven entirely by the "Normal"/"Heading1" styles
// defined in word/styles.xml (standard Word point sizes/fonts) — paragraphs
// only reference the style, they never carry ad hoc direct sz/b formatting.
QString textParagraphXml(const QString& line, bool heading) {
    const char* style = heading ? "Heading1" : "Normal";
    if (line.trimmed().isEmpty())
        return QString("<w:p><w:pPr><w:pStyle w:val=\"%1\"/></w:pPr></w:p>").arg(style);
    return QString(
        "<w:p><w:pPr><w:pStyle w:val=\"%1\"/></w:pPr>"
        "<w:r><w:t xml:space=\"preserve\">%2</w:t></w:r></w:p>")
        .arg(style)
        .arg(xmlEscape(line));
}

// Standard Word default look: Normal = Calibri 11pt, Heading1 = Calibri
// Light 16pt in Word's default accent blue. These are the same values a
// fresh Word document ships with, so the handout reads like an ordinary
// Word document instead of a bespoke, per-slide-scaled font size.
const char* kStylesXml =
    "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
    "<w:styles xmlns:w=\"http://schemas.openxmlformats.org/wordprocessingml/2006/main\">"
    "<w:docDefaults>"
    "<w:rPrDefault><w:rPr>"
    "<w:rFonts w:ascii=\"Calibri\" w:hAnsi=\"Calibri\" w:cs=\"Calibri\"/>"
    "<w:sz w:val=\"22\"/><w:szCs w:val=\"22\"/>"
    "</w:rPr></w:rPrDefault>"
    "<w:pPrDefault><w:pPr><w:spacing w:after=\"160\" w:line=\"259\" w:lineRule=\"auto\"/></w:pPr></w:pPrDefault>"
    "</w:docDefaults>"
    "<w:style w:type=\"paragraph\" w:default=\"1\" w:styleId=\"Normal\">"
    "<w:name w:val=\"Normal\"/><w:qFormat/>"
    "</w:style>"
    "<w:style w:type=\"paragraph\" w:styleId=\"Heading1\">"
    "<w:name w:val=\"heading 1\"/>"
    "<w:basedOn w:val=\"Normal\"/><w:next w:val=\"Normal\"/><w:qFormat/>"
    "<w:pPr><w:keepNext/><w:spacing w:before=\"240\" w:after=\"120\"/><w:outlineLvl w:val=\"0\"/></w:pPr>"
    "<w:rPr>"
    "<w:rFonts w:ascii=\"Calibri Light\" w:hAnsi=\"Calibri Light\" w:cs=\"Calibri Light\"/>"
    "<w:color w:val=\"2E74B5\"/><w:sz w:val=\"32\"/><w:szCs w:val=\"32\"/>"
    "</w:rPr>"
    "</w:style>"
    "</w:styles>";

QString pageBreakParagraphXml() {
    return "<w:p><w:pPr><w:pStyle w:val=\"Normal\"/></w:pPr><w:r><w:br w:type=\"page\"/></w:r></w:p>";
}

QString imageParagraphXml(const QString& rId, int docPrId, qint64 cx, qint64 cy) {
    return QString(
        "<w:p><w:pPr><w:pStyle w:val=\"Normal\"/></w:pPr><w:r><w:drawing>"
        "<wp:inline distT=\"0\" distB=\"0\" distL=\"0\" distR=\"0\">"
        "<wp:extent cx=\"%1\" cy=\"%2\"/>"
        "<wp:docPr id=\"%3\" name=\"Picture%3\"/>"
        "<wp:cNvGraphicFramePr><a:graphicFrameLocks noChangeAspect=\"1\"/></wp:cNvGraphicFramePr>"
        "<a:graphic><a:graphicData uri=\"http://schemas.openxmlformats.org/drawingml/2006/picture\">"
        "<pic:pic><pic:nvPicPr><pic:cNvPr id=\"%3\" name=\"Picture%3\"/><pic:cNvPicPr/></pic:nvPicPr>"
        "<pic:blipFill><a:blip r:embed=\"%4\"/><a:stretch><a:fillRect/></a:stretch></pic:blipFill>"
        "<pic:spPr><a:xfrm><a:off x=\"0\" y=\"0\"/><a:ext cx=\"%1\" cy=\"%2\"/></a:xfrm>"
        "<a:prstGeom prst=\"rect\"><a:avLst/></a:prstGeom></pic:spPr>"
        "</pic:pic></a:graphicData></a:graphic></wp:inline></w:drawing></w:r></w:p>")
        .arg(cx).arg(cy).arg(docPrId).arg(rId);
}

} // namespace

HandoutExporter::Result HandoutExporter::exportSlides(Presentation& pres, const QVector<int>& slideIndices,
                                                        const Options& opts, const QString& filePath) {
    if (slideIndices.isEmpty())
        return { false, "No slides selected." };

    // A4 page, 1in margins, in twips (1/1440 in).
    constexpr int    kPageWidthTwips    = 11906;
    constexpr int    kPageHeightTwips   = 16838;
    constexpr int    kMarginTwips       = 1440;
    constexpr qint64 kEmuPerTwip        = 635;
    constexpr qint64 kEmuPerPx          = 9525; // assumes 96 DPI source images
    const qint64      contentWidthEmu   = qint64(kPageWidthTwips - 2 * kMarginTwips) * kEmuPerTwip;
    const qint64      maxImageHeightEmu = qint64(5.5 * 914400);

    QString body;
    bool    firstOutputSlide = true;
    int     imageCounter     = 0;
    QVector<QPair<QString, QByteArray>> mediaFiles; // "imageN.png" -> PNG bytes

    for (int idx : slideIndices) {
        if (idx < 0 || idx >= pres.slides.size()) continue;
        const Slide& slide = pres.slides[idx];

        QVector<ContentBlock> blocks = collectBlocks(slide, opts);
        if (blocks.isEmpty()) continue;

        markHeading(blocks, opts.headingMode);
        sortBlocks(blocks, opts.orderMode);

        if (!firstOutputSlide) body += pageBreakParagraphXml();
        firstOutputSlide = false;

        for (const ContentBlock& block : blocks) {
            if (block.isImage) {
                QImage img(block.imagePath);
                if (img.isNull()) {
                    body += textParagraphXml(
                        QString("[Bild fehlt: %1]").arg(QFileInfo(block.imagePath).fileName()), false);
                    continue;
                }

                QByteArray pngBytes;
                {
                    QBuffer buf(&pngBytes);
                    buf.open(QIODevice::WriteOnly);
                    img.save(&buf, "PNG");
                }

                qint64 rawW = qint64(img.width())  * kEmuPerPx;
                qint64 rawH = qint64(img.height()) * kEmuPerPx;
                double scale = 1.0;
                if (rawW > contentWidthEmu) scale = std::min(scale, contentWidthEmu / double(rawW));
                if (rawH * scale > maxImageHeightEmu) scale = std::min(scale, maxImageHeightEmu / double(rawH));
                qint64 cx = qint64(std::llround(rawW * scale));
                qint64 cy = qint64(std::llround(rawH * scale));

                ++imageCounter;
                // rId1 is reserved for the styles.xml relationship (see below).
                QString rId = QString("rId%1").arg(imageCounter + 1);
                mediaFiles.append({ QString("image%1.png").arg(imageCounter), pngBytes });

                body += imageParagraphXml(rId, imageCounter, cx, cy);
            } else {
                const QStringList lines = QString(block.text).replace("\r\n", "\n").replace('\r', '\n').split('\n');
                for (const QString& line : lines)
                    body += textParagraphXml(line, block.isHeading);
            }
        }
    }

    if (firstOutputSlide) // nothing was ever emitted
        return { false, "No content matched the selected options." };

    QString documentXml = QString(
        "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
        "<w:document xmlns:w=\"http://schemas.openxmlformats.org/wordprocessingml/2006/main\" "
        "xmlns:r=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships\" "
        "xmlns:wp=\"http://schemas.openxmlformats.org/drawingml/2006/wordprocessingDrawing\" "
        "xmlns:a=\"http://schemas.openxmlformats.org/drawingml/2006/main\" "
        "xmlns:pic=\"http://schemas.openxmlformats.org/drawingml/2006/picture\">"
        "<w:body>%1<w:sectPr>"
        "<w:pgSz w:w=\"%2\" w:h=\"%3\"/>"
        "<w:pgMar w:top=\"%4\" w:right=\"%4\" w:bottom=\"%4\" w:left=\"%4\" w:header=\"708\" w:footer=\"708\" w:gutter=\"0\"/>"
        "</w:sectPr></w:body></w:document>")
        .arg(body)
        .arg(kPageWidthTwips).arg(kPageHeightTwips).arg(kMarginTwips);

    const bool hasImages = !mediaFiles.isEmpty();

    QString contentTypesXml =
        "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
        "<Types xmlns=\"http://schemas.openxmlformats.org/package/2006/content-types\">"
        "<Default Extension=\"rels\" ContentType=\"application/vnd.openxmlformats-package.relationships+xml\"/>"
        + QString(hasImages ? "<Default Extension=\"png\" ContentType=\"image/png\"/>" : "") +
        "<Override PartName=\"/word/document.xml\" "
        "ContentType=\"application/vnd.openxmlformats-officedocument.wordprocessingml.document.main+xml\"/>"
        "<Override PartName=\"/word/styles.xml\" "
        "ContentType=\"application/vnd.openxmlformats-officedocument.wordprocessingml.styles+xml\"/>"
        "</Types>";

    QString rootRelsXml =
        "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
        "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">"
        "<Relationship Id=\"rId1\" "
        "Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument\" "
        "Target=\"word/document.xml\"/>"
        "</Relationships>";

    // rId1 = styles.xml, rId2..N+1 = images (see the +1 offset above).
    QString docRelsXml =
        "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
        "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">"
        "<Relationship Id=\"rId1\" "
        "Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/styles\" "
        "Target=\"styles.xml\"/>";
    for (int i = 0; i < mediaFiles.size(); ++i) {
        docRelsXml += QString(
            "<Relationship Id=\"rId%1\" "
            "Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/image\" "
            "Target=\"media/%2\"/>").arg(i + 2).arg(mediaFiles[i].first);
    }
    docRelsXml += "</Relationships>";

    ZipWriter zip;
    zip.addFile("[Content_Types].xml", contentTypesXml.toUtf8());
    zip.addFile("_rels/.rels", rootRelsXml.toUtf8());
    zip.addFile("word/document.xml", documentXml.toUtf8());
    zip.addFile("word/styles.xml", QByteArray(kStylesXml));
    zip.addFile("word/_rels/document.xml.rels", docRelsXml.toUtf8());

    for (const auto& media : mediaFiles)
        zip.addFile("word/media/" + media.first, media.second);

    if (!zip.save(filePath))
        return { false, QString("Could not write %1").arg(filePath) };

    return { true, {} };
}
