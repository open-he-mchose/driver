// SPDX-License-Identifier: GPL-3.0-only
// mchose-gui — configurator for MCHOSE magnetic keyboards (unofficial).
#include <QGuiApplication>
#include <QIcon>
#include <QLocalServer>
#include <QLocalSocket>
#include <QLocale>
#include <QPainter>
#include <QPainterPath>
#include <QTranslator>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickImageProvider>
#include <QQuickWindow>
#include <QTimer>

#include "backend.hpp"

#if defined(Q_OS_LINUX)
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#endif

#if defined(Q_OS_WIN)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

namespace {

// The application icon (same drawing as packaging/linux/mchose-gui.svg), painted so that no SVG plugin is needed.
QImage appIcon(int size)
{
    QImage img(size, size, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::transparent);
    QPainter p(&img);
    p.setRenderHint(QPainter::Antialiasing);
    p.scale(size / 256.0, size / 256.0);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0x2b, 0x2f, 0x36));
    p.drawRoundedRect(QRectF(16, 16, 224, 224), 40, 40);
    p.setBrush(QColor(0x4a, 0x50, 0x5a));
    p.drawRoundedRect(QRectF(40, 36, 176, 164), 28, 28);
    p.setBrush(QColor(0x6b, 0x72, 0x7e));
    p.drawRoundedRect(QRectF(56, 48, 144, 120), 20, 20);
    const QColor accent(0xf4, 0xc5, 0x42);
    p.setBrush(accent);
    p.drawRoundedRect(QRectF(64, 208, 128, 10), 5, 5);
    QPainterPath arrow;
    arrow.moveTo(128, 72);
    arrow.lineTo(128, 144);
    arrow.moveTo(104, 120);
    arrow.lineTo(128, 144);
    arrow.lineTo(152, 120);
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(accent, 14, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.drawPath(arrow);
    return img;
}

// image://app/icon for QML (the tray icon).
class IconProvider : public QQuickImageProvider {
public:
    IconProvider() : QQuickImageProvider(QQuickImageProvider::Image) {}
    QImage requestImage(const QString&, QSize* size, const QSize& requested) override
    {
        const int s = requested.width() > 0 ? requested.width() : 64;
        if (size)
            *size = QSize(s, s);
        return appIcon(s);
    }
};

// One running program per user: a second start asks the first one to show its window and exits.
QString instanceName()
{
#if defined(Q_OS_WIN)
    return "open-he-mchose-gui-" + qEnvironmentVariable("USERNAME");
#else
    const QString runtime = qEnvironmentVariable("XDG_RUNTIME_DIR");
    return runtime.isEmpty() ? "open-he-mchose-gui-" + qEnvironmentVariable("USER") : runtime + "/open-he-mchose-gui.socket";
#endif
}

// Whether Qt can show a tray icon without Qt Widgets: Windows always; Linux when a StatusNotifierItem host runs
// (KDE, XFCE, GNOME with the AppIndicator extension...; not plain X11 trays like awesome's).
bool trayAvailable()
{
#if defined(Q_OS_WIN)
    return true;
#elif defined(Q_OS_LINUX)
    auto* bus = QDBusConnection::sessionBus().interface();
    return bus && bus->isServiceRegistered("org.kde.StatusNotifierWatcher").value();
#else
    return false;
#endif
}

// Sends one command ("show", "ping" or "quit") to the running program; false when none runs.
bool signalRunningInstance(const QString& name, const QByteArray& command)
{
    QLocalSocket socket;
    socket.connectToServer(name);
    if (!socket.waitForConnected(500))
        return false;
#if defined(Q_OS_WIN)
    AllowSetForegroundWindow(ASFW_ANY);   // lets the running program bring its window to the front
#endif
    socket.write(command + "\n");
    socket.waitForBytesWritten(500);
    socket.disconnectFromServer();
    return true;
}

