#include "SlideRenderer.h"
#include "SlideEditor2D.h"

namespace SlideRenderer {

static constexpr int kMargin = 20; // matches SlideEditor2D::slideRect()'s fixed margin

void configure(SlideEditor2D& editor, int pixelWidth, int pixelHeight) {
    editor.resize(pixelWidth + kMargin * 2, pixelHeight + kMargin * 2);
    editor.setRulersVisible(false);
}

QImage renderSlide(SlideEditor2D& editor, Presentation& pres, int slideIndex,
                   int pixelWidth, int pixelHeight) {
    editor.setSlide(&pres, slideIndex);
    editor.zoomReset();

    QImage full(editor.size(), QImage::Format_ARGB32);
    full.fill(Qt::transparent);
    editor.render(&full);
    return full.copy(kMargin, kMargin, pixelWidth, pixelHeight);
}

}
