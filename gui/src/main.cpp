// SPDX-License-Identifier: GPL-3.0-only
// mchose-gui — configurator for MCHOSE magnetic keyboards (unofficial).
#include <QGuiApplication>
#include <QLocale>
#include <QTranslator>
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

    // Translation for the system language (built in when Qt LinguistTools were available); MCHOSE_LANG=en (or ru, ...)
    // overrides it.
    QTranslator translator;
    const QString lang = qEnvironmentVariable("MCHOSE_LANG");
    if (translator.load(lang.isEmpty() ? QLocale::system() : QLocale(lang), "mchose-gui", "_", ":/i18n"))
        QGuiApplication::installTranslator(&translator);

    Backend backend;
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("backend", &backend);
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app, [] { QCoreApplication::exit(1); },
                     Qt::QueuedConnection);
    engine.loadFromModule("MchoseGui", "Main");

    // --screenshot FILE [--tab N] [--layer N] [--select] [--demo-macro TEXT] [--run-example NAME.lua] [--example NAME.lua]: render the window, save it as an image and quit (for docs and
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
        const int runArg = static_cast<int>(args.indexOf("--run-example"));
        const int exArg = static_cast<int>(args.indexOf("--example"));
        if (exArg >= 0 && exArg + 1 < args.size())
            QTimer::singleShot(300, &app, [&engine, name = args[exArg + 1]] {
                if (auto* root = engine.rootObjects().value(0))
                    if (auto* panel = root->findChild<QObject*>("scriptPanel"))
                        QMetaObject::invokeMethod(panel, "loadExample", Q_ARG(QVariant, name));
            });
        if (runArg >= 0 && runArg + 1 < args.size())
            QTimer::singleShot(1500, &app, [&backend, name = args[runArg + 1]] { backend.runScript(backend.exampleSource(name), name); });
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
