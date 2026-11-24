// SPDX-License-Identifier: GPL-3.0-only
// Steps of the macro being edited (core MacroAction list) for the QML macro editor, plus recording.
#pragma once

#include <QAbstractListModel>
#include <QElapsedTimer>
#include <QStringList>

#include "mchose/glw/macro.hpp"

class MacroModel : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int defaultDelay READ defaultDelay WRITE setDefaultDelay NOTIFY defaultDelayChanged)
    Q_PROPERTY(bool recording READ recording NOTIFY recordingChanged)
    Q_PROPERTY(QStringList keyNames READ keyNames CONSTANT)
    Q_PROPERTY(bool modified READ modified NOTIFY modifiedChanged)
public:
    enum Role { KindRole = Qt::UserRole + 1, NameRole, PressRole, DelayRole, SummaryRole };

    explicit MacroModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& = {}) const override { return static_cast<int>(steps_.size()); }
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void load(const mchose::glw::Macro& m);
    const mchose::glw::Macro& steps() const { return steps_; }

    Q_INVOKABLE bool loadText(const QString& text);   // false (and message) on a parse error
    Q_INVOKABLE QString text() const;
    Q_INVOKABLE void addTap(const QString& key);       // key or combination, e.g. "A", "Ctrl+C"
    Q_INVOKABLE void addPress(const QString& key, bool press);
    Q_INVOKABLE void addDelay(int ms);
    Q_INVOKABLE void addText(const QString& typed);
    Q_INVOKABLE void remove(int row);
    Q_INVOKABLE void move(int row, int delta);
    Q_INVOKABLE void setDelay(int row, int ms);
    Q_INVOKABLE void setKey(int row, const QString& key);
    Q_INVOKABLE void setPress(int row, bool press);
    Q_INVOKABLE void clear();

    // Recording from the editor window: Qt key events. With realDelays the time between events becomes the
    // delay of the previous step, else the default delay is used.
    Q_INVOKABLE void startRecording(bool realDelays);
    Q_INVOKABLE void stopRecording();
    Q_INVOKABLE bool recordKey(int qtKey, bool pressed, bool autoRepeat);

    int defaultDelay() const { return defaultDelay_; }
    void setDefaultDelay(int ms);
    bool recording() const { return recording_; }
    bool modified() const { return modified_; }
    void setModified(bool m);
    QStringList keyNames() const;

signals:
    void defaultDelayChanged();
    void recordingChanged();
    void modifiedChanged();
    void message(QString text, bool error);

private:
    void insert(const mchose::glw::Macro& add);
    void changed();

    mchose::glw::Macro steps_;
    int defaultDelay_ = 10;
    bool recording_ = false;
    bool realDelays_ = true;
    bool modified_ = false;
    QElapsedTimer recordClock_;
};
