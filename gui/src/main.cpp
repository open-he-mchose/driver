// SPDX-License-Identifier: GPL-3.0-only
// mchose-gui — configurator for MCHOSE magnetic keyboards (unofficial).
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QTimer>

#include "backend.hpp"

int main(int argc, char* argv[])
{
    // The system look: no style is chosen here and no colours are defined by the app. Qt Quick Controls use
    // the platform's default style and the system palette (QT_QUICK_CONTROLS_STYLE still works as usual).
    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName("mchose-gui");
    QGuiApplication::setOrganizationName("open-he-mchose");
    QGuiApplication::setApplicationVersion(MCHOSE_VERSION);

    Backend backend;
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("backend", &backend);
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app, [] { QCoreApplication::exit(1); },
                     Qt::QueuedConnection);
    engine.loadFromModule("MchoseGui", "Main");

    // --screenshot FILE [--tab N] [--layer N] [--select] [--demo-macro TEXT]: render the window, save it as an image and quit (for docs and
    // tests; works with QT_QPA_PLATFORM=offscreen).
    const QStringList args = app.arguments();
    const int shot = static_cast<int>(args.indexOf("--screenshot"));
    if (shot >= 0 && shot + 1 < args.size() && !engine.rootObjects().isEmpty()) {
        auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().first());
        const int tabArg = static_cast<int>(args.indexOf("--tab"));
        if (tabArg >= 0 && tabArg + 1 < args.size())
            window->setProperty("startTab", args[tabArg + 1].toInt());
        const int layerArg = static_cast<int>(args.indexOf("--layer"));
        if (layerArg >= 0 && layerArg + 1 < args.size())
            backend.keys()->setLayer(args[layerArg + 1].toInt());
        const int demoArg = static_cast<int>(args.indexOf("--demo-macro"));
        if (demoArg >= 0 && demoArg + 1 < args.size())
            QTimer::singleShot(2500, &app, [&backend, text = args[demoArg + 1]] { backend.macroEditor()->loadText(text); });
        if (args.contains("--select"))
            QTimer::singleShot(2000, &app, [&backend] { backend.keys()->selectRect(1.4, 2.3, 4.6, 4.2, false); });
        const QString file = args[shot + 1];
        QTimer::singleShot(3500, &app, [window, file] {
            window->grabWindow().save(file);
            QCoreApplication::quit();
        });
    }
    return app.exec();
}
