#pragma once

#include <QColor>
#include <QFont>
#include <QGraphicsItem>
#include <QJsonObject>
#include <QPolygonF>
#include <QString>
#include <QVector>

enum class FeatureKind {
    Land,
    Country,
    TerrainArea,
    Region,
    Settlement,
};

enum class TerrainType {
    Grass,
    Forest,
    Desert,
    Mountains,
    Water,
    Swamp,
};

enum class SettlementType {
    City,
    Town,
    Village,
};

QString terrainName(TerrainType terrain);
QColor terrainColor(TerrainType terrain);
const QVector<TerrainType> &terrainTypes();
QString settlementName(SettlementType type);

/// One map object. A single class keeps drawing and saving simple.
class MapFeature : public QGraphicsItem
{
public:
    explicit MapFeature(FeatureKind kind, QGraphicsItem *parent = nullptr);

    FeatureKind kind() const { return m_kind; }
    QString kindName() const;

    QPolygonF polygon() const { return m_polygon; }
    void setPolygon(const QPolygonF &polygon);
    QPointF point() const { return m_point; }
    void setPoint(const QPointF &point);
    QPointF centroid() const;

    QRectF boundingRect() const override;
    QPainterPath shape() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

    QString name;
    QString description;
    QString countryName;
    QColor color;
    TerrainType terrain = TerrainType::Grass;
    SettlementType settlement = SettlementType::City;
    bool highlighted = false;

    QJsonObject toJson() const;

private:
    QFont labelFont() const;

    FeatureKind m_kind;
    QPolygonF m_polygon;
    QPointF m_point;
};
