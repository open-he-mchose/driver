// SPDX-License-Identifier: GPL-3.0-only
#include "worker.hpp"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QStandardPaths>

#include <cmath>
#include <cstdio>

#include "mchose/glw/backup_file.hpp"
#include "mchose/glw/macro_text.hpp"

using namespace mchose;
using namespace mchose::glw;

namespace {

// Factory codes are those of profile 0; Fn keys (F0 FF layer) carry absolute layer numbers.
KeyCode forProfile(KeyCode c, int profile)
{
    if (c.type == KeyCode::Special && c.code1 == 0xFF && c.code2 < 4)
        c.code2 = static_cast<std::uint8_t>(c.code2 + 4 * profile);
    return c;
}

QString hex_color(std::uint8_t r, std::uint8_t g, std::uint8_t b)
{
    char buf[8];
    std::snprintf(buf, sizeof buf, "#%02x%02x%02x", r, g, b);
    return QString::fromLatin1(buf);
}

std::array<std::uint8_t, 3> parse_color(const QString& s)
{
    QString h = s.startsWith('#') ? s.mid(1) : s;
    bool ok = false;
    const uint v = h.toUInt(&ok, 16);
    if (!ok || h.size() != 6)
        throw std::runtime_error("colour must be #rrggbb");
    return {static_cast<std::uint8_t>(v >> 16), static_cast<std::uint8_t>(v >> 8), static_cast<std::uint8_t>(v)};
}

}  // namespace

Worker::Worker()
{
    streamTimer_ = new QTimer(this);
    streamTimer_->setInterval(5);
    connect(streamTimer_, &QTimer::timeout, this, &Worker::pollStream);
}

Worker::~Worker()
{
    if (kb_ && streamTimer_->isActive()) {
        try {
            kb_->set_depth_stream(false);
        } catch (...) {
        }
    }
}

bool Worker::ready()
{
    if (kb_)
        return true;
    emit message(tr("No keyboard connected"), true);
    return false;
}

template <class F>
void Worker::run(const QString& what, F&& f, bool reread)
{
    if (!ready())
        return;
    emit busyChanged(true);
    try {
        f();
        if (!what.isEmpty())
            emit message(what, false);
        if (reread)
            refresh();
    } catch (const SafetyError& e) {
        emit message(tr("Refused: %1").arg(QString::fromStdString(e.what())), true);
    } catch (const std::exception& e) {
        emit message(QString::fromStdString(e.what()), true);
    }
    emit busyChanged(false);
}

void Worker::connectDevice()
{
    emit busyChanged(true);
    try {
        streamTimer_->stop();
        kb_.reset();
        transport_.reset();
        auto devs = hid::enumerate();
        const hid::DeviceInfo* chosen = nullptr;
        for (const auto& d : devs)
            if (d.model && !chosen)
                chosen = &d;
        if (!chosen) {
            emit connectedChanged(false, {});
            emit message(devs.empty() ? tr("No MCHOSE keyboard found") : tr("This MCHOSE keyboard is not supported yet"), true);
            emit busyChanged(false);
            return;
        }
        transport_ = std::make_unique<hid::HidapiTransport>(chosen->path);
        kb_ = std::make_unique<Keyboard>(*transport_, *chosen->model);
        factoryIndex_.clear();
        for (int l = 0; l < 4 && l < kb_->model().layers; ++l)
            factory_[static_cast<std::size_t>(l)] = kb_->factory_keymap(l);
        for (int i = 0; i < kb_->model().keys; ++i)
            factoryIndex_[factory_[0][static_cast<std::size_t>(i)].bytes()] = i;
        backedUp_ = false;
        emit connectedChanged(true, QString::fromStdString(chosen->model->name));
        refresh();
    } catch (const std::exception& e) {
        kb_.reset();
        transport_.reset();
        emit connectedChanged(false, {});
        emit message(QString::fromStdString(e.what()), true);
    }
    emit busyChanged(false);
}

