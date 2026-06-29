#pragma once
#include <QWidget>
#include "models/DataModel.h"

class QListWidget;
class QListWidgetItem;
class QPushButton;

class SlideListPanel : public QWidget {
    Q_OBJECT
public:
    explicit SlideListPanel(QWidget* parent = nullptr);

    void setPresentation(Presentation* pres);
    void setSelectedSlide(int index);

signals:
    void slideSelected(int index);
    void slideAdded();
    void slideRemoved(int index);
    void slideDuplicated(int index);
    void slideRenamed(int index, const QString& name);
    void slideMoved(int from, int to);

private slots:
    void onCurrentRowChanged(int row);
    void onContextMenu(const QPoint& pos);
    void onItemDoubleClicked(QListWidgetItem* item);

private:
    void populate();
    QPixmap makeThumbnail(const Slide& slide) const;

    Presentation* m_pres      = nullptr;
    QListWidget*  m_list      = nullptr;
    QPushButton*  m_addBtn    = nullptr;
    QPushButton*  m_removeBtn = nullptr;
    bool          m_updating  = false;
};
