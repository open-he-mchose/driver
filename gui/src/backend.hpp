// SPDX-License-Identifier: GPL-3.0-only
// The object QML talks to. Forwards requests to the Worker thread and keeps the last snapshot.
#pragma once

#include <QObject>

#include <optional>
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
    Q_PROPERTY(bool winLock READ winLock NOTIFY snapshotChanged)
    Q_PROPERTY(bool altTabLock READ altTabLock NOTIFY snapshotChanged)
    Q_PROPERTY(bool altF4Lock READ altF4Lock NOTIFY snapshotChanged)
    Q_PROPERTY(bool menuLock READ menuLock NOTIFY snapshotChanged)
    Q_PROPERTY(bool macMode READ macMode NOTIFY snapshotChanged)
    Q_PROPERTY(QStringList effectNames READ effectNames CONSTANT)
    Q_PROPERTY(KeyModel* keys READ keys CONSTANT)
    Q_PROPERTY(MacroModel* macroEditor READ macroEditor CONSTANT)
    Q_PROPERTY(QVariantList macros READ macros NOTIFY snapshotChanged)   // [{steps, bound, text}] for M1..M16
    Q_PROPERTY(QVariantList advanced READ advanced NOTIFY snapshotChanged)   // [{layer, key, label, kind, summary, problem}]
    Q_PROPERTY(QStringList advancedOrphans READ advancedOrphans NOTIFY snapshotChanged)
    Q_PROPERTY(QString version READ version CONSTANT)
    Q_PROPERTY(bool scriptRunning READ scriptRunning NOTIFY scriptRunningChanged)
    Q_PROPERTY(QVariantList scriptParams READ scriptParams NOTIFY scriptParamsChanged)
    Q_PROPERTY(QStringList scriptExamples READ scriptExamples CONSTANT)
    Q_PROPERTY(QStringList gamepadPresets READ gamepadPresets CONSTANT)
    Q_PROPERTY(QStringList gamepadTargets READ gamepadTargets CONSTANT)
    Q_PROPERTY(bool gamepadRunning READ gamepadRunning NOTIFY gamepadChanged)
    Q_PROPERTY(QString gamepadStatus READ gamepadStatus NOTIFY gamepadChanged)
    Q_PROPERTY(QVariantMap gamepadLive READ gamepadLive NOTIFY gamepadLiveChanged)
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
    bool winLock() const { return snap_.winLock; }
    bool altTabLock() const { return snap_.altTabLock; }
    bool altF4Lock() const { return snap_.altF4Lock; }
    bool menuLock() const { return snap_.menuLock; }
    bool macMode() const { return snap_.macMode; }
    QStringList effectNames() const;
    KeyModel* keys() { return &keys_; }
    MacroModel* macroEditor() { return &macroEditor_; }
    QVariantList macros() const;
    QVariantList advanced() const;
    QStringList advancedOrphans() const { return snap_.advOrphans; }
    QString version() const { return QStringLiteral(MCHOSE_VERSION); }
    bool scriptRunning() const { return scriptRunning_; }
    QVariantList scriptParams() const { return scriptParams_; }
    QStringList scriptExamples() const;
    QStringList gamepadPresets() const;
    QStringList gamepadTargets() const;
    bool gamepadRunning() const { return gamepadRunning_; }
    QString gamepadStatus() const { return gamepadStatus_; }
    QVariantMap gamepadLive() const { return gamepadLive_; }

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
    Q_INVOKABLE void clearMacro(int index);    // empties the macro and frees its keys
    Q_INVOKABLE void unbindMacro(int index);   // its keys get their default function back
    Q_INVOKABLE void bindMacro(int index, int mode);     // to the selected keys in the current layer
    // Advanced keys on the selected keys in the current layer.
    Q_INVOKABLE void createPair(int kind, int socdMode);   // kind: 0 SOCD, 1 RS, 2 LT
    Q_INVOKABLE void createModTap(const QString& tap, const QString& hold, int holdMs);
    Q_INVOKABLE void createToggle(const QString& code);
    Q_INVOKABLE void createDks(double shallowMm, double deepMm, const QVariantList& actions);
    Q_INVOKABLE void removeAdvanced(int layer, int key);
    Q_INVOKABLE void setRotation(const QVariantList& profiles);
    Q_INVOKABLE void copyProfile(int src, int dst);
    Q_INVOKABLE void setReportRate(int hz);
    Q_INVOKABLE void setSettings(bool winLock, bool altTabLock, bool altF4Lock, bool menuLock, bool macMode);
    Q_INVOKABLE void paintSelected(const QString& color);
    // Lighting scripts (Lua).
    Q_INVOKABLE QString exampleSource(const QString& name) const;
    Q_INVOKABLE QString readTextFile(const QUrl& file);
    Q_INVOKABLE bool writeTextFile(const QUrl& file, const QString& text);
    Q_INVOKABLE void runScript(const QString& source, const QString& name, const QVariantMap& values = {});
    Q_INVOKABLE void previewParams(const QString& source);   // parameters of a script before it runs
    Q_INVOKABLE void stopScript();
    Q_INVOKABLE void setScriptParam(const QString& name, double value);
    // Gamepad: the mapping is edited as text (see mchose/gamepad/gamepad.hpp); these helpers edit that text.
    Q_INVOKABLE QString gamepadPreset(const QString& name) const;
    Q_INVOKABLE QVariantMap gamepadCheck(const QString& text);   // {ok, error, count, deadzone, full, button}; updates the key labels
    Q_INVOKABLE QString gamepadBindSelected(const QString& text, const QString& target);
    Q_INVOKABLE QString gamepadUnbindSelected(const QString& text);
    Q_INVOKABLE QString gamepadSetCurve(const QString& text, const QString& name, double value);
    Q_INVOKABLE void startGamepad(const QString& text);
    Q_INVOKABLE void stopGamepad();
    Q_INVOKABLE void gamepadKeysTyping(const QString& text, bool typing);   // false: disable the mapped keys in the base layer
    // Spans as the keyboard stores them (the format normalises some combinations, like the official driver).
    Q_INVOKABLE QVariantList storedSpans(int ds, int de, int us, int ue) const;

