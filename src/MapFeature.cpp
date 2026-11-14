#include "MapFeature.h"

#include <QPainter>
#include <QPainterPath>
#include <QStyleOptionGraphicsItem>

MapFeature::MapFeature(FeatureKind kind, QGraphicsItem *parent)
    : QGraphicsItem(parent)
    , m_kind(kind)
{
    switch (kind) {
    case FeatureKind::Land:
        setZValue(0);
        break;
    case FeatureKind::TerrainArea:
        setZValue(1);
        break;
    case FeatureKind::Country:
        setZValue(2);
        break;
    case FeatureKind::Region:
        setZValue(3);
        break;
    case FeatureKind::Settlement:
        setZValue(5);
        break;
    }
}

QString MapFeature::kindName() const
{
    switch (m_kind) {
    case FeatureKind::Land:
        return QStringLiteral("Land");
    case FeatureKind::Country:
        return QStringLiteral("Country");
    case FeatureKind::TerrainArea:
        return QStringLiteral("Terrain");
    case FeatureKind::Region:
        return QStringLiteral("Region");
    case FeatureKind::Settlement:
        return QStringLiteral("Settlement");
    }
    return QStringLiteral("Feature");
}

void MapFeature::setPolygon(const QPolygonF &polygon)
{
    prepareGeometryChange();
    m_polygon = polygon;
    update();
}

QPointF MapFeature::centroid() const
{
    if (m_polygon.isEmpty())
        return QPointF();

    // Area-weighted centroid, falling back to the average point for shapes
    // with (near) zero area.
    double area = 0.0;
    double cx = 0.0;
    double cy = 0.0;
    for (int i = 0; i < m_polygon.size(); ++i) {
        const QPointF &a = m_polygon.at(i);
        const QPointF &b = m_polygon.at((i + 1) % m_polygon.size());
        const double cross = a.x() * b.y() - b.x() * a.y();
        area += cross;
        cx += (a.x() + b.x()) * cross;
        cy += (a.y() + b.y()) * cross;
    }
    if (qFuzzyIsNull(area))
        return m_polygon.boundingRect().center();

    area *= 0.5;
    return QPointF(cx / (6.0 * area), cy / (6.0 * area));
}

QRectF MapFeature::boundingRect() const
{
    if (m_kind == FeatureKind::Settlement)
        return QRectF(-12, -12, 24, 24);
    return m_polygon.boundingRect().adjusted(-4, -4, 4, 4);
}

QPainterPath MapFeature::shape() const
{
    QPainterPath path;
    path.addPolygon(m_polygon);
    path.closeSubpath();
    return path;
}

void MapFeature::paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *)
{
    painter->setRenderHint(QPainter::Antialiasing);

    switch (m_kind) {
    case FeatureKind::Land: {
        painter->setBrush(QColor(0xa6, 0xbd, 0x7f));
        painter->setPen(QPen(QColor(0x4c, 0x66, 0x3c), 2));
        painter->drawPolygon(m_polygon);
        break;
    }
    case FeatureKind::Country: {
        QColor fill = color.isValid() ? color : QColor(0xd9, 0x6b, 0x5e);
        fill.setAlpha(85);
        painter->setBrush(fill);
        painter->setPen(QPen(color.isValid() ? color.darker(160) : QColor(0x8a, 0x35, 0x2c), 2.5));
        painter->drawPolygon(m_polygon);
        break;
    }
    case FeatureKind::TerrainArea:
    case FeatureKind::Region:
    case FeatureKind::Settlement:
        break;
    }
}
