#pragma once
#include <QDialog>
#include "ShapeBoolean.h"

// Pathfinder-style picker: shown when 2+ Shape/Text elements are selected
// and the user chooses "Formen verschneiden..." — offers the 4 standard
// boolean ops (Union/Subtract/Intersect/Exclude), each with a small icon
// preview. Picking one accepts immediately (no separate OK button).
class BooleanCutDialog : public QDialog {
    Q_OBJECT
public:
    explicit BooleanCutDialog(QWidget* parent = nullptr);

    ShapeBoolean::Op chosenOp() const { return m_chosenOp; }

private:
    ShapeBoolean::Op m_chosenOp = ShapeBoolean::Op::Subtract;
};
