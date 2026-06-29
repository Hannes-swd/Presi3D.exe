#include "DataModel.h"
#include <QUuid>

void Presentation::addSlide(const QString& name) {
    Slide s;
    s.name = name.isEmpty() ? QStringLiteral("Slide %1").arg(slides.size() + 1) : name;
    if (!slides.isEmpty())
        s.posX = slides.last().posX + 2000.f;
    slides.append(s);
    modified = true;
}

void Presentation::removeSlide(int index) {
    if (index >= 0 && index < slides.size()) {
        slides.removeAt(index);
        modified = true;
    }
}

void Presentation::duplicateSlide(int index) {
    if (index < 0 || index >= slides.size()) return;
    Slide copy = slides[index];
    copy.id   = QUuid::createUuid().toString(QUuid::WithoutBraces);
    copy.name = copy.name + QStringLiteral(" (Kopie)");
    copy.posX += 200.f;
    copy.posY += 200.f;
    for (auto& elem : copy.elements)
        elem.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    slides.insert(index + 1, copy);
    modified = true;
}

void Presentation::moveSlide(int from, int to) {
    if (from >= 0 && from < slides.size() &&
        to   >= 0 && to   < slides.size() && from != to) {
        slides.move(from, to);
        modified = true;
    }
}
