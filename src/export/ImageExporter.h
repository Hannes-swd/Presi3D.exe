#pragma once
#include <QString>
#include <QVector>
#include "models/DataModel.h"

// Renders selected slides to standalone PNG images, reusing SlideEditor2D's
// on-screen rendering (drawElement) so the images match the editor exactly.
class ImageExporter {
public:
    struct Result {
        bool    ok;
        QString errorMessage;
    };

    // Renders each slide index in `slideIndices` (in presentation order) to a
    // PNG file in outputDir, named "01-<slide-name>.png", "02-...", etc.
    // pixelWidth/pixelHeight set the output resolution (default 1920x1080).
    static Result exportSlides(Presentation& pres, const QVector<int>& slideIndices,
                               const QString& outputDir,
                               int pixelWidth = 1920, int pixelHeight = 1080);
};
