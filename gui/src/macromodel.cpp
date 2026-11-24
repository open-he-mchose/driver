// SPDX-License-Identifier: GPL-3.0-only
#include "macromodel.hpp"

#include <QHash>

#include <algorithm>
#include <stdexcept>

#include "mchose/glw/keycode.hpp"
#include "mchose/glw/macro_text.hpp"

using namespace mchose::glw;
using Kind = MacroAction::Kind;

namespace {

const char* kModifierNames[8] = {"LCtrl", "LShift", "LAlt", "LGui", "RCtrl", "RShift", "RAlt", "RGui"};

QString action_name(const MacroAction& a)
{
    switch (a.kind) {
    case Kind::Modifier:
        for (int i = 0; i < 8; ++i)
            if (a.code == (1u << i))
                return kModifierNames[i];
        return QStringLiteral("mods %1").arg(a.code);
    case Kind::Key: {
        const char* n = hid_usage_name(a.code);
        return n ? QString::fromLatin1(n) : QStringLiteral("usage %1").arg(a.code);
    }
    case Kind::Mouse: return QStringLiteral("mouse %1").arg(a.code);
    case Kind::Delay: return {};
    }
    return {};
}

// Qt key -> name understood by parse_keycode (US layout; shifted symbols map to their base key: Shift is
// recorded as its own step).
QString qt_key_name(int k)
{
    if (k >= Qt::Key_A && k <= Qt::Key_Z)
        return QString(QChar('A' + (k - Qt::Key_A)));
    if (k >= Qt::Key_0 && k <= Qt::Key_9)
        return QString(QChar('0' + (k - Qt::Key_0)));
    if (k >= Qt::Key_F1 && k <= Qt::Key_F24)
        return QStringLiteral("F%1").arg(k - Qt::Key_F1 + 1);
    static const QHash<int, QString> map{
        {Qt::Key_Escape, "Esc"}, {Qt::Key_Tab, "Tab"}, {Qt::Key_Backtab, "Tab"}, {Qt::Key_Backspace, "Backspace"},
        {Qt::Key_Return, "Enter"}, {Qt::Key_Enter, "Enter"}, {Qt::Key_Space, "Space"}, {Qt::Key_Insert, "Insert"},
        {Qt::Key_Delete, "Delete"}, {Qt::Key_Home, "Home"}, {Qt::Key_End, "End"}, {Qt::Key_PageUp, "PgUp"},
        {Qt::Key_PageDown, "PgDn"}, {Qt::Key_Left, "Left"}, {Qt::Key_Right, "Right"}, {Qt::Key_Up, "Up"},
        {Qt::Key_Down, "Down"}, {Qt::Key_CapsLock, "CapsLock"}, {Qt::Key_Print, "PrtSc"}, {Qt::Key_ScrollLock, "ScrLk"},
        {Qt::Key_Pause, "Pause"}, {Qt::Key_Menu, "Menu"}, {Qt::Key_Shift, "LShift"}, {Qt::Key_Control, "LCtrl"},
        {Qt::Key_Alt, "LAlt"}, {Qt::Key_Meta, "LGui"}, {Qt::Key_Super_L, "LGui"}, {Qt::Key_Super_R, "RGui"},
        {Qt::Key_AltGr, "RAlt"}, {Qt::Key_Minus, "-"}, {Qt::Key_Underscore, "-"}, {Qt::Key_Equal, "="},
        {Qt::Key_Plus, "="}, {Qt::Key_BracketLeft, "["}, {Qt::Key_BraceLeft, "["}, {Qt::Key_BracketRight, "]"},
        {Qt::Key_BraceRight, "]"}, {Qt::Key_Backslash, "\\"}, {Qt::Key_Bar, "\\"}, {Qt::Key_Semicolon, ";"},
        {Qt::Key_Colon, ";"}, {Qt::Key_Apostrophe, "'"}, {Qt::Key_QuoteDbl, "'"}, {Qt::Key_QuoteLeft, "`"},
        {Qt::Key_AsciiTilde, "`"}, {Qt::Key_Comma, "Comma"}, {Qt::Key_Less, "Comma"}, {Qt::Key_Period, "."},
        {Qt::Key_Greater, "."}, {Qt::Key_Slash, "/"}, {Qt::Key_Question, "/"}, {Qt::Key_Exclam, "1"},
        {Qt::Key_At, "2"}, {Qt::Key_NumberSign, "3"}, {Qt::Key_Dollar, "4"}, {Qt::Key_Percent, "5"},
        {Qt::Key_AsciiCircum, "6"}, {Qt::Key_Ampersand, "7"}, {Qt::Key_Asterisk, "8"}, {Qt::Key_ParenLeft, "9"},
        {Qt::Key_ParenRight, "0"}};
    return map.value(k);
}

}  // namespace

