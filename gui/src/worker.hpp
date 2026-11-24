// SPDX-License-Identifier: GPL-3.0-only
// Runs every keyboard operation on its own thread so the UI never blocks on USB I/O or flash writes.
#pragma once

#include <QObject>
#include <QString>
#include <QTimer>
#include <QVariantMap>
#include <QVector>

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
        QString mapping;     // readable name of the layer-0 code
        bool remapped = false;   // differs from the factory code
        double actuation = 0, rtPress = 0, rtRelease = 0;   // mm
        int mode = 0;        // 0 normal, 1 rapid trigger, 2 continuous
        QString color;       // stored colour "#rrggbb"
    };
    QVector<Key> keys;       // by matrix index
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
    void remap(QVector<int> keys, QString code);         // code as text: "Esc", "Ctrl+C", "Mute", "none" ...
    void assignFactoryOf(QVector<int> keys, int source); // give keys the factory function of key `source`
    void resetToFactory(QVector<int> keys);              // factory code of each key (always possible)
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
    std::vector<mchose::glw::KeyCode> factory_;                // factory layer 0, by matrix index
    void setKeys(const QVector<int>& keys, const std::function<mchose::glw::KeyCode(int)>& codeFor);
    QTimer* streamTimer_ = nullptr;
    bool backedUp_ = false;
};
