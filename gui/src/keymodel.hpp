// SPDX-License-Identifier: GPL-3.0-only
// The keys of the physical layout with their current settings, for the QML keyboard view.
#pragma once

#include <QAbstractListModel>
#include <QVector>

#include "mchose/model.hpp"
#include "worker.hpp"

class KeyModel : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int selectedCount READ selectedCount NOTIFY selectionChanged)
    Q_PROPERTY(double widthUnits READ widthUnits CONSTANT)
    Q_PROPERTY(double heightUnits READ heightUnits CONSTANT)
    Q_PROPERTY(int layer READ layer WRITE setLayer NOTIFY layerChanged)   // keymap layer shown by the mapping roles
public:
    enum Role {
        IndexRole = Qt::UserRole + 1, LabelRole, XRole, YRole, WRole, MappingRole, ActuationRole, ModeRole,
        PressRole, ReleaseRole, ColorRole, DepthRole, PressedRole, SelectedRole, RemappedRole, DangerousRole, LiveColorRole
    };

    explicit KeyModel(QObject* parent = nullptr);

    void setLayout(std::span<const mchose::KeyCap> layout);
    void setSnapshot(const Snapshot& s);
    void setDepth(int matrixIndex, double mm, bool pressed);
    void clearDepth();
    void setLiveColors(const QByteArray& rgb);   // empty: no preview

    int rowCount(const QModelIndex& = {}) const override { return static_cast<int>(keys_.size()); }
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE void select(int row, bool add);
    Q_INVOKABLE void selectRect(double x0, double y0, double x1, double y1, bool add);   // key units
    Q_INVOKABLE void selectAll();
    Q_INVOKABLE void clearSelection();
    Q_INVOKABLE QVariantMap firstSelected() const;   // settings of the first selected key
    Q_INVOKABLE QVariantMap keyInfo(int matrixIndex) const;   // label and trigger settings of one key
    QVector<int> selectedIndices() const;            // matrix indices
    int selectedCount() const;
    double widthUnits() const { return width_; }
    double heightUnits() const { return height_; }
    int layer() const { return layer_; }
    void setLayer(int l);

signals:
    void selectionChanged();
    void layerChanged();
    void depthSample(int matrixIndex, double mm, bool pressed);   // every depth-stream update

private:
    struct Item {
        mchose::KeyCap cap;
        Snapshot::Key settings;
        double depth = 0;
        bool pressed = false;
        bool selected = false;
        QString live;   // "#rrggbb" while an effect runs, else empty
    };
    QVector<Item> keys_;
    QVector<int> rowOfIndex_;   // matrix index -> row
    double width_ = 16, height_ = 6.25;
    int layer_ = 0;
};
