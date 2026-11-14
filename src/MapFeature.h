#pragma once

#include <QColor>
#include <QGraphicsItem>
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

QString terrainName(TerrainType terrain);
QColor terrainColor(TerrainType terrain);
const QVector<TerrainType> &terrainTypes();

/// One map object. A single class keeps drawing and saving simple.
class MapFeature : public QGraphicsItem
{
public:
    explicit MapFeature(FeatureKind kind, QGraphicsItem *parent = nullptr);

    FeatureKind kind() const { return m_kind; }
    QString kindName() const;

    QPolygonF polygon() const { return m_polygon; }
    void setPolygon(const QPolygonF &polygon);
    QPointF centroid() const;

    QRectF boundingRect() const override;
    QPainterPath shape() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

    QString name;
    QString description;
    QString countryName;
    QColor color;
    TerrainType terrain = TerrainType::Grass;

private:
    FeatureKind m_kind;
    QPolygonF m_polygon;
};
