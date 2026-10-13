#include "mod3d_gui/MainWindow.h"
#include <QApplication>

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("Mod3D");
    app.setApplicationVersion("2.1.0");
    app.setOrganizationName("Mod3D Geophysical Modeling");

    mod3d_gui::MainWindow window;
    window.show();

    if (argc > 1) {
        window.document()->openFile(QString::fromLocal8Bit(argv[1]));
    }

    return app.exec();
}
