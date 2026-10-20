#include "mod3d_gui/MainWindow.h"
#include <QApplication>
#include <QIcon>

#include <QSurfaceFormat>

int main(int argc, char *argv[]) {
    // Modern 3D graphics defaults: 24-bit depth buffer, 8-bit stencil, 4x MSAA
    QSurfaceFormat fmt;
    fmt.setDepthBufferSize(24);
    fmt.setStencilBufferSize(8);
    fmt.setSamples(4);
    fmt.setSwapInterval(1);
    QSurfaceFormat::setDefaultFormat(fmt);

    QApplication app(argc, argv);
    app.setApplicationName("Mod3D");
    app.setApplicationVersion("2.4.0");
    app.setOrganizationName("Mod3D Geophysical Modeling");
    app.setWindowIcon(QIcon(":/icons/app_icon.png"));

    mod3d_gui::MainWindow window;
    window.show();

    if (argc > 1) {
        window.document()->openFile(QString::fromLocal8Bit(argv[1]));
    }

    return app.exec();
}
