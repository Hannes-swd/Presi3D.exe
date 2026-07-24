#pragma once
#include <QString>
#include <QVector>
#include "models/DataModel.h"

// Exports selected slides as a Word (.docx) handout: per slide, the
// largest/topmost text becomes a heading, followed by the remaining text
// and images in on-slide reading order.
class HandoutExporter {
public:
    struct Result {
        bool    ok;
        QString errorMessage;
    };

    enum class HeadingMode { LargestFont, Topmost };
    enum class OrderMode   { ByPosition, ByFontSize };

    struct Options {
        bool includeText         = true;
        bool includeShapeText    = true;
        bool includeImages       = true;
        bool includeTables       = false;
        bool includeIconsButtons = false;

        HeadingMode headingMode = HeadingMode::LargestFont;
        OrderMode   orderMode   = OrderMode::ByPosition;
    };

    static Result exportSlides(Presentation& pres, const QVector<int>& slideIndices,
                                const Options& opts, const QString& filePath);
};
