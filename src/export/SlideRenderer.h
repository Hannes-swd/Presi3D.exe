#pragma once
#include <QImage>
#include "models/DataModel.h"

class SlideEditor2D;

// Shared offscreen-rendering helper used by ImageExporter and PdfExporter:
// draws a slide through SlideEditor2D's own drawElement() (rather than a
// separate re-implementation) so exported pages match the editor exactly.
namespace SlideRenderer {

// Sizes `editor` (freshly constructed, never shown) so that slideRect() maps
// 1:1 onto pixelWidth x pixelHeight and disables rulers, ready for repeated
// renderSlide() calls.
void configure(SlideEditor2D& editor, int pixelWidth, int pixelHeight);

// Renders one slide to a QImage of exactly pixelWidth x pixelHeight, with no
// rulers, selection handles, or scene-background margin.
QImage renderSlide(SlideEditor2D& editor, Presentation& pres, int slideIndex,
                   int pixelWidth, int pixelHeight);

}
