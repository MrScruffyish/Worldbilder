#include "MapFeature.h"

#include <QPainter>
#include <QPainterPath>
#include <QFontMetricsF>
#include <QGraphicsScene>
#include <QJsonArray>
#include <QJsonObject>
#include <QLineF>
#include <QPainterPathStroker>
#include <QStyleOptionGraphicsItem>

#include <algorithm>
#include <cmath>

namespace {

struct TerrainInfo {
    TerrainType type;
    const char *name;
    QColor color;
};

const TerrainInfo kTerrains[] = {
    {TerrainType::Grass, "Grass", QColor(0x7f, 0xae, 0x5a)},
    {TerrainType::Forest, "Forest", QColor(0x3f, 0x7a, 0x4a)},
    {TerrainType::Desert, "Desert", QColor(0xdf, 0xc9, 0x8a)},
    {TerrainType::Mountains, "Mountains", QColor(0x9a, 0x93, 0x8b)},
    {TerrainType::Water, "Water", QColor(0x4f, 0x83, 0xb8)},
    {TerrainType::Swamp, "Swamp", QColor(0x6c, 0x7f, 0x5a)},
    {TerrainType::Farmland, "Farmland", QColor(0xbc, 0xb4, 0x5f)},
    {TerrainType::Uncharted, "Uncharted", QColor(0x9d, 0x97, 0x8b)},
};

const TerrainInfo &terrainInfoFor(TerrainType terrain)
{
    for (const TerrainInfo &info : kTerrains) {
        if (info.type == terrain)
            return info;
    }
    return kTerrains[0];
}

void drawHaloLabel(QPainter *painter, const QString &text, const QPointF &center,
                   const QFont &font, const QColor &color)
{
    if (text.isEmpty())
        return;

    const QFontMetricsF metrics(font);
    const qreal width = metrics.horizontalAdvance(text);
    const qreal baseline = center.y() - metrics.height() / 2.0 + metrics.ascent();

    QPainterPath path;
    path.addText(QPointF(center.x() - width / 2.0, baseline), font, text);

    painter->save();
    painter->setPen(QPen(Qt::white, 3.5));
    painter->setBrush(Qt::white);
    painter->drawPath(path);
    painter->setPen(QPen(color, 0.8));
    painter->setBrush(color);
    painter->drawPath(path);
    painter->restore();
}

QPainterPath polylinePath(const QPolygonF &polygon)
{
    QPainterPath path;
    if (polygon.isEmpty())
        return path;

    path.moveTo(polygon.first());
    for (int i = 1; i < polygon.size(); ++i)
        path.lineTo(polygon.at(i));
    return path;
}

void drawRoadBridges(QPainter *painter, const QPolygonF &road, const QPolygonF &river)
{
    constexpr double kPi = 3.14159265358979323846;
    for (int i = 0; i + 1 < road.size(); ++i) {
        const QLineF roadLine(road.at(i), road.at(i + 1));
        for (int j = 0; j + 1 < river.size(); ++j) {
            QPointF intersection;
            if (roadLine.intersects(QLineF(river.at(j), river.at(j + 1)), &intersection)
                != QLineF::BoundedIntersection) {
                continue;
            }

            const double angle = std::atan2(roadLine.dy(), roadLine.dx()) * 180.0 / kPi;
            painter->save();
            painter->translate(intersection);
            painter->rotate(angle);
            painter->setPen(QPen(QColor(0x4b, 0x3a, 0x2a), 1.0));
            painter->setBrush(QColor(0xe8, 0xd8, 0xb0));
            painter->drawRect(QRectF(-7, -5, 14, 10));
            painter->restore();
        }
    }
}

} // namespace

QString terrainName(TerrainType terrain)
{
    return QString::fromLatin1(terrainInfoFor(terrain).name);
}

QColor terrainColor(TerrainType terrain)
{
    return terrainInfoFor(terrain).color;
}

double terrainTravelCost(TerrainType terrain)
{
    switch (terrain) {
    case TerrainType::Grass:
        return 1.0;
    case TerrainType::Forest:
        return 1.4;
    case TerrainType::Desert:
        return 1.8;
    case TerrainType::Mountains:
        return 3.5;
    case TerrainType::Water:
        return 5.0;
    case TerrainType::Swamp:
        return 2.5;
    case TerrainType::Farmland:
        return 1.1;
    case TerrainType::Uncharted:
        return 1.6;
    }
    return 1.0;
}

const QVector<TerrainType> &terrainTypes()
{
    static const QVector<TerrainType> types = {
        TerrainType::Grass, TerrainType::Forest, TerrainType::Desert,
        TerrainType::Mountains, TerrainType::Water, TerrainType::Swamp,
        TerrainType::Farmland, TerrainType::Uncharted,
    };
    return types;
}

