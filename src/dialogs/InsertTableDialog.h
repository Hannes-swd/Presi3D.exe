#pragma once
#include <QDialog>
#include <QWidget>

class QSpinBox;
class QLabel;

class TablePickerWidget : public QWidget {
    Q_OBJECT
public:
    explicit TablePickerWidget(QWidget* parent = nullptr);
    void setSelection(int rows, int cols);

signals:
    void selectionChanged(int rows, int cols);

protected:
    void paintEvent(QPaintEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mousePressEvent(QMouseEvent*) override;

private:
    static constexpr int CELL  = 22;
    static constexpr int MAXR  = 10;
    static constexpr int MAXC  = 10;
    int m_rows = 3;
    int m_cols = 3;
};

class InsertTableDialog : public QDialog {
    Q_OBJECT
public:
    explicit InsertTableDialog(QWidget* parent = nullptr);
    int rows() const;
    int cols() const;

private:
    TablePickerWidget* m_picker  = nullptr;
    QSpinBox*          m_rowSpin = nullptr;
    QSpinBox*          m_colSpin = nullptr;
    QLabel*            m_label   = nullptr;
    bool               m_upd     = false;
};
