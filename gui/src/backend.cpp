// SPDX-License-Identifier: GPL-3.0-only
#include "backend.hpp"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QSettings>
#include <QStandardPaths>

#include "mchose/glw/advanced.hpp"
#include "mchose/script/lighting_script.hpp"
#include "mchose/model.hpp"

Backend::Backend(QObject* parent) : QObject(parent)
{
    qRegisterMetaType<Snapshot>();
    qRegisterMetaType<QVector<int>>();
    if (auto m = mchose::find_model(mchose::kMchoseVid, 0x211A))
        keys_.setLayout(m->layout);

    worker_ = new Worker;
    worker_->moveToThread(&thread_);
    connect(&thread_, &QThread::finished, worker_, &QObject::deleteLater);

    connect(this, &Backend::doConnect, worker_, &Worker::connectDevice);
    connect(this, &Backend::doRefresh, worker_, &Worker::refresh);
    connect(this, &Backend::doSwitch, worker_, &Worker::switchProfile);
    connect(this, &Backend::doTrigger, worker_, &Worker::applyTrigger);
    connect(this, &Backend::doRemap, worker_, &Worker::remap);
    connect(this, &Backend::doAssignFactory, worker_, &Worker::assignFactoryOf);
    connect(this, &Backend::doReset, worker_, &Worker::resetToFactory);
    connect(this, &Backend::doUnbindMacro, worker_, &Worker::unbindMacro);
    connect(this, &Backend::doLight, worker_, &Worker::applyLight);
    connect(this, &Backend::doBackup, worker_, &Worker::backup);
    connect(this, &Backend::doRestore, worker_, &Worker::restore);
    connect(this, &Backend::doStreaming, worker_, &Worker::setStreaming);
    connect(this, &Backend::doSetMacro, worker_, &Worker::setMacro);
    connect(this, &Backend::doBindMacro, worker_, &Worker::bindMacro);
    connect(this, &Backend::doAdvPair, worker_, &Worker::advPair);
    connect(this, &Backend::doAdvModTap, worker_, &Worker::advModTap);
    connect(this, &Backend::doAdvToggle, worker_, &Worker::advToggle);
    connect(this, &Backend::doAdvDks, worker_, &Worker::advDks);
    connect(this, &Backend::doAdvClear, worker_, &Worker::advClear);
    connect(this, &Backend::doRotation, worker_, &Worker::setRotation);
    connect(this, &Backend::doCopyProfile, worker_, &Worker::copyProfile);
    connect(this, &Backend::doReportRate, worker_, &Worker::setReportRate);
    connect(this, &Backend::doSettings, worker_, &Worker::setSettings);
    connect(this, &Backend::doPaint, worker_, &Worker::paintColors);
    connect(this, &Backend::doScriptRun, worker_, &Worker::scriptRun);
    connect(this, &Backend::doScriptStop, worker_, &Worker::scriptStop);
    connect(this, &Backend::doScriptParam, worker_, &Worker::scriptSetParam);
    connect(worker_, &Worker::scriptRunningChanged, this, [this](bool on) {
        scriptRunning_ = on;
        if (!on)
            keys_.setLiveColors({});
        emit scriptRunningChanged();
    });
    connect(worker_, &Worker::scriptParams, this, [this](const QVariantList& p) {
        scriptParams_ = p;
        emit scriptParamsChanged();
    });
    connect(worker_, &Worker::scriptLog, this, &Backend::scriptLog);
    connect(this, &Backend::doGamepadStart, worker_, &Worker::gamepadStart);
    connect(this, &Backend::doGamepadStop, worker_, &Worker::gamepadStop);
    connect(worker_, &Worker::gamepadRunningChanged, this, [this](bool on, const QString& status) {
        gamepadRunning_ = on;
        gamepadStatus_ = status;
        emit gamepadChanged();
    });
    connect(worker_, &Worker::gamepadState, this, [this](const QVariantMap& st) {
        gamepadLive_ = st;
        emit gamepadLiveChanged();
    });
    connect(worker_, &Worker::scriptColors, this, [this](const QByteArray& rgb) { keys_.setLiveColors(rgb); });
    connect(&macroEditor_, &MacroModel::message, this, &Backend::message);
    connect(this, &Backend::doCalibStart, worker_, &Worker::calibStart);
    connect(this, &Backend::doCalibEnd, worker_, &Worker::calibEnd);
    connect(this, &Backend::doCalibReset, worker_, &Worker::calibReset);
    connect(worker_, &Worker::calibSample, this, [this](int key, int status, bool pressed) { keys_.setCalibration(key, status, pressed); });
    connect(worker_, &Worker::calibrationChanged, this, [this](bool on) {
        if (on)
            keys_.resetCalibration();
        calibrating_ = on;
        emit calibratingChanged();
        if (!on && connected_)
            restoreEffect();   // the Lua effect or gamepad that was stopped for the session
    });

    connect(worker_, &Worker::connectedChanged, this, [this](bool on, const QString&) {
        connected_ = on;
        emit connectedChanged();
        if (on)
            restoreEffect();
    });
    connect(worker_, &Worker::refreshed, this, [this](const Snapshot& s) {
        snap_ = s;
        keys_.setSnapshot(s);
        emit snapshotChanged();
    });
    connect(worker_, &Worker::busyChanged, this, [this](bool b) {
        busy_ = b;
        emit busyChanged();
    });
    connect(worker_, &Worker::streamingChanged, this, [this](bool on) {
        streaming_ = on;
        if (!on)
            keys_.clearDepth();
        emit streamingChanged();
    });
    connect(worker_, &Worker::depth, this, [this](int key, double mm, bool pressed) { keys_.setDepth(key, mm, pressed); });
    connect(worker_, &Worker::message, this, &Backend::message);

    thread_.start();
    emit doConnect();
}

