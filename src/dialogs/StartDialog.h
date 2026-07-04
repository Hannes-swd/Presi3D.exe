#pragma once
#include <QDialog>

class QListWidget;
class QPushButton;
class UpdateChecker;

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

    void onCheckUpdateClicked();
    void onUpdateAvailable(const QString& version, const QString& downloadUrl);
    void onUpToDate();
    void onUpdateCheckFailed(const QString& error);
    void onInstallerReady(const QString& installerPath);
    void onUpdateDownloadFailed(const QString& error);

private:
    void buildUi();
    void loadRecentList();
    void refreshUpdateButton();

    Choice       m_choice       = None;
    QString      m_selectedPath;
    QListWidget* m_recentList   = nullptr;
    QPushButton* m_openRecentBtn = nullptr;

    UpdateChecker* m_updateChecker    = nullptr;
    QPushButton*   m_updateBtn        = nullptr;
    bool           m_updateAvailable  = false;
    bool           m_updateBusy       = false;
    QString        m_updateVersion;
    QString        m_updateDownloadUrl;
};
