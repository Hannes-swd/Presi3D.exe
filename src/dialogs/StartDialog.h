#pragma once
#include <QDialog>

class QListWidget;
class QPushButton;

class StartDialog : public QDialog {
    Q_OBJECT
public:
    enum Choice { None, NewProject, OpenProject };

    explicit StartDialog(QWidget* parent = nullptr);

    Choice  choice()       const { return m_choice; }
    QString selectedPath() const { return m_selectedPath; }

    static void   addRecentProject(const QString& path);
    static QStringList recentProjects();

private slots:
    void onNewProject();
    void onOpenProject();
    void onRecentDoubleClicked();
    void onRecentClicked();

private:
    void buildUi();
    void loadRecentList();

    Choice       m_choice       = None;
    QString      m_selectedPath;
    QListWidget* m_recentList   = nullptr;
    QPushButton* m_openRecentBtn = nullptr;
};