Backend::~Backend()
{
    if (calibrating_)
        QMetaObject::invokeMethod(worker_, "calibEnd", Qt::BlockingQueuedConnection);
    if (gamepadRunning_)
        QMetaObject::invokeMethod(worker_, "gamepadStop", Qt::BlockingQueuedConnection);
    if (scriptRunning_)
        QMetaObject::invokeMethod(worker_, "scriptStop", Qt::BlockingQueuedConnection);
    if (streaming_)
        QMetaObject::invokeMethod(worker_, "setStreaming", Qt::BlockingQueuedConnection, Q_ARG(bool, false));
    thread_.quit();
    thread_.wait();
}

QVariantList Backend::rotation() const
{
    QVariantList l;
    for (int p : snap_.rotation)
        l.push_back(p);
    return l;
}

QStringList Backend::effectNames() const
{
    return {"static (per-key colours)", "aurora mapping", "shadow steps", "tranquility", "pulsation", "flower dance",
            "tidal rhythm", "surging tide", "colorful fountain", "galaxy roaming", "spiral", "surge rhythm", "ocean",
            "ripple dance", "ripple", "instant flash", "matrix shadows", "melody piano", "flow dance", "raining", "stars",
            "fireworks", "ripple flow"};
}

void Backend::connectDevice() { emit doConnect(); }
void Backend::refresh() { emit doRefresh(); }
void Backend::switchProfile(int profile) { emit doSwitch(profile); }

void Backend::applyTrigger(double actuation, int mode, double press, double release)
{
    const auto sel = keys_.selectedIndices();
    if (sel.isEmpty()) {
        emit message(tr("Select keys first"), true);
        return;
    }
    emit doTrigger(sel, actuation, mode, press, release);
}

void Backend::remapSelected(const QString& code)
{
    const auto sel = keys_.selectedIndices();
    if (sel.isEmpty()) {
        emit message(tr("Select the keys to change first (on the keyboard at the top)"), true);
        return;
    }
    emit doRemap(sel, keys_.layer(), code);
}

void Backend::assignFactoryOf(int sourceMatrixIndex)
{
    const auto sel = keys_.selectedIndices();
    if (sel.isEmpty()) {
        emit message(tr("Select the keys to change first (on the keyboard at the top)"), true);
        return;
    }
    emit doAssignFactory(sel, keys_.layer(), sourceMatrixIndex);
}

