#include "mod3d_gui/MainWindow.h"
#include <QApplication>
#include <QIcon>

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("Mod3D");
    app.setApplicationVersion("2.1.4");
    app.setOrganizationName("Mod3D Geophysical Modeling");
    app.setWindowIcon(QIcon(":/icons/app_icon.png"));

    mod3d_gui::MainWindow window;
    window.show();

    if (argc > 1) {
        window.document()->openFile(QString::fromLocal8Bit(argv[1]));
    }

    return app.exec();
}