signals:
    void connectedChanged();
    void busyChanged();
    void streamingChanged();
    void snapshotChanged();
    void message(QString text, bool error);
    void scriptRunningChanged();
    void scriptParamsChanged();
    void scriptLog(QString line, bool error);
    void gamepadChanged();
    void gamepadLiveChanged();

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
    void doUnbindMacro(int index);
    void doSetMacro(int index, QString text);
    void doBindMacro(QVector<int> keys, int layer, int index, int mode);
    void doAdvPair(QVector<int> keys, int layer, int kind, int socdMode);
    void doAdvModTap(QVector<int> keys, int layer, QString tap, QString hold, int holdMs);
    void doAdvToggle(QVector<int> keys, int layer, QString code);
    void doAdvDks(QVector<int> keys, int layer, int shallow, int deep, QVariantList actions);
    void doAdvClear(int layer, int key);
    void doRotation(QVector<int> profiles);
    void doCopyProfile(int src, int dst);
    void doReportRate(int hz);
    void doSettings(bool winLock, bool altTabLock, bool altF4Lock, bool menuLock, bool macMode);
    void doPaint(QVector<int> keys, QString color);
    void doScriptRun(QString source, QString name, QVariantMap values);
    void doScriptStop();
    void doScriptParam(QString name, double value);
    void doGamepadStart(QString config);
    void doGamepadStop();

private:
    QThread thread_;
    Worker* worker_ = nullptr;
    KeyModel keys_;
    MacroModel macroEditor_;
    Snapshot snap_;
    bool connected_ = false;
    bool busy_ = false;
    bool streaming_ = false;
    bool scriptRunning_ = false;
    bool gamepadRunning_ = false;
    QString gamepadStatus_;
    QVariantMap gamepadLive_;
    std::optional<int> keyByName(QStringView name) const;   // factory name or #N -> matrix index
    QString keyName(int index) const;
    QVariantList scriptParams_;
};