void Backend::resetSelected()
{
    const auto sel = keys_.selectedIndices();
    if (sel.isEmpty()) {
        emit message(tr("Select the keys to reset first"), true);
        return;
    }
    emit doReset(sel, keys_.layer());
}

void Backend::applyLight(int effect, int brightness, int speed, const QString& color)
{
    emit doLight(effect, brightness, speed, color);
}

void Backend::backup(const QUrl& file) { emit doBackup(file.toLocalFile()); }
void Backend::restore(const QUrl& file) { emit doRestore(file.toLocalFile()); }
void Backend::setStreaming(bool on) { emit doStreaming(on); }

QVariantList Backend::macros() const
{
    QVariantList l;
    for (int i = 0; i < snap_.macroTexts.size(); ++i)
        l.push_back(QVariantMap{{"steps", snap_.macroSteps.value(i)}, {"bound", snap_.macroBindings.value(i)},
                                {"text", snap_.macroTexts.value(i)}});
    return l;
}

void Backend::editMacro(int index)
{
    if (index < 0 || index >= snap_.macroTexts.size())
        return;
    macroEditor_.stopRecording();
    if (snap_.macroTexts[index].isEmpty())
        macroEditor_.load({});
    else if (macroEditor_.loadText(snap_.macroTexts[index]))
        macroEditor_.setModified(false);
}

void Backend::saveMacro(int index)
{
    macroEditor_.stopRecording();
    emit doSetMacro(index, macroEditor_.text());
    macroEditor_.setModified(false);
}

void Backend::clearMacro(int index)
{
    macroEditor_.load({});
    emit doSetMacro(index, QString());
    if (!snap_.macroBindings.value(index).isEmpty())
        emit doUnbindMacro(index);
}

void Backend::unbindMacro(int index) { emit doUnbindMacro(index); }

void Backend::bindMacro(int index, int mode)
{
    const auto sel = keys_.selectedIndices();
    if (sel.isEmpty()) {
        emit message(tr("Select the key(s) for the macro on the keyboard at the top"), true);
        return;
    }
    emit doBindMacro(sel, keys_.layer(), index, mode);
}

QVariantList Backend::advanced() const
{
    QVariantList l;
    for (const auto& a : snap_.adv)
        l.push_back(QVariantMap{{"layer", a.layer}, {"key", a.key}, {"label", a.label}, {"kind", a.kind},
                                {"summary", a.summary}, {"problem", a.problem}});
    return l;
}

namespace {

bool need_selection(Backend* b, const QVector<int>& sel, int count)
{
    if (sel.size() == count)
        return true;
    emit b->message(count == 2 ? Backend::tr("Select exactly two keys on the keyboard for a pair")
                               : Backend::tr("Select exactly one key on the keyboard"), true);
    return false;
}

}  // namespace

void Backend::createPair(int kind, int socdMode)
{
    const auto sel = keys_.selectedIndices();
    if (need_selection(this, sel, 2))
        emit doAdvPair(sel, keys_.layer(), kind, socdMode);
}

void Backend::createModTap(const QString& tap, const QString& hold, int holdMs)
{
    const auto sel = keys_.selectedIndices();
    if (need_selection(this, sel, 1))
        emit doAdvModTap(sel, keys_.layer(), tap, hold, holdMs);
}

void Backend::createToggle(const QString& code)
{
    const auto sel = keys_.selectedIndices();
    if (need_selection(this, sel, 1))
        emit doAdvToggle(sel, keys_.layer(), code);
}

void Backend::createDks(double shallowMm, double deepMm, const QVariantList& actions)
{
    const auto sel = keys_.selectedIndices();
    if (need_selection(this, sel, 1))
        emit doAdvDks(sel, keys_.layer(), qRound(shallowMm * 10), qRound(deepMm * 10), actions);
}

void Backend::removeAdvanced(int layer, int key) { emit doAdvClear(layer, key); }

