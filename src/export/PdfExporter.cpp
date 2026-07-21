#include "PdfExporter.h"
#include "SlideRenderer.h"
#include "SlideEditor2D.h"
#include <QPdfWriter>
#include <QPainter>
#include <QPageSize>
#include <QFileInfo>
#include <QDir>

PdfExporter::Result PdfExporter::exportSlides(Presentation& pres, const QVector<int>& slideIndices,
                                              const QString& filePath,
                                              int pixelWidth, int pixelHeight) {
    if (slideIndices.isEmpty())
        return { false, "No slides selected." };

    QDir().mkpath(QFileInfo(filePath).absolutePath());

    const int dpi = 150;

    QPdfWriter writer(filePath);
    writer.setResolution(dpi);
    writer.setPageSize(QPageSize(QSizeF(pixelWidth, pixelHeight) * 25.4 / dpi, QPageSize::Millimeter));
    writer.setPageMargins(QMarginsF(0, 0, 0, 0));

    QPainter painter;
    if (!painter.begin(&writer))
        return { false, QString("Could not create PDF at %1").arg(filePath) };

    SlideEditor2D editor;
    SlideRenderer::configure(editor, pixelWidth, pixelHeight);

    bool first = true;
    for (int idx : slideIndices) {
        if (idx < 0 || idx >= pres.slides.size()) continue;
        if (!first) writer.newPage();
        first = false;

        QImage img = SlideRenderer::renderSlide(editor, pres, idx, pixelWidth, pixelHeight);
        painter.drawImage(QRect(0, 0, writer.width(), writer.height()), img);
    }
    painter.end();

    return { true, {} };
}