QString settlementName(SettlementType type)
{
    switch (type) {
    case SettlementType::City:
        return QStringLiteral("City");
    case SettlementType::Town:
        return QStringLiteral("Town");
    case SettlementType::Village:
        return QStringLiteral("Village");
    }
    return QStringLiteral("Settlement");
}

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
    case FeatureKind::River:
        setZValue(4);
        break;
    case FeatureKind::Road:
        setZValue(5);
        break;
    case FeatureKind::Railroad:
        setZValue(5.5);
        break;
    case FeatureKind::Settlement:
        setZValue(6);
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
    case FeatureKind::River:
        return QStringLiteral("River");
    case FeatureKind::Road:
        return QStringLiteral("Road");
    case FeatureKind::Railroad:
        return QStringLiteral("Railroad");
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

void MapFeature::setPoint(const QPointF &point)
{
    prepareGeometryChange();
    m_point = point;
    update();
}

void MapFeature::refresh()
{
    prepareGeometryChange();
    update();
}

namespace {

QString kindKey(FeatureKind kind)
{
    switch (kind) {
    case FeatureKind::Land:
        return QStringLiteral("land");
    case FeatureKind::Country:
        return QStringLiteral("country");
    case FeatureKind::TerrainArea:
        return QStringLiteral("terrain");
    case FeatureKind::Region:
        return QStringLiteral("region");
    case FeatureKind::River:
        return QStringLiteral("river");
    case FeatureKind::Road:
        return QStringLiteral("road");
    case FeatureKind::Railroad:
        return QStringLiteral("railroad");
    case FeatureKind::Settlement:
        return QStringLiteral("settlement");
    }
    return QStringLiteral("land");
}

} // namespace

QJsonObject MapFeature::toJson() const
{
    QJsonObject object;
    object.insert(QStringLiteral("kind"), kindKey(m_kind));
    object.insert(QStringLiteral("name"), name);
    if (!description.isEmpty())
        object.insert(QStringLiteral("description"), description);
    if (!history.isEmpty())
        object.insert(QStringLiteral("history"), history);
    if (!facts.isEmpty())
        object.insert(QStringLiteral("facts"), facts);
    if (!languages.isEmpty())
        object.insert(QStringLiteral("languages"), QJsonArray::fromStringList(languages));
    if (!characteristics.isEmpty())
        object.insert(QStringLiteral("characteristics"),
                      QJsonArray::fromStringList(characteristics));
    if (!countryName.isEmpty())
        object.insert(QStringLiteral("country"), countryName);
    if (color.isValid())
        object.insert(QStringLiteral("color"), color.name(QColor::HexArgb));
    if (m_kind == FeatureKind::TerrainArea)
        object.insert(QStringLiteral("terrain"), static_cast<int>(terrain));
    if (m_kind == FeatureKind::Settlement) {
        object.insert(QStringLiteral("settlement"), static_cast<int>(settlement));
        object.insert(QStringLiteral("point"), QJsonArray{m_point.x(), m_point.y()});
    }

    QJsonArray points;
    for (const QPointF &point : m_polygon) {
        points.append(point.x());
        points.append(point.y());
    }
    object.insert(QStringLiteral("polygon"), points);
    return object;
}