QVariantList Backend::storedSpans(int ds, int de, int us, int ue) const
{
    using namespace mchose::glw;
    const DksSpans s = dks_unmask(dks_mask({ds, de, us, ue}));
    return {s.down_start, s.down_end, s.up_start, s.up_end};
}

void Backend::setRotation(const QVariantList& profiles)
{
    QVector<int> l;
    for (const auto& v : profiles)
        l.push_back(v.toInt());
    if (l.isEmpty()) {
        emit message(tr("At least one profile must be in the rotation"), true);
        return;
    }
    emit doRotation(l);
}

void Backend::copyProfile(int src, int dst) { emit doCopyProfile(src, dst); }
void Backend::setReportRate(int hz) { emit doReportRate(hz); }
void Backend::setSettings(bool w, bool at, bool af, bool m, bool mac) { emit doSettings(w, at, af, m, mac); }

void Backend::paintSelected(const QString& color)
{
    const auto sel = keys_.selectedIndices();
    if (sel.isEmpty()) {
        emit message(tr("Select the keys to colour first"), true);
        return;
    }
    emit doPaint(sel, color);
}

QStringList Backend::scriptExamples() const
{
    QStringList l = QDir(":/examples").entryList({"*.lua"}, QDir::Files, QDir::Name);
    return l;
}

QString Backend::exampleSource(const QString& name) const
{
    QFile f(":/examples/" + name);
    return f.open(QIODevice::ReadOnly) ? QString::fromUtf8(f.readAll()) : QString();
}

QString Backend::readTextFile(const QUrl& file)
{
    QFile f(file.toLocalFile());
    if (!f.open(QIODevice::ReadOnly)) {
        emit message(tr("Cannot read %1").arg(file.toLocalFile()), true);
        return {};
    }
    return QString::fromUtf8(f.readAll());
}

bool Backend::writeTextFile(const QUrl& file, const QString& text)
{
    QFile f(file.toLocalFile());
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate) || f.write(text.toUtf8()) < 0) {
        emit message(tr("Cannot write %1").arg(file.toLocalFile()), true);
        return false;
    }
    emit message(tr("Saved %1").arg(file.toLocalFile()), false);
    return true;
}

void Backend::runScript(const QString& source, const QString& name, const QVariantMap& values)
{
    if (persist_) {
        QSettings st;
        // An unchanged example is stored by name only, so that program updates reach it.
        st.setValue("effect/scriptSource", source == exampleSource(name) ? QString() : source);
        st.setValue("effect/scriptName", name);
        saveParams(name, values);
        setRunningEffect("script");
    }
    runningScript_ = name;
    emit doScriptRun(source, name, values);
}

void Backend::previewParams(const QString& source, const QVariantMap& values)
{
    QVariantList params;
    try {
        mchose::script::LightingScript s({}, 128, 1, 1);
        s.set_instruction_limit(2'000'000);
        s.load(source.toStdString(), "preview");
        for (auto it = values.cbegin(); it != values.cend(); ++it)
            s.set_param(it.key().toStdString(), it.value().toDouble());
        for (const auto& p : s.params())
            params.push_back(QVariantMap{{"name", QString::fromStdString(p.name)}, {"label", QString::fromStdString(p.label)},
                                         {"min", p.min}, {"max", p.max}, {"value", p.value}, {"def", p.def}, {"step", p.step}});
    } catch (const std::exception&) {
        // errors are reported when the script is run
    }
    scriptParams_ = params;
    emit scriptParamsChanged();
}
void Backend::stopScript()
{
    if (persist_ && QSettings().value("effect/running").toString() == "script")
        setRunningEffect({});
    emit doScriptStop();
}

void Backend::setScriptParam(const QString& name, double value)
{
    if (persist_ && !runningScript_.isEmpty())
        QSettings().setValue("scriptParams/" + runningScript_ + "/" + name, value);
    emit doScriptParam(name, value);
}

