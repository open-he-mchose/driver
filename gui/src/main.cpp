// SPDX-License-Identifier: GPL-3.0-only
// mchose-gui — configurator for MCHOSE magnetic keyboards (unofficial).
#include <QApplication>
#include <QDir>
#include <QFileInfo>
#include <QLibraryInfo>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QTimer>

#include <cstring>

#include "backend.hpp"

namespace {

// True if a Qt Quick Controls style module (e.g. "org.kde.desktop") is installed.
bool qml_module_installed(const QString& uri)
{
    QStringList dirs{QLibraryInfo::path(QLibraryInfo::QmlImportsPath)};
    for (const char* var : {"QML_IMPORT_PATH", "QML2_IMPORT_PATH"})
        for (const QString& d : qEnvironmentVariable(var).split(QDir::listSeparator(), Qt::SkipEmptyParts))
            dirs << d;
    const QString rel = QString(uri).replace('.', '/') + "/qmldir";
    for (const QString& d : dirs)
        if (QFileInfo::exists(d + '/' + rel))
            return true;
    return false;
}

}  // namespace

int main(int argc, char* argv[])
{
    // Take --style NAME out of argv before QApplication sees it: QApplication treats it as a Qt Widgets
    // style and swallows it.
    QString cliStyle;
    {
        int out = 1;
        for (int i = 1; i < argc; ++i) {
            if (std::strcmp(argv[i], "--style") == 0 && i + 1 < argc)
                cliStyle = QString::fromLocal8Bit(argv[++i]);
            else if (std::strncmp(argv[i], "--style=", 8) == 0)
                cliStyle = QString::fromLocal8Bit(argv[i] + 8);
            else
                argv[out++] = argv[i];
        }
        argc = out;
        argv[argc] = nullptr;
    }
    // QApplication (not QGuiApplication): the desktop style draws controls with the system QStyle.
    QApplication app(argc, argv);
    QApplication::setApplicationName("mchose-gui");
    QApplication::setOrganizationName("open-he-mchose");
    QApplication::setApplicationVersion(MCHOSE_VERSION);
    // Style: --style NAME, else QT_QUICK_CONTROLS_STYLE, else in a KDE Plasma session the KDE desktop style
    // if installed (kf6-qqc2-desktop-style: draws with the system QStyle and the KDE colour scheme), else the
    // platform default (Fusion with the system palette, e.g. from qt6ct, on Linux; the native style on
    // Windows). Outside Plasma the desktop style takes its colours from kdeglobals, not from qt6ct, and can
    // clash with the palette, so it is only used there on request (--style org.kde.desktop).
    QString style = !cliStyle.isEmpty() ? cliStyle : qEnvironmentVariable("QT_QUICK_CONTROLS_STYLE");
#ifdef Q_OS_LINUX
    if (style.isEmpty())
        style = qEnvironmentVariable("XDG_CURRENT_DESKTOP").contains("KDE", Qt::CaseInsensitive) &&
                        qml_module_installed("org.kde.desktop")
                    ? QStringLiteral("org.kde.desktop")
                    : QStringLiteral("Fusion");   // explicit: Qt may otherwise pick up a widget style name (e.g. kvantum)
#endif
    static const QStringList builtin{"Basic", "Fusion", "Imagine", "Material", "Universal", "FluentWinUI3", "Windows", "macOS", "iOS"};
    if (!style.isEmpty() && !builtin.contains(style, Qt::CaseInsensitive) && !qml_module_installed(style)) {
        qWarning("Qt Quick Controls style '%s' is not installed; using Fusion", qPrintable(style));
        style = QStringLiteral("Fusion");
    }
    if (!style.isEmpty())
        QQuickStyle::setStyle(style);
    // Material / Universal: follow the system light / dark scheme unless their theme variable is set.
    if (style.compare("Material", Qt::CaseInsensitive) == 0 && qEnvironmentVariableIsEmpty("QT_QUICK_CONTROLS_MATERIAL_THEME"))
        qputenv("QT_QUICK_CONTROLS_MATERIAL_THEME", "System");
    if (style.compare("Universal", Qt::CaseInsensitive) == 0 && qEnvironmentVariableIsEmpty("QT_QUICK_CONTROLS_UNIVERSAL_THEME"))
        qputenv("QT_QUICK_CONTROLS_UNIVERSAL_THEME", "System");

    Backend backend;
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("backend", &backend);
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app, [] { QCoreApplication::exit(1); },
                     Qt::QueuedConnection);
    engine.loadFromModule("MchoseGui", "Main");

    // --screenshot FILE [--tab N] [--select]: render the window, save it as an image and quit (for docs and
    // tests; works with QT_QPA_PLATFORM=offscreen).
    const QStringList args = app.arguments();
    const int shot = static_cast<int>(args.indexOf("--screenshot"));
    if (shot >= 0 && shot + 1 < args.size() && !engine.rootObjects().isEmpty()) {
        auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().first());
        const int tabArg = static_cast<int>(args.indexOf("--tab"));
        if (tabArg >= 0 && tabArg + 1 < args.size())
            window->setProperty("startTab", args[tabArg + 1].toInt());
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