MapFeature *MapFeature::fromJson(const QJsonObject &object)
{
    const QString kindText = object.value(QStringLiteral("kind")).toString();
    FeatureKind kind;
    if (kindText == QLatin1String("land"))
        kind = FeatureKind::Land;
    else if (kindText == QLatin1String("country"))
        kind = FeatureKind::Country;
    else if (kindText == QLatin1String("terrain"))
        kind = FeatureKind::TerrainArea;
    else if (kindText == QLatin1String("region"))
        kind = FeatureKind::Region;
    else if (kindText == QLatin1String("river"))
        kind = FeatureKind::River;
    else if (kindText == QLatin1String("road"))
        kind = FeatureKind::Road;
    else if (kindText == QLatin1String("railroad"))
        kind = FeatureKind::Railroad;
    else if (kindText == QLatin1String("settlement"))
        kind = FeatureKind::Settlement;
    else
        return nullptr;

    auto *feature = new MapFeature(kind);
    feature->name = object.value(QStringLiteral("name")).toString();
    feature->description = object.value(QStringLiteral("description")).toString();
    feature->history = object.value(QStringLiteral("history")).toString();
    feature->facts = object.value(QStringLiteral("facts")).toString();
    for (const QJsonValue &value : object.value(QStringLiteral("languages")).toArray()) {
        const QString language = value.toString();
        if (!language.isEmpty())
            feature->languages.append(language);
    }
    for (const QJsonValue &value : object.value(QStringLiteral("characteristics")).toArray()) {
        const QString characteristic = value.toString();
        if (!characteristic.isEmpty())
            feature->characteristics.append(characteristic);
    }
    feature->countryName = object.value(QStringLiteral("country")).toString();

    const QString colorText = object.value(QStringLiteral("color")).toString();
    if (!colorText.isEmpty())
        feature->color = QColor(colorText);

    const int terrain = object.value(QStringLiteral("terrain")).toInt();
    if (terrain >= static_cast<int>(TerrainType::Grass)
        && terrain <= static_cast<int>(TerrainType::Uncharted)) {
        feature->terrain = static_cast<TerrainType>(terrain);
    }

    const int settlement = object.value(QStringLiteral("settlement")).toInt();
    if (settlement >= static_cast<int>(SettlementType::City)
        && settlement <= static_cast<int>(SettlementType::Village)) {
        feature->settlement = static_cast<SettlementType>(settlement);
    }

    const QJsonArray point = object.value(QStringLiteral("point")).toArray();
    if (point.size() >= 2)
        feature->setPoint(QPointF(point.at(0).toDouble(), point.at(1).toDouble()));

    QPolygonF polygon;
    const QJsonArray coordinates = object.value(QStringLiteral("polygon")).toArray();
    for (int i = 0; i + 1 < coordinates.size(); i += 2)
        polygon.append(QPointF(coordinates.at(i).toDouble(), coordinates.at(i + 1).toDouble()));
    feature->setPolygon(polygon);

    return feature;
}

