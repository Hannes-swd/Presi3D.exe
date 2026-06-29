#pragma once
#include <QString>
#include "models/DataModel.h"

class HtmlImporter {
public:
    // Reads an exported folder (must contain index.html and styles.css).
    // Returns a new Presentation on success (caller owns it), nullptr on error.
    static Presentation* importFrom(const QString& folderPath, QString& errorMsg);
};
