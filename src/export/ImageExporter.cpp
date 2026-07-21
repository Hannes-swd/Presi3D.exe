#include "ImageExporter.h"
#include "SlideRenderer.h"
#include "SlideEditor2D.h"
#include <QDir>
#include <QRegularExpression>

static QString sanitizeFileName(const QString& name) {
    QString s = name;
    s.replace(QRegularExpression("[\\\\/:*?\"<>|]"), "_");
    s = s.trimmed();
    return s.isEmpty() ? "slide" : s;
}

ImageExporter::Result ImageExporter::exportSlides(Presentation& pres, const QVector<int>& slideIndices,
                                                   const QString& outputDir,
                                                   int pixelWidth, int pixelHeight) {
    if (slideIndices.isEmpty())
        return { false, "No slides selected." };

    QDir().mkpath(outputDir);

    SlideEditor2D editor;
    SlideRenderer::configure(editor, pixelWidth, pixelHeight);

    const int numWidth = QString::number(pres.slides.size()).length();

    for (int idx : slideIndices) {
        if (idx < 0 || idx >= pres.slides.size()) continue;

        QImage img = SlideRenderer::renderSlide(editor, pres, idx, pixelWidth, pixelHeight);

        QString baseName = QString("%1-%2")
            .arg(idx + 1, numWidth, 10, QChar('0'))
            .arg(sanitizeFileName(pres.slides[idx].name));
        QString filePath = QDir(outputDir).filePath(baseName + ".png");

        if (!img.save(filePath, "PNG"))
            return { false, QString("Could not save %1").arg(filePath) };
    }

    return { true, {} };
}