QPointF MapFeature::centroid() const
{
    if (m_kind == FeatureKind::Settlement)
        return m_point;
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

QFont MapFeature::labelFont() const
{
    QFont font;
    if (m_kind == FeatureKind::Country) {
        font.setPixelSize(18);
        font.setBold(true);
    } else if (m_kind == FeatureKind::Region) {
        font.setPixelSize(12);
        font.setBold(true);
    } else if (m_kind == FeatureKind::Settlement) {
        font.setPixelSize(11);
    }
    return font;
}

QRectF MapFeature::boundingRect() const
{
    if (m_kind == FeatureKind::Settlement) {
        const QFontMetricsF metrics(labelFont());
        const qreal textWidth = metrics.horizontalAdvance(name);
        const qreal width = std::max(28.0, textWidth + 38.0);
        return QRectF(m_point.x() - 14, m_point.y() - 16, width, 34);
    }

    const qreal margin = (m_kind == FeatureKind::Road || m_kind == FeatureKind::River
                          || m_kind == FeatureKind::Railroad)
                             ? 10.0
                             : 4.0;
    QRectF rect = m_polygon.boundingRect().adjusted(-margin, -margin, margin, margin);
    if (!name.isEmpty() && (m_kind == FeatureKind::Country || m_kind == FeatureKind::Region)) {
        const QFontMetricsF metrics(labelFont());
        const qreal width = metrics.horizontalAdvance(name);
        const QPointF center = centroid();
        const QRectF labelRect(center.x() - width / 2.0 - 4, center.y() - metrics.height() / 2.0 - 4,
                               width + 8, metrics.height() + 8);
        rect = rect.united(labelRect);
    }
    return rect;
}

QPainterPath MapFeature::shape() const
{
    QPainterPath path;
    if (m_kind == FeatureKind::Settlement) {
        path.addEllipse(m_point, 12, 12);
        return path;
    }
    if (m_kind == FeatureKind::Road || m_kind == FeatureKind::River
        || m_kind == FeatureKind::Railroad) {
        QPainterPathStroker stroker;
        stroker.setWidth(10);
        return stroker.createStroke(polylinePath(m_polygon));
    }
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
        drawHaloLabel(painter, name, centroid(), labelFont(), QColor(0x2b, 0x26, 0x1e));
        break;
    }
    case FeatureKind::TerrainArea: {
        QColor fill = terrainColor(terrain);
        fill.setAlpha(210);
        painter->setBrush(fill);
        painter->setPen(Qt::NoPen);
        painter->drawPolygon(m_polygon);
        break;
    }
    case FeatureKind::Region: {
        QColor border = color.isValid() ? color.darker(180) : QColor(0x55, 0x4a, 0x3a);
        QColor fill = color.isValid() ? color : QColor(0xaa, 0xaa, 0xaa);
        fill.setAlpha(30);
        painter->setBrush(fill);
        QPen pen(border, 1.6);
        pen.setStyle(Qt::DashLine);
        painter->setPen(pen);
        painter->drawPolygon(m_polygon);
        if (painter->worldTransform().m11() >= 0.45)
            drawHaloLabel(painter, name, centroid(), labelFont(), QColor(0x3a, 0x33, 0x28));

        painter->setPen(QPen(Qt::white, 1.5));
        painter->setBrush(QColor(0x3a, 0x2f, 0x24));
        for (const QPointF &point : m_polygon)
            painter->drawEllipse(point, 3.5, 3.5);
        break;
    }
    case FeatureKind::River: {
        QPen pen(QColor(0x46, 0x7e, 0xbf), 3.0);
        pen.setCapStyle(Qt::RoundCap);
        pen.setJoinStyle(Qt::RoundJoin);
        painter->setPen(pen);
        painter->setBrush(Qt::NoBrush);
        painter->drawPath(polylinePath(m_polygon));
        break;
    }
    case FeatureKind::Road: {
        QPen casing(QColor(0x6b, 0x54, 0x3b), 6.0);
        casing.setCapStyle(Qt::RoundCap);
        casing.setJoinStyle(Qt::RoundJoin);
        painter->setPen(casing);
        painter->setBrush(Qt::NoBrush);
        painter->drawPath(polylinePath(m_polygon));

        QPen inner(QColor(0xd9, 0xc0, 0x8f), 3.0);
        inner.setCapStyle(Qt::RoundCap);
        inner.setJoinStyle(Qt::RoundJoin);
        painter->setPen(inner);
        painter->drawPath(polylinePath(m_polygon));

        if (scene()) {
            for (QGraphicsItem *item : scene()->items()) {
                auto *river = dynamic_cast<MapFeature *>(item);
                if (river && river->kind() == FeatureKind::River)
                    drawRoadBridges(painter, m_polygon, river->polygon());
            }
        }
        break;
    }
    case FeatureKind::Railroad: {
        QPen casing(QColor(0x3f, 0x3f, 0x3f), 6.0);
        casing.setCapStyle(Qt::RoundCap);
        casing.setJoinStyle(Qt::RoundJoin);
        painter->setPen(casing);
        painter->setBrush(Qt::NoBrush);
        painter->drawPath(polylinePath(m_polygon));

        QPen ties(QColor(0xe6, 0xe6, 0xe6), 2.0);
        ties.setCapStyle(Qt::FlatCap);
        ties.setJoinStyle(Qt::RoundJoin);
        ties.setDashPattern({1.0, 2.0});
        painter->setPen(ties);
        painter->drawPath(polylinePath(m_polygon));

        if (scene()) {
            for (QGraphicsItem *item : scene()->items()) {
                auto *river = dynamic_cast<MapFeature *>(item);
                if (river && river->kind() == FeatureKind::River)
                    drawRoadBridges(painter, m_polygon, river->polygon());
            }
        }
        break;
    }
    case FeatureKind::Settlement: {
        painter->save();
        painter->translate(m_point);

        QColor fill(0x6a, 0x8f, 0x4a);
        qreal radius = 5.0;
        if (settlement == SettlementType::City) {
            fill = QColor(0xc9, 0x4a, 0x3f);
            radius = 9.0;
        } else if (settlement == SettlementType::Town) {
            fill = QColor(0xd9, 0x8e, 0x2b);
            radius = 7.0;
        }

        painter->setPen(QPen(Qt::white, 2.2));
        painter->setBrush(fill);
        if (settlement == SettlementType::Town) {
            painter->drawRect(QRectF(-radius, -radius, radius * 2, radius * 2));
        } else {
            painter->drawEllipse(QPointF(0, 0), radius, radius);
        }
        painter->restore();

        if (!name.isEmpty() && painter->worldTransform().m11() >= 0.3) {
            const QFont font = labelFont();
            const QFontMetricsF metrics(font);
            const qreal width = metrics.horizontalAdvance(name);
            const QRectF textRect(m_point.x() + 13, m_point.y() - metrics.height() / 2.0 - 2,
                                  width + 8, metrics.height() + 4);
            painter->setPen(Qt::NoPen);
            painter->setBrush(QColor(255, 255, 255, 215));
            painter->drawRoundedRect(textRect, 3, 3);
            painter->setFont(font);
            painter->setPen(QColor(0x32, 0x2d, 0x26));
            painter->drawText(textRect, Qt::AlignCenter, name);
        }
        break;
    }
    }

    if (highlighted) {
        painter->setBrush(Qt::NoBrush);
        painter->setPen(QPen(QColor(0xff, 0x9f, 0x1c), 4.0));
        if (m_kind == FeatureKind::Settlement)
            painter->drawEllipse(m_point, 13, 13);
        else if (m_kind == FeatureKind::Road || m_kind == FeatureKind::River
                 || m_kind == FeatureKind::Railroad)
            painter->drawPath(polylinePath(m_polygon));
        else
            painter->drawPolygon(m_polygon);
    }
}
