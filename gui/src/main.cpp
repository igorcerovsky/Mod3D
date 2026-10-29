#include "mod3d_gui/MainWindow.h"
#include <QApplication>
#include <QIcon>
#include <QSurfaceFormat>
#include <QTimer>

int main(int argc, char *argv[]) {
    // Modern 3D graphics defaults: 24-bit depth buffer, 8-bit stencil, 4x MSAA, OpenGL 4.1 Core Profile
    QSurfaceFormat fmt;
    fmt.setVersion(4, 1);
    fmt.setProfile(QSurfaceFormat::CoreProfile);
    fmt.setDepthBufferSize(24);
    fmt.setStencilBufferSize(8);
    fmt.setSamples(4);
    fmt.setSwapInterval(1);
    QSurfaceFormat::setDefaultFormat(fmt);

    QApplication app(argc, argv);
    app.setApplicationName("Mod3D");
    app.setApplicationVersion("2.5.1");
    app.setOrganizationName("Mod3D Geophysical Modeling");
    app.setWindowIcon(QIcon(":/icons/app_icon.png"));

    mod3d_gui::MainWindow window;
    window.show();

    QString modelArg;
    const QStringList args = app.arguments();
    for (int i = 1; i < args.size(); ++i) {
        if (!args[i].startsWith('-')) {
            modelArg = args[i];
            break;
        }
    }

    if (!modelArg.isEmpty()) {
        window.document()->openFile(modelArg);
    } else {
        QTimer::singleShot(0, &window, &mod3d_gui::MainWindow::onFileNew);
    }

    return app.exec();
}
