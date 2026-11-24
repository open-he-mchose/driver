// SPDX-License-Identifier: GPL-3.0-only
// Runs every keyboard operation on its own thread so the UI never blocks on USB I/O or flash writes.
#pragma once

#include <QObject>
#include <QStringList>
#include <QString>
#include <QTimer>
#include <QVariantMap>
#include <QVector>

#include <array>
#include <functional>
#include <map>
#include <memory>
#include <vector>

#include "mchose/glw/keyboard.hpp"
#include "mchose/hid/hidapi_transport.hpp"
#include "mchose/gamepad/gamepad.hpp"
#include "mchose/script/script_player.hpp"

// Everything the UI shows, read from the keyboard in one go.
struct Snapshot {
    QString model;
    QString firmware;
    int activeProfile = 0;
    QVector<int> rotation;
    int reportRateHz = 0;
    int lightEffect = 0;
    int lightBrightness = 0;
    int lightSpeed = 0;
    QString lightColor;      // "#rrggbb"
    bool winLock = false, altTabLock = false, altF4Lock = false, menuLock = false, macMode = false;
    struct Key {
        std::array<QString, 4> mapping;      // readable name of the code in each layer
        std::array<bool, 4> remapped{};      // differs from the factory code of that layer
        std::array<bool, 4> dangerous{};     // factory reset / calibration reset / bootloader code
        double actuation = 0, rtPress = 0, rtRelease = 0;   // mm
        int mode = 0;        // 0 normal, 1 rapid trigger, 2 continuous
        QString color;       // stored colour "#rrggbb"
    };
    QVector<Key> keys;       // by matrix index
    QStringList factoryNames;   // factory layer-0 key name by matrix index ("LShift", "Up")
    QStringList macroTexts;      // 16 macros in the exact text form (empty = no macro)
    QVector<int> macroSteps;     // number of steps
    QStringList macroBindings;   // keys bound to each macro in the active profile, e.g. "F10 (once)"
    struct Adv {
        int layer = 0, key = 0;
        QString label, kind, summary, problem;   // problem: empty if the binding is fine
    };
    QVector<Adv> adv;            // advanced bindings of the active profile
    QStringList advOrphans;      // filled table entries nobody uses (harmless)
};
Q_DECLARE_METATYPE(Snapshot)

class Worker : public QObject {
    Q_OBJECT
public:
    Worker();
    ~Worker() override;

public slots:
    void connectDevice();
    void refresh();
    void switchProfile(int profile);
    void applyTrigger(QVector<int> keys, double actuation, int mode, double press, double release);
    void remap(QVector<int> keys, int layer, QString code);         // code as text: "Esc", "Ctrl+C", "Mute", "none" ...
    void assignFactoryOf(QVector<int> keys, int layer, int source); // the factory (base layer) function of key `source`
    void resetToFactory(QVector<int> keys, int layer);              // factory code of each key in that layer
    void unbindMacro(int index);   // keys bound to the macro (all layers, active profile) -> factory code
    void setMacro(int index, QString text);                         // exact text form (format_macro)
    void bindMacro(QVector<int> keys, int layer, int index, int mode);   // mode: 0 hold, 1-254 times, 255 toggle
    void advPair(QVector<int> keys, int layer, int kind, int socdMode);   // kind: 0 SOCD, 1 RS, 2 LT
    void advModTap(QVector<int> keys, int layer, QString tap, QString hold, int holdMs);
    void advToggle(QVector<int> keys, int layer, QString code);            // empty code: the key's own function
    void advDks(QVector<int> keys, int layer, int shallow, int deep, QVariantList actions);   // [{key, ds, de, us, ue}]
    void advClear(int layer, int key);
    void setRotation(QVector<int> profiles);
    void copyProfile(int src, int dst);
    void setReportRate(int hz);                       // the keyboard reboots; reconnects afterwards
    void setSettings(bool winLock, bool altTabLock, bool altF4Lock, bool menuLock, bool macMode);
    void paintColors(QVector<int> keys, QString color);
    void scriptRun(QString source, QString name, QVariantMap values);   // load, apply values, start (25 fps)
    void scriptStop();
    void scriptSetParam(QString name, double value);
    void applyLight(int effect, int brightness, int speed, QString color);
    void backup(QString path);
    void restore(QString path);
    void setStreaming(bool on);
    void gamepadStart(QString config);   // mapping text (mchose::gamepad::parse_config); turns the depth stream on
    void gamepadStop();

signals:
    void connectedChanged(bool connected, QString model);
    void refreshed(Snapshot snapshot);
    void message(QString text, bool error);
    void busyChanged(bool busy);
    void depth(int key, double mm, bool pressed);
    void streamingChanged(bool on);
    void scriptRunningChanged(bool running);
    void scriptParams(QVariantList params);      // [{name, label, min, max, value}]
    void scriptLog(QString line, bool error);
    void scriptColors(QByteArray rgb);           // 3 bytes per matrix index, after every frame
    void gamepadRunningChanged(bool running, QString status);
    void gamepadState(QVariantMap state);        // lx ly rx ry (-1..1, +y up), lt rt (0..1), buttons [names]

private:
    bool ready();
    template <class F>
    void run(const QString& what, F&& f, bool reread = true);
    void autoBackup();
    void pollStream();
    void scriptTick();

    std::unique_ptr<mchose::hid::HidapiTransport> transport_;
    std::unique_ptr<mchose::glw::Keyboard> kb_;
    std::map<std::array<std::uint8_t, 3>, int> factoryIndex_;   // factory code -> matrix index (depth stream)
    std::array<std::vector<mchose::glw::KeyCode>, 4> factory_;   // factory keymap per layer, by matrix index
    void setKeys(const QVector<int>& keys, int layer, const std::function<mchose::glw::KeyCode(int)>& codeFor,
                 bool allowDangerous = false);
    QTimer* streamTimer_ = nullptr;
    QTimer* scriptTimer_ = nullptr;
    std::unique_ptr<mchose::script::ScriptPlayer> player_;
    std::unique_ptr<mchose::gamepad::VirtualPad> pad_;
    std::unique_ptr<mchose::gamepad::Mapper> mapper_;
    void stopGamepadQuietly();
    bool backedUp_ = false;
};