MacroModel::MacroModel(QObject* parent) : QAbstractListModel(parent) {}

QVariant MacroModel::data(const QModelIndex& idx, int role) const
{
    if (!idx.isValid() || idx.row() >= static_cast<int>(steps_.size()))
        return {};
    const MacroAction& a = steps_[static_cast<std::size_t>(idx.row())];
    switch (role) {
    case KindRole:
        return a.kind == Kind::Delay ? QStringLiteral("delay") : a.kind == Kind::Mouse ? QStringLiteral("mouse") : QStringLiteral("key");
    case NameRole: return action_name(a);
    case PressRole: return a.press;
    case DelayRole: return a.delay_ms;
    case SummaryRole:
        return a.kind == Kind::Delay ? tr("pause") : (a.press ? tr("press %1") : tr("release %1")).arg(action_name(a));
    default: return {};
    }
}

QHash<int, QByteArray> MacroModel::roleNames() const
{
    return {{KindRole, "kind"}, {NameRole, "name"}, {PressRole, "press"}, {DelayRole, "delay"}, {SummaryRole, "summary"}};
}

void MacroModel::changed() { setModified(true); }

void MacroModel::setModified(bool m)
{
    if (modified_ == m)
        return;
    modified_ = m;
    emit modifiedChanged();
}

void MacroModel::load(const Macro& m)
{
    beginResetModel();
    steps_ = m;
    endResetModel();
    setModified(false);
}

bool MacroModel::loadText(const QString& text)
{
    try {
        auto m = parse_macro(text.toStdString(), static_cast<std::uint16_t>(defaultDelay_));
        beginResetModel();
        steps_ = m;
        endResetModel();
        changed();
        return true;
    } catch (const std::exception& e) {
        emit message(QString::fromStdString(e.what()), true);
        return false;
    }
}

QString MacroModel::text() const { return QString::fromStdString(format_macro(steps_)); }

void MacroModel::insert(const Macro& add)
{
    if (add.empty())
        return;
    const int first = static_cast<int>(steps_.size());
    beginInsertRows({}, first, first + static_cast<int>(add.size()) - 1);
    steps_.insert(steps_.end(), add.begin(), add.end());
    endInsertRows();
    changed();
}

void MacroModel::addTap(const QString& key)
{
    try {
        insert(parse_macro(key.toStdString(), static_cast<std::uint16_t>(defaultDelay_)));
    } catch (const std::exception& e) {
        emit message(QString::fromStdString(e.what()), true);
    }
}

void MacroModel::addPress(const QString& key, bool press)
{
    try {
        insert(parse_macro((press ? "+" : "-") + key.toStdString(), static_cast<std::uint16_t>(defaultDelay_)));
    } catch (const std::exception& e) {
        emit message(QString::fromStdString(e.what()), true);
    }
}

void MacroModel::addDelay(int ms)
{
    insert({{Kind::Delay, 0, false, static_cast<std::uint16_t>(std::clamp(ms, 0, 65535))}});
}

