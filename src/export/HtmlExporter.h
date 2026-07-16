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
    static QString slideToHtml(const Slide& slide, int index, int slideCount,
                               const QMap<QString, QString>& uuidToHtmlId,
                               const QMap<QString, QString>& uuidToVisString,
                               const VariableSet& vars);
    static QString buildVariablesJson(const Presentation& pres,
                                      const QMap<QString, QString>& uuidToHtmlId);
    static QString elementToHtml(const SlideElement& elem,
                                 const QMap<QString, QString>& uuidToHtmlId,
                                 const VariableSet& vars,
                                 const QString& currentSlideId,
                                 int slideNumber, int slideCount);
    static QString colorToCss(const QColor& c);
    static bool    copyImages(const Presentation& pres,
                              const QString& assetsDir,
                              QStringList& errors);
    static bool    copyModels(const Presentation& pres,
                              const QString& assetsDir,
                              QStringList& errors);
    static void    cleanupOrphanedAssets(const Presentation& pres,
                                         const QString& assetsDir);
    static QString worldObjectToHtml(const WorldObject& w);
};