void showWindow(QQuickWindow* w)
{
    if (!w)
        return;
    if (w->visibility() == QWindow::Minimized)
        w->showNormal();
    else
        w->show();
    w->raise();
    w->requestActivate();
}

}  // namespace

int main(int argc, char* argv[])
{
    // The system look: no style is chosen here and no colours are defined by the app. Qt Quick Controls use
    // the platform's default style and the system palette (QT_QUICK_CONTROLS_STYLE still works as usual).
    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName("mchose-gui");
    QGuiApplication::setOrganizationName("open-he-mchose");
    QGuiApplication::setApplicationVersion(MCHOSE_VERSION);
    QIcon icon;
    for (int size : {16, 24, 32, 48, 64, 128, 256})
        icon.addPixmap(QPixmap::fromImage(appIcon(size)));
    QGuiApplication::setWindowIcon(icon);
    // Closing the window may keep the program running (Device tab: "Keep running in the background"); Main.qml
    // quits explicitly otherwise.
    QGuiApplication::setQuitOnLastWindowClosed(false);

    // --hidden: start without a window (start at login). --quit: ask the running program to quit.
    // --screenshot: a separate, stateless run (docs, CI).
    const QStringList args = app.arguments();
    const bool screenshotMode = args.contains("--screenshot");
    const bool hidden = args.contains("--hidden") && !screenshotMode;
    QLocalServer instanceServer;
    if (args.contains("--quit"))
        return signalRunningInstance(instanceName(), "quit") ? 0 : 1;
    if (!screenshotMode) {
        const QString name = instanceName();
        if (signalRunningInstance(name, hidden ? "ping" : "show"))
            return 0;
        QLocalServer::removeServer(name);   // a stale socket of a crashed run
        instanceServer.setSocketOptions(QLocalServer::UserAccessOption);
        if (!instanceServer.listen(name))
            qWarning("mchose-gui: cannot listen on %s: %s", qPrintable(name), qPrintable(instanceServer.errorString()));
    }

    // Translation for the system language (built in when Qt LinguistTools were available); MCHOSE_LANG=en (or ru, ...)
    // overrides it.
    QTranslator translator;
    const QString lang = qEnvironmentVariable("MCHOSE_LANG");
    if (translator.load(lang.isEmpty() ? QLocale::system() : QLocale(lang), "mchose-gui", "_", ":/i18n"))
        QGuiApplication::installTranslator(&translator);

    Backend backend;
    backend.setPersistent(!screenshotMode);
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("backend", &backend);
    engine.rootContext()->setContextProperty("trayAvailable", !screenshotMode && trayAvailable());
    engine.addImageProvider("app", new IconProvider);
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app, [] { QCoreApplication::exit(1); },
                     Qt::QueuedConnection);
    engine.loadFromModule("MchoseGui", "Main");
    auto rootWindow = [&engine] { return qobject_cast<QQuickWindow*>(engine.rootObjects().value(0)); };
    if (!hidden)
        showWindow(rootWindow());

    QObject::connect(&instanceServer, &QLocalServer::newConnection, &app, [&] {
        while (QLocalSocket* c = instanceServer.nextPendingConnection()) {
            auto handle = [c, rootWindow] {
                while (c->canReadLine()) {
                    const QByteArray line = c->readLine().trimmed();
                    if (line == "show")
                        showWindow(rootWindow());
                    else if (line == "quit")
                        QCoreApplication::quit();
                }
            };
            QObject::connect(c, &QLocalSocket::readyRead, c, handle);
            QObject::connect(c, &QLocalSocket::disconnected, c, &QObject::deleteLater);
            handle();   // the line may have arrived already
        }
    });

    // --screenshot FILE [--tab N] [--layer N] [--select] [--demo-macro TEXT] [--run-example NAME.lua] [--example NAME.lua]: render the window, save it as an image and quit (for docs and
    // tests; works with QT_QPA_PLATFORM=offscreen).
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
