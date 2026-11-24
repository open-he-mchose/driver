// SPDX-License-Identifier: GPL-3.0-only
#include "keymodel.hpp"

#include <algorithm>

KeyModel::KeyModel(QObject* parent) : QAbstractListModel(parent) {}

void KeyModel::setLayout(std::span<const mchose::KeyCap> layout)
{
    beginResetModel();
    keys_.clear();
    rowOfIndex_.fill(-1, 256);
    width_ = 0;
    height_ = 0;
    for (const auto& cap : layout) {
        rowOfIndex_[cap.index] = static_cast<int>(keys_.size());
        keys_.push_back({cap, {}, 0, false, false, {}});
        width_ = std::max(width_, static_cast<double>(cap.x + cap.w));
        height_ = std::max(height_, static_cast<double>(cap.y + 1));
    }
    endResetModel();
    emit selectionChanged();
}

void KeyModel::setSnapshot(const Snapshot& s)
{
    for (auto& k : keys_)
        if (k.cap.index < s.keys.size())
            k.settings = s.keys[k.cap.index];
    if (!keys_.isEmpty())
        emit dataChanged(index(0), index(static_cast<int>(keys_.size()) - 1));
}

void KeyModel::setDepth(int matrixIndex, double mm, bool pressed)
{
    if (matrixIndex < 0 || matrixIndex >= rowOfIndex_.size() || rowOfIndex_[matrixIndex] < 0)
        return;
    const int row = rowOfIndex_[matrixIndex];
    keys_[row].depth = mm;
    keys_[row].pressed = pressed;
    emit dataChanged(index(row), index(row), {DepthRole, PressedRole});
}

void KeyModel::setLiveColors(const QByteArray& rgb)
{
    for (auto& k : keys_) {
        const int o = 3 * k.cap.index;
        if (rgb.size() >= o + 3) {
            k.live = QStringLiteral("#%1%2%3")
                         .arg(static_cast<unsigned char>(rgb[o]), 2, 16, QChar('0'))
                         .arg(static_cast<unsigned char>(rgb[o + 1]), 2, 16, QChar('0'))
                         .arg(static_cast<unsigned char>(rgb[o + 2]), 2, 16, QChar('0'));
        } else {
            k.live.clear();
        }
    }
    if (!keys_.isEmpty())
        emit dataChanged(index(0), index(static_cast<int>(keys_.size()) - 1), {LiveColorRole});
}

void KeyModel::clearDepth()
{
    for (auto& k : keys_) {
        k.depth = 0;
        k.pressed = false;
    }
    if (!keys_.isEmpty())
        emit dataChanged(index(0), index(static_cast<int>(keys_.size()) - 1), {DepthRole, PressedRole});
}

QVariant KeyModel::data(const QModelIndex& idx, int role) const
{
    if (!idx.isValid() || idx.row() >= keys_.size())
        return {};
    const Item& k = keys_[idx.row()];
    switch (role) {
    case IndexRole: return k.cap.index;
    case LabelRole: return QString::fromUtf8(k.cap.label);
    case XRole: return k.cap.x;
    case YRole: return k.cap.y;
    case WRole: return k.cap.w;
    case MappingRole: return k.settings.mapping[static_cast<std::size_t>(layer_)];
    case ActuationRole: return k.settings.actuation;
    case ModeRole: return k.settings.mode;
    case PressRole: return k.settings.rtPress;
    case ReleaseRole: return k.settings.rtRelease;
    case ColorRole: return k.settings.color;
    case DepthRole: return k.depth;
    case PressedRole: return k.pressed;
    case SelectedRole: return k.selected;
    case RemappedRole: return k.settings.remapped[static_cast<std::size_t>(layer_)];
    case DangerousRole: return k.settings.dangerous[static_cast<std::size_t>(layer_)];
    case LiveColorRole: return k.live;
    default: return {};
    }
}

QHash<int, QByteArray> KeyModel::roleNames() const
{
    return {{IndexRole, "matrixIndex"}, {LabelRole, "label"},    {XRole, "kx"},           {YRole, "ky"},
            {WRole, "kw"},              {MappingRole, "mapping"}, {ActuationRole, "actuation"}, {ModeRole, "mode"},
            {PressRole, "rtPress"},     {ReleaseRole, "rtRelease"}, {ColorRole, "keyColor"}, {DepthRole, "depth"},
            {PressedRole, "pressed"},   {SelectedRole, "selected"}, {RemappedRole, "remapped"},
            {DangerousRole, "dangerous"}, {LiveColorRole, "liveColor"}};
}

void KeyModel::select(int row, bool add)
{
    if (row < 0 || row >= keys_.size())
        return;
    if (!add)
        for (auto& k : keys_)
            k.selected = false;
    keys_[row].selected = add ? !keys_[row].selected : true;
    emit dataChanged(index(0), index(static_cast<int>(keys_.size()) - 1), {SelectedRole});
    emit selectionChanged();
}

void KeyModel::selectRect(double x0, double y0, double x1, double y1, bool add)
{
    const double ax = std::min(x0, x1), bx = std::max(x0, x1), ay = std::min(y0, y1), by = std::max(y0, y1);
    for (auto& k : keys_) {
        const bool inside = k.cap.x + k.cap.w > ax && k.cap.x < bx && k.cap.y + 1 > ay && k.cap.y < by;
        if (inside)
            k.selected = true;
        else if (!add)
            k.selected = false;
    }
    emit dataChanged(index(0), index(static_cast<int>(keys_.size()) - 1), {SelectedRole});
    emit selectionChanged();
}

void KeyModel::selectAll()
{
    for (auto& k : keys_)
        k.selected = true;
    emit dataChanged(index(0), index(static_cast<int>(keys_.size()) - 1), {SelectedRole});
    emit selectionChanged();
}

void KeyModel::clearSelection()
{
    for (auto& k : keys_)
        k.selected = false;
    emit dataChanged(index(0), index(static_cast<int>(keys_.size()) - 1), {SelectedRole});
    emit selectionChanged();
}

QVariantMap KeyModel::firstSelected() const
{
    for (const auto& k : keys_)
        if (k.selected)
            return {{"label", QString::fromUtf8(k.cap.label)},
                    {"mapping", k.settings.mapping[static_cast<std::size_t>(layer_)]},
                    {"remapped", k.settings.remapped[static_cast<std::size_t>(layer_)]},
                    {"dangerous", k.settings.dangerous[static_cast<std::size_t>(layer_)]},
                    {"actuation", k.settings.actuation}, {"mode", k.settings.mode},
                    {"rtPress", k.settings.rtPress}, {"rtRelease", k.settings.rtRelease}, {"color", k.settings.color}};
    return {};
}

void KeyModel::setLayer(int l)
{
    if (l < 0 || l > 3 || l == layer_)
        return;
    layer_ = l;
    if (!keys_.isEmpty())
        emit dataChanged(index(0), index(static_cast<int>(keys_.size()) - 1), {MappingRole, RemappedRole, DangerousRole});
    emit layerChanged();
    emit selectionChanged();   // panels re-read firstSelected()
}

QVector<int> KeyModel::selectedIndices() const
{
    QVector<int> out;
    for (const auto& k : keys_)
        if (k.selected)
            out.push_back(k.cap.index);
    return out;
}

int KeyModel::selectedCount() const
{
    return static_cast<int>(std::count_if(keys_.begin(), keys_.end(), [](const Item& k) { return k.selected; }));
}