void Worker::refresh()
{
    if (!kb_)
        return;
    try {
        Snapshot s;
        const Model& m = kb_->model();
        s.model = QString::fromStdString(m.name);
        auto fi = kb_->info();
        s.firmware = QString::fromStdString(fi.version_string() + " (" + fi.build + ")");
        auto b = kb_->base_info();
        s.activeProfile = b.runtime_profile();
        for (int i = 0; i < b.length() && i < 4; ++i)
            s.rotation.push_back(b.order(i));
        auto fc = kb_->funcconfig(s.activeProfile);
        s.reportRateHz = report_rate_hz(fc.report_rate()).value_or(0);
        s.lightEffect = fc.light_effect();
        s.lightBrightness = fc.light_brightness();
        s.lightSpeed = fc.light_speed();
        auto c = fc.light_color();
        s.lightColor = hex_color(c[0], c[1], c[2]);
        std::array<std::vector<KeyCode>, 4> km;
        for (int l = 0; l < 4 && l < m.layers; ++l)
            km[static_cast<std::size_t>(l)] = kb_->keymap(s.activeProfile, l);
        auto trig = kb_->triggers(s.activeProfile);
        auto colors = kb_->colors(s.activeProfile);
        const double unit = m.distance_unit_um / 1000.0;
        for (int i = 0; i < m.keys; ++i) {
            const auto k = static_cast<std::size_t>(i);
            Snapshot::Key key;
            for (std::size_t l = 0; l < 4; ++l) {
                if (km[l].empty())
                    continue;
                const KeyCode code = km[l][k];
                const bool transparent = l > 0 && code == KeyCode{KeyCode::Keyboard, 0, 0};
                key.mapping[l] = transparent ? QString() : QString::fromStdString(to_string(code));
                key.remapped[l] = k < factory_[l].size() && code != forProfile(factory_[l][k], s.activeProfile);
                key.dangerous[l] = code.is_dangerous();
            }
            key.actuation = trig[k].actuation() * unit;
            key.rtPress = trig[k].rt_press() * unit;
            key.rtRelease = trig[k].rt_release() * unit;
            key.mode = static_cast<int>(trig[k].mode());
            key.color = hex_color(colors[k][0], colors[k][1], colors[k][2]);
            s.keys.push_back(key);
        }
        // Macros (shared by all profiles) and the keys bound to them in this profile.
        auto macros = kb_->macros();
        for (int i = 0; i < kMacroCount; ++i) {
            s.macroTexts << QString::fromStdString(format_macro(macros[static_cast<std::size_t>(i)]));
            s.macroSteps << static_cast<int>(macros[static_cast<std::size_t>(i)].size());
            s.macroBindings << QString();
        }
        static const char* layerNames[4] = {"", "Fn+", "Mac ", "Mac Fn+"};
        for (std::size_t l = 0; l < 4; ++l)
            for (int k = 0; k < static_cast<int>(km[l].size()) && k < m.keys; ++k) {
                const KeyCode c = km[l][static_cast<std::size_t>(k)];
                if (c.type != KeyCode::Macro || c.code1 >= kMacroCount)
                    continue;
                QString label = QString::fromStdString(to_string(factory_[0][static_cast<std::size_t>(k)]));
                const QString mode = c.code2 == 0 ? tr("hold") : c.code2 == 0xFF ? tr("toggle") : c.code2 == 1 ? tr("once") : tr("%1×").arg(c.code2);
                QString& b = s.macroBindings[c.code1];
                b += (b.isEmpty() ? "" : ", ") + QString::fromLatin1(layerNames[l]) + label + " (" + mode + ")";
            }
        emit refreshed(s);
    } catch (const std::exception& e) {
        emit message(QString::fromStdString(e.what()), true);
    }
}

void Worker::autoBackup()
{
    if (backedUp_)
        return;
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + "/backups";
    QDir().mkpath(dir);
    const QString file = dir + "/backup-" + QDateTime::currentDateTime().toString("yyyyMMdd-HHmmss") + ".json";
    auto img = kb_->backup();
    QFile f(file);
    if (!f.open(QIODevice::WriteOnly))
        throw std::runtime_error("cannot write the automatic backup " + file.toStdString());
    f.write(QByteArray::fromStdString(backup_to_json(img, QDateTime::currentDateTime().toString(Qt::ISODate).toStdString())));
    backedUp_ = true;
    emit message(tr("Backup saved before the first change: %1").arg(file), false);
}

void Worker::switchProfile(int profile)
{
    run(tr("Profile %1 active").arg(profile), [&] {
        WriteOptions o;
        o.allow_outside_rotation = true;   // the profile list in the UI shows all four
        kb_->switch_profile(profile, o);
    });
}

void Worker::applyTrigger(QVector<int> keys, double actuation, int mode, double press, double release)
{
    run(tr("Trigger settings written for %n key(s)", nullptr, static_cast<int>(keys.size())), [&] {
        autoBackup();
        const int p = kb_->active_profile();
        auto tr = kb_->triggers(p);
        const double per_mm = 1000.0 / kb_->model().distance_unit_um;
        for (int k : keys) {
            TriggerEntry e = tr[static_cast<std::size_t>(k)];
            e.set_actuation(static_cast<int>(std::lround(actuation * per_mm)));
            e.set_mode(static_cast<TriggerEntry::Mode>(mode));
            e.set_rt_press(static_cast<int>(std::lround(press * per_mm)));
            e.set_rt_release(static_cast<int>(std::lround(release * per_mm)));
            if (e.raw != tr[static_cast<std::size_t>(k)].raw)
                kb_->set_trigger(p, k, e);
        }
    });
}

void Worker::setKeys(const QVector<int>& keys, int layer, const std::function<KeyCode(int)>& codeFor, bool allowDangerous)
{
    if (layer < 0 || layer >= kb_->model().layers)
        throw std::runtime_error("layer out of range");
    autoBackup();
    const int p = kb_->active_profile();
    auto current = kb_->keymap(p, layer);
    WriteOptions opt;
    opt.allow_dangerous = allowDangerous;
    for (int k : keys) {
        const KeyCode c = codeFor(k);
        if (current[static_cast<std::size_t>(k)] != c)
            kb_->set_key(p, layer, k, c, opt);
    }
}