// ---------------------------------------------------------------- settings
// Stored with QSettings (Linux: ~/.config/open-he-mchose/mchose-gui.conf; Windows: the registry, HKCU\Software\open-he-mchose).
//   effect/running        "script", "gamepad" or empty: what is started again when the keyboard connects
//   effect/scriptSource (empty: the built-in example effect/scriptName), effect/scriptName, effect/gamepadConfig
//   scriptParams/<script name>/<param>   values the user set (missing = the script's default)
//   app/keepInBackground  closing the window keeps the program running
//   ui/...                editor texts, window size

QVariantMap Backend::savedParams(const QString& script) const
{
    QVariantMap m;
    if (!persist_ || script.isEmpty())
        return m;
    QSettings st;
    st.beginGroup("scriptParams/" + script);
    for (const QString& k : st.childKeys())
        m.insert(k, st.value(k).toDouble());
    return m;
}

void Backend::saveParams(const QString& script, const QVariantMap& values)
{
    if (!persist_ || script.isEmpty())
        return;
    QSettings st;
    st.remove("scriptParams/" + script);
    st.beginGroup("scriptParams/" + script);
    for (auto it = values.cbegin(); it != values.cend(); ++it)
        st.setValue(it.key(), it.value().toDouble());
}

QVariant Backend::uiValue(const QString& key, const QVariant& def) const
{
    return persist_ ? QSettings().value("ui/" + key, def) : def;
}

void Backend::setUiValue(const QString& key, const QVariant& value)
{
    if (persist_)
        QSettings().setValue("ui/" + key, value);
}

void Backend::setRunningEffect(const QString& kind)
{
    QSettings st;
    st.setValue("effect/running", kind);
    st.sync();   // survives a crash or a killed session
}

// Called on every connect (start, the Connect button, the reconnect after a report-rate change): starts the effect
// that was running when the program was closed.
void Backend::startCalibration() { emit doCalibStart(); }
void Backend::finishCalibration() { emit doCalibEnd(); }
void Backend::resetCalibration() { emit doCalibReset(); }

void Backend::restoreEffect()
{
    if (!persist_ || scriptRunning_ || gamepadRunning_ || calibrating_)
        return;
    QSettings st;
    const QString kind = st.value("effect/running").toString();
    if (kind == "script") {
        const QString name = st.value("effect/scriptName").toString();
        QString source = st.value("effect/scriptSource").toString();
        if (source.isEmpty())
            source = exampleSource(name);
        if (source.isEmpty())
            return;
        runningScript_ = name;
        emit doScriptRun(source, name, savedParams(name));
    } else if (kind == "gamepad") {
        const QString config = st.value("effect/gamepadConfig").toString();
        if (!config.isEmpty())
            emit doGamepadStart(config);
    }
}

bool Backend::keepInBackground() const
{
    return !persist_ || QSettings().value("app/keepInBackground", true).toBool();
}

void Backend::setKeepInBackground(bool on)
{
    if (!persist_)
        return;
    QSettings().setValue("app/keepInBackground", on);
    emit appSettingsChanged();
}

// Start at login: Linux — an XDG autostart entry; Windows — the HKCU Run key. Both start the program hidden.
namespace {
QString executablePath()
{
    const QString appImage = qEnvironmentVariable("APPIMAGE");   // set by the AppImage runtime
    return appImage.isEmpty() ? QCoreApplication::applicationFilePath() : appImage;
}
#if defined(Q_OS_LINUX)
QString autostartFile()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + "/autostart/mchose-gui.desktop";
}
#elif defined(Q_OS_WIN)
const char* const kRunKey = "HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run";
const char* const kRunValue = "MCHOSE configurator";
#endif
}  // namespace

bool Backend::startAtLoginSupported() const
{
#if defined(Q_OS_LINUX) || defined(Q_OS_WIN)
    return true;
#else
    return false;
#endif
}

bool Backend::startAtLogin() const
{
#if defined(Q_OS_LINUX)
    return QFile::exists(autostartFile());
#elif defined(Q_OS_WIN)
    return QSettings(kRunKey, QSettings::NativeFormat).contains(kRunValue);
#else
    return false;
#endif
}

