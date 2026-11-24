// SPDX-License-Identifier: GPL-3.0-only
#include "backend.hpp"

#include "mchose/glw/advanced.hpp"
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
    connect(&macroEditor_, &MacroModel::message, this, &Backend::message);

    connect(worker_, &Worker::connectedChanged, this, [this](bool on, const QString&) {
        connected_ = on;
        emit connectedChanged();
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
}

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
