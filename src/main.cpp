#include <QApplication>
#include "MainWindow.h"
#include "dialogs/StartDialog.h"

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("Presi 3D");
    app.setApplicationVersion("1.0");
    app.setOrganizationName("presiEditor");

    StartDialog start;
    if (start.exec() != QDialog::Accepted)
        return 0;

    MainWindow w;
    if (start.choice() == StartDialog::OpenProject)
        w.openPresentationFromFolder(start.selectedPath());

    w.show();
    return app.exec();
}
