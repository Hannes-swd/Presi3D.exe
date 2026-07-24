#pragma once
#include <QString>
#include <QFont>
#include <QTextLayout>
#include "models/DataModel.h"

// Shared text layout helpers, originally local to SlideEditor2D.cpp. Extracted
// so ShadowRenderer can reproduce the exact same line wrapping/positioning
// when building a glyph-accurate silhouette for Text-element shadows.

// QTextLayout lays out a single paragraph and does NOT treat '\n' (LF) as a
// line break — only QChar::LineSeparator (U+2028) forces one. Our model
// stores plain '\n' (so export's simple "\n" -> "<br>" replace keeps working),
// so every QTextLayout built from element text must substitute LineSeparator
// in first, purely for layout/painting/hit-testing. Both characters are a
// single QChar, so this never shifts any cursor/selection/span offset.
QString layoutText(const QString& s);

QFont elemFont(const SlideElement& e, float scaleY);

// Build a QTextLayout for a text element at widget scale and do line layout.
// The layout origin is at (0,0); add elemToWidget(e).topLeft() to get widget coords.
void buildLayout(QTextLayout& layout, const SlideElement& e,
                  const QFont& font, float width,
                  const QString& alignOverride = {});

// Compute vertical offset caused by middle/bottom alignment of text within element rect.
float textVOff(const QTextLayout& layout, float elemH, const QString& vAlign);
