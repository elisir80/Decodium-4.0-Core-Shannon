#pragma once

#include <QAbstractListModel>
#include <QByteArray>
#include <QHash>
#include <QString>
#include <QVariant>
#include <QVector>

class MapLayerModel final : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Role {
        LayerIdRole = Qt::UserRole + 1,
        LabelRole,
        ColorRole,
        LayerEnabledRole,
        CountRole
    };
    Q_ENUM(Role)

    explicit MapLayerModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE bool layerEnabled(const QString& id) const;
    void setCount(const QString& id, int count);

    Q_INVOKABLE void setLayerEnabled(const QString& id, bool enabled);
    Q_INVOKABLE void toggleLayer(const QString& id);

signals:
    void layerToggled(const QString& id, bool enabled);

private:
    struct Layer {
        QString id;
        QString label;
        QString color;
        bool enabled {true};
        int count {0};
    };

    int indexOf(const QString& id) const;

    QVector<Layer> m_layers;
};