void Worker::remap(QVector<int> keys, int layer, QString code)
{
    run(tr("Key mapping written"), [&] {
        auto c = parse_keycode(code.toStdString());
        if (!c)
            throw std::runtime_error("unknown key code '" + code.toStdString() + "'");
        setKeys(keys, layer, [&](int) { return *c; });
    });
}

void Worker::assignFactoryOf(QVector<int> keys, int layer, int source)
{
    run(tr("Key mapping written"), [&] {
        if (source < 0 || source >= static_cast<int>(factory_[0].size()))
            throw std::runtime_error("unknown source key");
        const KeyCode c = forProfile(factory_[0][static_cast<std::size_t>(source)], kb_->active_profile());
        setKeys(keys, layer, [&](int) { return c; });
    });
}

void Worker::resetToFactory(QVector<int> keys, int layer)
{
    run(tr("Default mapping restored"), [&] {
        const int p = kb_->active_profile();
        const auto& f = factory_[static_cast<std::size_t>(layer)];
        // The factory Fn layer holds the reset codes (Fn+Esc, Fn+Backspace): restoring them is restoring the
        // keyboard's own defaults, so they are allowed here.
        setKeys(keys, layer, [&](int k) { return forProfile(f[static_cast<std::size_t>(k)], p); }, true);
    });
}

void Worker::setMacro(int index, QString text)
{
    run(text.trimmed().isEmpty() ? tr("Macro M%1 cleared").arg(index + 1) : tr("Macro M%1 saved").arg(index + 1), [&] {
        if (index < 0 || index >= kMacroCount)
            throw std::runtime_error("macro index out of range");
        Macro m = text.trimmed().isEmpty() ? Macro{} : parse_macro(text.toStdString());
        autoBackup();
        auto set = kb_->macros();
        set[static_cast<std::size_t>(index)] = m;
        kb_->set_macros(set);
    });
}

void Worker::bindMacro(QVector<int> keys, int layer, int index, int mode)
{
    run(tr("Macro M%1 bound").arg(index + 1), [&] {
        if (index < 0 || index >= kMacroCount || mode < 0 || mode > 255)
            throw std::runtime_error("bad macro binding");
        const KeyCode c{KeyCode::Macro, static_cast<std::uint8_t>(index), static_cast<std::uint8_t>(mode)};
        setKeys(keys, layer, [&](int) { return c; });
    });
}

void Worker::applyLight(int effect, int brightness, int speed, QString color)
{
    run(tr("Lighting written"), [&] {
        autoBackup();
        const int p = kb_->active_profile();
        FuncConfig fc = kb_->funcconfig(p);
        fc.set_light_effect(static_cast<std::uint8_t>(effect));
        fc.set_light_brightness(static_cast<std::uint8_t>(brightness));
        fc.set_light_speed(static_cast<std::uint8_t>(speed));
        auto c = parse_color(color);
        fc.set_light_color(c[0], c[1], c[2]);
        kb_->set_funcconfig(p, fc);
    });
}

void Worker::backup(QString path)
{
    run(tr("Backup saved: %1").arg(path), [&] {
        auto img = kb_->backup();
        QFile f(path);
        if (!f.open(QIODevice::WriteOnly))
            throw std::runtime_error("cannot write " + path.toStdString());
        f.write(QByteArray::fromStdString(backup_to_json(img, QDateTime::currentDateTime().toString(Qt::ISODate).toStdString())));
    }, false);
}

void Worker::restore(QString path)
{
    int writes = 0;
    run({}, [&] {
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly))
            throw std::runtime_error("cannot read " + path.toStdString());
        auto img = backup_from_json(f.readAll().toStdString());
        autoBackup();
        writes = kb_->restore(img);
        emit message(writes ? tr("Restored (%1 writes)").arg(writes) : tr("The keyboard already matches the backup"), false);
    });
}

void Worker::setStreaming(bool on)
{
    if (!ready())
        return;
    try {
        kb_->set_depth_stream(on);
        if (on)
            streamTimer_->start();
        else
            streamTimer_->stop();
        emit streamingChanged(on);
    } catch (const std::exception& e) {
        emit message(QString::fromStdString(e.what()), true);
    }
}

void Worker::pollStream()
{
    if (!kb_)
        return;
    try {
        // Drain what arrived; no requests are sent while streaming.
        for (int n = 0; n < 32; ++n) {
            auto rep = kb_->session().transport().read(std::chrono::milliseconds(0));
            if (!rep)
                break;
            if (auto s = parse_depth_sample(*rep)) {
                auto it = factoryIndex_.find(s->key.bytes());
                if (it != factoryIndex_.end())
                    emit depth(it->second, s->travel / 100.0, s->pressed);
            }
        }
    } catch (const std::exception& e) {
        streamTimer_->stop();
        emit streamingChanged(false);
        emit message(QString::fromStdString(e.what()), true);
    }
}