void MacroModel::addText(const QString& typed)
{
    QString escaped = typed;
    escaped.replace('\\', "\\\\").replace('"', "\\\"");
    try {
        insert(parse_macro(("\"" + escaped + "\"").toStdString(), static_cast<std::uint16_t>(defaultDelay_)));
    } catch (const std::exception& e) {
        emit message(QString::fromStdString(e.what()), true);
    }
}

void MacroModel::remove(int row)
{
    if (row < 0 || row >= rowCount())
        return;
    beginRemoveRows({}, row, row);
    steps_.erase(steps_.begin() + row);
    endRemoveRows();
    changed();
}

void MacroModel::move(int row, int delta)
{
    const int to = row + delta;
    if (row < 0 || row >= rowCount() || to < 0 || to >= rowCount() || delta == 0)
        return;
    beginMoveRows({}, row, row, {}, delta > 0 ? to + 1 : to);
    std::swap(steps_[static_cast<std::size_t>(row)], steps_[static_cast<std::size_t>(to)]);
    endMoveRows();
    changed();
}

void MacroModel::setDelay(int row, int ms)
{
    if (row < 0 || row >= rowCount())
        return;
    steps_[static_cast<std::size_t>(row)].delay_ms = static_cast<std::uint16_t>(std::clamp(ms, 0, 65535));
    emit dataChanged(index(row), index(row));
    changed();
}

void MacroModel::setKey(int row, const QString& key)
{
    if (row < 0 || row >= rowCount())
        return;
    auto c = parse_keycode(key.toStdString());
    MacroAction& a = steps_[static_cast<std::size_t>(row)];
    if (!c || c->type != KeyCode::Keyboard || (c->code1 && c->code2) || (!c->code1 && !c->code2)) {
        emit message(tr("A step takes one key or one modifier: '%1'").arg(key), true);
        emit dataChanged(index(row), index(row));
        return;
    }
    if (c->code1) {
        a.kind = Kind::Modifier;
        a.code = c->code1;
    } else {
        a.kind = Kind::Key;
        a.code = c->code2;
    }
    emit dataChanged(index(row), index(row));
    changed();
}

void MacroModel::setPress(int row, bool press)
{
    if (row < 0 || row >= rowCount() || steps_[static_cast<std::size_t>(row)].kind == Kind::Delay)
        return;
    steps_[static_cast<std::size_t>(row)].press = press;
    emit dataChanged(index(row), index(row));
    changed();
}

void MacroModel::clear()
{
    beginResetModel();
    steps_.clear();
    endResetModel();
    changed();
}

void MacroModel::setDefaultDelay(int ms)
{
    ms = std::clamp(ms, 0, 65535);
    if (ms == defaultDelay_)
        return;
    defaultDelay_ = ms;
    emit defaultDelayChanged();
}

void MacroModel::startRecording(bool realDelays)
{
    realDelays_ = realDelays;
    recording_ = true;
    recordClock_.start();
    emit recordingChanged();
}

void MacroModel::stopRecording()
{
    if (!recording_)
        return;
    recording_ = false;
    emit recordingChanged();
}

bool MacroModel::recordKey(int qtKey, bool pressed, bool autoRepeat)
{
    if (!recording_ || autoRepeat)
        return false;
    const QString name = qt_key_name(qtKey);
    if (name.isEmpty()) {
        emit message(tr("This key cannot be recorded; add it by name"), true);
        return false;
    }
    if (realDelays_ && !steps_.empty()) {
        const qint64 ms = recordClock_.restart();
        setDelay(rowCount() - 1, static_cast<int>(std::min<qint64>(ms, 65535)));
    } else {
        recordClock_.restart();
    }
    addPress(name, pressed);
    return true;
}

QStringList MacroModel::keyNames() const
{
    QStringList l;
    for (const char* m : kModifierNames)
        l << m;
    for (int u = 0x04; u <= 0x73; ++u)
        if (const char* n = hid_usage_name(static_cast<std::uint8_t>(u)))
            l << (u == 0x36 ? QStringLiteral("Comma") : QString::fromLatin1(n));
    return l;
}
