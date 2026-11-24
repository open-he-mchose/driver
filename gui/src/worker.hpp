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
    struct Key {
        std::array<QString, 4> mapping;      // readable name of the code in each layer
        std::array<bool, 4> remapped{};      // differs from the factory code of that layer
        std::array<bool, 4> dangerous{};     // factory reset / calibration reset / bootloader code
        double actuation = 0, rtPress = 0, rtRelease = 0;   // mm
        int mode = 0;        // 0 normal, 1 rapid trigger, 2 continuous
        QString color;       // stored colour "#rrggbb"
    };
    QVector<Key> keys;       // by matrix index
    QStringList macroTexts;      // 16 macros in the exact text form (empty = no macro)
    QVector<int> macroSteps;     // number of steps
    QStringList macroBindings;   // keys bound to each macro in the active profile, e.g. "F10 (once)"
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
    void setMacro(int index, QString text);                         // exact text form (format_macro)
    void bindMacro(QVector<int> keys, int layer, int index, int mode);   // mode: 0 hold, 1-254 times, 255 toggle
    void applyLight(int effect, int brightness, int speed, QString color);
    void backup(QString path);
    void restore(QString path);
    void setStreaming(bool on);

signals:
    void connectedChanged(bool connected, QString model);
    void refreshed(Snapshot snapshot);
    void message(QString text, bool error);
    void busyChanged(bool busy);
    void depth(int key, double mm, bool pressed);
    void streamingChanged(bool on);

private:
    bool ready();
    template <class F>
    void run(const QString& what, F&& f, bool reread = true);
    void autoBackup();
    void pollStream();

    std::unique_ptr<mchose::hid::HidapiTransport> transport_;
    std::unique_ptr<mchose::glw::Keyboard> kb_;
    std::map<std::array<std::uint8_t, 3>, int> factoryIndex_;   // factory code -> matrix index (depth stream)
    std::array<std::vector<mchose::glw::KeyCode>, 4> factory_;   // factory keymap per layer, by matrix index
    void setKeys(const QVector<int>& keys, int layer, const std::function<mchose::glw::KeyCode(int)>& codeFor,
                 bool allowDangerous = false);
    QTimer* streamTimer_ = nullptr;
    bool backedUp_ = false;
};
