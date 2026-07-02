#pragma once
#include <QString>
#include <QMap>
#include "models/DataModel.h"

class HtmlExporter {
public:
    struct Result {
        bool    ok;
        QString errorMessage;
    };

    // Export the presentation to outputDir, creating:
    //   index.html, styles.css, impress.js, assets/
    static Result exportTo(const Presentation& pres, const QString& outputDir);

private:
    static QString generateHtml(const Presentation& pres);
    static QString generateCss(const Presentation& pres);
    static QString slideToHtml(const Slide& slide, int index,
                               const QMap<QString, QString>& uuidToHtmlId,
                               const QMap<QString, QString>& uuidToVisString);
    static QString elementToHtml(const SlideElement& elem,
                                 const QMap<QString, QString>& uuidToHtmlId = {});
    static QString colorToCss(const QColor& c);
    static bool    copyImages(const Presentation& pres,
                              const QString& assetsDir,
                              QStringList& errors);
};
