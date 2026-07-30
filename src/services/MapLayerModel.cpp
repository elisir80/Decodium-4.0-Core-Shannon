#include "MapLayerModel.h"

MapLayerModel::MapLayerModel(QObject* parent)
    : QAbstractListModel(parent)
    , m_layers {
          {QStringLiteral("live"), QStringLiteral("LIVE"), QStringLiteral("#00d8ff"), true, 0},
          {QStringLiteral("worked"), QStringLiteral("HISTORICAL"), QStringLiteral("#42c7d9"), true, 0},
          {QStringLiteral("confirmed"), QStringLiteral("CONFIRMED"), QStringLiteral("#2ecc71"), true, 0},
          {QStringLiteral("active"), QStringLiteral("ACTIVE GRIDS"), QStringLiteral("#f6c344"), true, 0},
          {QStringLiteral("missing"), QStringLiteral("MISSING GRIDS"), QStringLiteral("#ff8c42"), true, 0},
          {QStringLiteral("psk"), QStringLiteral("PSK REPORTER"), QStringLiteral("#ba7cff"), true, 0},
          {QStringLiteral("pota"), QStringLiteral("POTA"), QStringLiteral("#74d66a"), false, 0},
          {QStringLiteral("states"), QStringLiteral("STATES"), QStringLiteral("#58b8d6"), false, 0},
          {QStringLiteral("counties"), QStringLiteral("COUNTIES"), QStringLiteral("#7c91a8"), false, 0},
          {QStringLiteral("iota"), QStringLiteral("IOTA"), QStringLiteral("#44d7e8"), false, 0},
          {QStringLiteral("wpx"), QStringLiteral("WPX"), QStringLiteral("#f0b94d"), false, 0},
          {QStringLiteral("moon"), QStringLiteral("MOON"), QStringLiteral("#dbe7ff"), false, 0},
          {QStringLiteral("propagation"), QStringLiteral("PROPAGATION"), QStringLiteral("#ffcf66"), false, 0},
          {QStringLiteral("radar"), QStringLiteral("RADAR WORLD"), QStringLiteral("#ff4d4d"), false, 0},
          {QStringLiteral("lightning"), QStringLiteral("LIGHTNING"), QStringLiteral("#ffffff"), false, 0},
          {QStringLiteral("muf"), QStringLiteral("MUF"), QStringLiteral("#f6c344"), false, 0},
          {QStringLiteral("fof2"), QStringLiteral("foF2"), QStringLiteral("#66d9ff"), false, 0},
          {QStringLiteral("es"), QStringLiteral("Es"), QStringLiteral("#ff9f43"), false, 0},
          {QStringLiteral("aurora"), QStringLiteral("AURORA"), QStringLiteral("#77ff9f"), false, 0},
          {QStringLiteral("tropo"), QStringLiteral("TROPO"), QStringLiteral("#ffd166"), false, 0},
          {QStringLiteral("earthquakes"), QStringLiteral("EARTHQUAKES"), QStringLiteral("#ff5f57"), false, 0},
          {QStringLiteral("wildfires"), QStringLiteral("WILDFIRES"), QStringLiteral("#ff9f43"), false, 0},
          {QStringLiteral("offline"), QStringLiteral("OFFLINE MODE"), QStringLiteral("#8fa8c4"), false, 0}
      }
{
}

int MapLayerModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : m_layers.size();
}

QVariant MapLayerModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_layers.size()) {
        return {};
    }

    Layer const& layer = m_layers.at(index.row());
    switch (role) {
    case Qt::DisplayRole:
    case LabelRole:
        return layer.label;
    case LayerIdRole:
        return layer.id;
    case ColorRole:
        return layer.color;
    case LayerEnabledRole:
        return layer.enabled;
    case CountRole:
        return layer.count;
    default:
        return {};
    }
}

QHash<int, QByteArray> MapLayerModel::roleNames() const
{
    return {
        {LayerIdRole, QByteArrayLiteral("layerId")},
        {LabelRole, QByteArrayLiteral("label")},
        {ColorRole, QByteArrayLiteral("layerColor")},
        {LayerEnabledRole, QByteArrayLiteral("layerEnabled")},
        {CountRole, QByteArrayLiteral("layerCount")}
    };
}

bool MapLayerModel::layerEnabled(const QString& id) const
{
    int const row = indexOf(id);
    return row >= 0 ? m_layers.at(row).enabled : false;
}

void MapLayerModel::setCount(const QString& id, int count)
{
    int const row = indexOf(id);
    if (row < 0 || m_layers.at(row).count == count) {
        return;
    }
    m_layers[row].count = count;
    QModelIndex const modelIndex = index(row, 0);
    emit dataChanged(modelIndex, modelIndex, {CountRole});
}

void MapLayerModel::setLayerEnabled(const QString& id, bool enabled)
{
    int const row = indexOf(id);
    if (row < 0 || m_layers.at(row).enabled == enabled) {
        return;
    }
    m_layers[row].enabled = enabled;
    QModelIndex const modelIndex = index(row, 0);
    emit dataChanged(modelIndex, modelIndex, {LayerEnabledRole});
    emit layerToggled(m_layers.at(row).id, enabled);
}

void MapLayerModel::toggleLayer(const QString& id)
{
    setLayerEnabled(id, !layerEnabled(id));
}

int MapLayerModel::indexOf(const QString& id) const
{
    for (int row = 0; row < m_layers.size(); ++row) {
        if (m_layers.at(row).id.compare(id, Qt::CaseInsensitive) == 0) {
            return row;
        }
    }
    return -1;
}