void Backend::setStartAtLogin(bool on)
{
    if (!persist_)
        return;
#if defined(Q_OS_LINUX)
    const QString file = autostartFile();
    if (on) {
        // Desktop Entry quoting: the path in double quotes, with " ` $ \ escaped.
        QString path = executablePath();
        for (const char* c : {"\\", "\"", "`", "$"})
            path.replace(QLatin1String(c), QLatin1String("\\") + QLatin1String(c));
        QDir().mkpath(QFileInfo(file).absolutePath());
        QSaveFile f(file);
        if (f.open(QIODevice::WriteOnly)) {
            f.write(QStringLiteral("[Desktop Entry]\nType=Application\nName=MCHOSE configurator\n"
                                   "Comment=Starts the MCHOSE configurator in the background (restores the lighting effect)\n"
                                   "Exec=\"%1\" --hidden\nIcon=mchose-gui\nTerminal=false\nX-GNOME-Autostart-enabled=true\n")
                        .arg(path).toUtf8());
            if (!f.commit())
                emit message(tr("Cannot write %1").arg(file), true);
        } else {
            emit message(tr("Cannot write %1").arg(file), true);
        }
    } else {
        QFile::remove(file);
    }
#elif defined(Q_OS_WIN)
    QSettings run(kRunKey, QSettings::NativeFormat);
    if (on)
        run.setValue(kRunValue, "\"" + QDir::toNativeSeparators(executablePath()) + "\" --hidden");
    else
        run.remove(kRunValue);
#else
    Q_UNUSED(on);
#endif
    emit appSettingsChanged();
}

// ---------------------------------------------------------------- gamepad

namespace gp = mchose::gamepad;

QStringList Backend::gamepadPresets() const
{
    QStringList l;
    for (const auto& n : gp::preset_names())
        l << QString::fromStdString(n);
    return l;
}

QStringList Backend::gamepadTargets() const
{
    QStringList l;
    for (const auto& n : gp::target_names())
        l << QString::fromStdString(n);
    return l;
}

QString Backend::gamepadPreset(const QString& name) const { return QString::fromStdString(gp::preset_text(name.toStdString())); }

std::optional<int> Backend::keyByName(QStringView name) const
{
    if (name.startsWith(u'#')) {
        bool ok = false;
        const int i = name.mid(1).toInt(&ok);
        return ok && i >= 0 && i < snap_.factoryNames.size() ? std::optional<int>(i) : std::nullopt;
    }
    for (int i = 0; i < snap_.factoryNames.size(); ++i)
        if (snap_.factoryNames[i].compare(name, Qt::CaseInsensitive) == 0)
            return i;
    return std::nullopt;
}

QString Backend::keyName(int index) const
{
    const QString n = snap_.factoryNames.value(index);
    // Names must be unique and readable back; otherwise use the index.
    if (n.isEmpty() || n.contains(u' ') || n.contains(u'=') || n.startsWith(u'#') || keyByName(n) != index)
        return QStringLiteral("#%1").arg(index);
    return n;
}

namespace {

QString shortTarget(gp::Target t)
{
    static const char* names[] = {"L↑", "L↓", "L←", "L→", "R↑", "R↓", "R←", "R→", "LT", "RT", "A", "B", "X", "Y", "LB", "RB",
                                  "Back", "Start", "Guide", "L3", "R3", "D↑", "D↓", "D←", "D→"};
    const auto i = static_cast<std::size_t>(t);
    return i < std::size(names) ? QString::fromUtf8(names[i]) : QString();
}

}  // namespace

QVariantMap Backend::gamepadCheck(const QString& text)
{
    QHash<int, QString> labels;
    try {
        const auto cfg = gp::parse_config(text.toStdString(), [this](std::string_view n) {
            return keyByName(QString::fromUtf8(n.data(), static_cast<int>(n.size())));
        });
        for (const auto& b : cfg.bindings)
            labels[b.key] = labels.contains(b.key) ? labels[b.key] + "," + shortTarget(b.target) : shortTarget(b.target);
        keys_.setGamepadLabels(labels);
        return {{"ok", true}, {"error", QString()}, {"count", static_cast<int>(cfg.bindings.size())},
                {"deadzone", cfg.curve.deadzone}, {"full", cfg.curve.full}, {"button", cfg.curve.button}};
    } catch (const std::exception& e) {
        keys_.setGamepadLabels(labels);
        return {{"ok", false}, {"error", QString::fromStdString(e.what())}, {"count", 0}};
    }
}

