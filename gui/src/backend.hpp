// SPDX-License-Identifier: GPL-3.0-only
// The object QML talks to. Forwards requests to the Worker thread and keeps the last snapshot.
#pragma once

#include <QObject>
#include <QStringList>
#include <QThread>
#include <QUrl>
#include <QVariantList>

#include "keymodel.hpp"
#include "macromodel.hpp"
#include "worker.hpp"

class Backend : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool connected READ connected NOTIFY connectedChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(bool streaming READ streaming NOTIFY streamingChanged)
    Q_PROPERTY(QString modelName READ modelName NOTIFY snapshotChanged)
    Q_PROPERTY(QString firmware READ firmware NOTIFY snapshotChanged)
    Q_PROPERTY(int activeProfile READ activeProfile NOTIFY snapshotChanged)
    Q_PROPERTY(QVariantList rotation READ rotation NOTIFY snapshotChanged)
    Q_PROPERTY(int reportRateHz READ reportRateHz NOTIFY snapshotChanged)
    Q_PROPERTY(int lightEffect READ lightEffect NOTIFY snapshotChanged)
    Q_PROPERTY(int lightBrightness READ lightBrightness NOTIFY snapshotChanged)
    Q_PROPERTY(int lightSpeed READ lightSpeed NOTIFY snapshotChanged)
    Q_PROPERTY(QString lightColor READ lightColor NOTIFY snapshotChanged)
    Q_PROPERTY(QStringList effectNames READ effectNames CONSTANT)
    Q_PROPERTY(KeyModel* keys READ keys CONSTANT)
    Q_PROPERTY(MacroModel* macroEditor READ macroEditor CONSTANT)
    Q_PROPERTY(QVariantList macros READ macros NOTIFY snapshotChanged)   // [{steps, bound, text}] for M1..M16
    Q_PROPERTY(QVariantList advanced READ advanced NOTIFY snapshotChanged)   // [{layer, key, label, kind, summary, problem}]
    Q_PROPERTY(QStringList advancedOrphans READ advancedOrphans NOTIFY snapshotChanged)
    Q_PROPERTY(QString version READ version CONSTANT)
public:
    explicit Backend(QObject* parent = nullptr);
    ~Backend() override;

    bool connected() const { return connected_; }
    bool busy() const { return busy_; }
    bool streaming() const { return streaming_; }
    QString modelName() const { return snap_.model; }
    QString firmware() const { return snap_.firmware; }
    int activeProfile() const { return snap_.activeProfile; }
    QVariantList rotation() const;
    int reportRateHz() const { return snap_.reportRateHz; }
    int lightEffect() const { return snap_.lightEffect; }
    int lightBrightness() const { return snap_.lightBrightness; }
    int lightSpeed() const { return snap_.lightSpeed; }
    QString lightColor() const { return snap_.lightColor; }
    QStringList effectNames() const;
    KeyModel* keys() { return &keys_; }
    MacroModel* macroEditor() { return &macroEditor_; }
    QVariantList macros() const;
    QVariantList advanced() const;
    QStringList advancedOrphans() const { return snap_.advOrphans; }
    QString version() const { return QStringLiteral(MCHOSE_VERSION); }

    Q_INVOKABLE void connectDevice();
    Q_INVOKABLE void refresh();
    Q_INVOKABLE void switchProfile(int profile);
    Q_INVOKABLE void applyTrigger(double actuation, int mode, double press, double release);
    Q_INVOKABLE void remapSelected(const QString& code);
    Q_INVOKABLE void assignFactoryOf(int sourceMatrixIndex);   // picker: the function of that key
    Q_INVOKABLE void resetSelected();                          // factory mapping
    Q_INVOKABLE void applyLight(int effect, int brightness, int speed, const QString& color);
    Q_INVOKABLE void backup(const QUrl& file);
    Q_INVOKABLE void restore(const QUrl& file);
    Q_INVOKABLE void setStreaming(bool on);
    Q_INVOKABLE void editMacro(int index);               // load M(index+1) into macroEditor
    Q_INVOKABLE void saveMacro(int index);               // write the editor's steps
    Q_INVOKABLE void clearMacro(int index);
    Q_INVOKABLE void bindMacro(int index, int mode);     // to the selected keys in the current layer
    // Advanced keys on the selected keys in the current layer.
    Q_INVOKABLE void createPair(int kind, int socdMode);   // kind: 0 SOCD, 1 RS, 2 LT
    Q_INVOKABLE void createModTap(const QString& tap, const QString& hold, int holdMs);
    Q_INVOKABLE void createToggle(const QString& code);
    Q_INVOKABLE void createDks(double shallowMm, double deepMm, const QVariantList& actions);
    Q_INVOKABLE void removeAdvanced(int layer, int key);
    // Spans as the keyboard stores them (the format normalises some combinations, like the official driver).
    Q_INVOKABLE QVariantList storedSpans(int ds, int de, int us, int ue) const;

signals:
    void connectedChanged();
    void busyChanged();
    void streamingChanged();
    void snapshotChanged();
    void message(QString text, bool error);

    // to the worker thread
    void doConnect();
    void doRefresh();
    void doSwitch(int profile);
    void doTrigger(QVector<int> keys, double actuation, int mode, double press, double release);
    void doRemap(QVector<int> keys, int layer, QString code);
    void doAssignFactory(QVector<int> keys, int layer, int source);
    void doReset(QVector<int> keys, int layer);
    void doLight(int effect, int brightness, int speed, QString color);
    void doBackup(QString path);
    void doRestore(QString path);
    void doStreaming(bool on);
    void doSetMacro(int index, QString text);
    void doBindMacro(QVector<int> keys, int layer, int index, int mode);
    void doAdvPair(QVector<int> keys, int layer, int kind, int socdMode);
    void doAdvModTap(QVector<int> keys, int layer, QString tap, QString hold, int holdMs);
    void doAdvToggle(QVector<int> keys, int layer, QString code);
    void doAdvDks(QVector<int> keys, int layer, int shallow, int deep, QVariantList actions);
    void doAdvClear(int layer, int key);

private:
    QThread thread_;
    Worker* worker_ = nullptr;
    KeyModel keys_;
    MacroModel macroEditor_;
    Snapshot snap_;
    bool connected_ = false;
    bool busy_ = false;
    bool streaming_ = false;
};
