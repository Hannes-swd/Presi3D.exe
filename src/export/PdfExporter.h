#pragma once
#include <QString>
#include <QVector>
#include "models/DataModel.h"

// Exports selected slides into a single multi-page PDF (one slide per page),
// reusing SlideEditor2D's rendering via SlideRenderer so pages match the
// editor exactly.
class PdfExporter {
public:
    struct Result {
        bool    ok;
        QString errorMessage;
    };

    // slideIndices: presentation order, any subset. filePath: full path to
    // the .pdf file to write (created/overwritten).
    static Result exportSlides(Presentation& pres, const QVector<int>& slideIndices,
                               const QString& filePath,
                               int pixelWidth = 1920, int pixelHeight = 1080);
};