namespace {

// The key of a "KEY = TARGET" line, or empty for comments, blank lines and curve settings.
QString lineKey(const QString& line)
{
    QString l = line;
    for (int i = 0; i < l.size(); ++i)
        if (l[i] == u'#' && (i == 0 || l[i - 1].isSpace()) && !(i + 1 < l.size() && l[i + 1].isDigit())) {
            l.truncate(i);
            break;
        }
    const int eq = l.indexOf(u'=', 1);
    if (eq < 0)
        return {};
    const QString k = l.left(eq).trimmed();
    const QString lower = k.toLower();
    if (lower == "deadzone" || lower == "full" || lower == "button")
        return {};
    return k;
}

}  // namespace

QString Backend::gamepadBindSelected(const QString& text, const QString& target)
{
    const auto sel = keys_.selectedIndices();
    if (sel.isEmpty()) {
        emit message(tr("Select the keys to map on the keyboard at the top"), true);
        return text;
    }
    QStringList out;
    for (const QString& line : text.split(u'\n')) {
        const QString k = lineKey(line);
        if (!k.isEmpty()) {
            const auto idx = keyByName(k);
            if (idx && sel.contains(*idx))
                continue;   // replaced below
        }
        out << line;
    }
    while (!out.isEmpty() && out.last().trimmed().isEmpty())
        out.removeLast();
    for (int k : sel)
        out << keyName(k) + " = " + target;
    return out.join(u'\n') + u'\n';
}

QString Backend::gamepadUnbindSelected(const QString& text)
{
    const auto sel = keys_.selectedIndices();
    QStringList out;
    for (const QString& line : text.split(u'\n')) {
        const QString k = lineKey(line);
        if (!k.isEmpty()) {
            const auto idx = keyByName(k);
            if (idx && sel.contains(*idx))
                continue;
        }
        out << line;
    }
    return out.join(u'\n');
}

QString Backend::gamepadSetCurve(const QString& text, const QString& name, double value)
{
    QStringList lines = text.split(u'\n');
    const QString entry = name + " = " + QString::number(value, 'f', 2);
    for (QString& line : lines) {
        const int eq = line.indexOf(u'=');
        if (eq > 0 && line.left(eq).trimmed().compare(name, Qt::CaseInsensitive) == 0) {
            line = entry;
            return lines.join(u'\n');
        }
    }
    while (!lines.isEmpty() && lines.last().trimmed().isEmpty())
        lines.removeLast();
    lines << entry;
    return lines.join(u'\n') + u'\n';
}

void Backend::startGamepad(const QString& text)
{
    if (persist_) {
        QSettings().setValue("effect/gamepadConfig", text);
        setRunningEffect("gamepad");
    }
    emit doGamepadStart(text);
}

void Backend::stopGamepad()
{
    if (persist_ && QSettings().value("effect/running").toString() == "gamepad")
        setRunningEffect({});
    emit doGamepadStop();
}

void Backend::gamepadKeysTyping(const QString& text, bool typing)
{
    QVector<int> keys;
    try {
        const auto cfg = gp::parse_config(text.toStdString(), [this](std::string_view n) {
            return keyByName(QString::fromUtf8(n.data(), static_cast<int>(n.size())));
        });
        for (const auto& b : cfg.bindings)
            if (!keys.contains(b.key))
                keys << b.key;
    } catch (const std::exception& e) {
        emit message(QString::fromStdString(e.what()), true);
        return;
    }
    if (keys.isEmpty())
        return;
    const int layer = snap_.macMode ? 2 : 0;   // the base layer in use
    if (typing)
        emit doReset(keys, layer);
    else
        emit doRemap(keys, layer, QStringLiteral("none"));
}
