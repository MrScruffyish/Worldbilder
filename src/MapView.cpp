#include "MapView.h"

#include "MapFeature.h"

#include <QGraphicsScene>
#include <QFontMetricsF>
#include <QKeyEvent>
#include <QLineF>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QScrollBar>
#include <QWheelEvent>

#include <algorithm>

namespace {

qreal distanceToSegment(const QPointF &point, const QPointF &a, const QPointF &b, QPointF *closest)
{
    const QPointF ab = b - a;
    const qreal lengthSquared = ab.x() * ab.x() + ab.y() * ab.y();
    qreal t = 0.0;
    if (lengthSquared > 0.0) {
        t = ((point.x() - a.x()) * ab.x() + (point.y() - a.y()) * ab.y()) / lengthSquared;
        t = std::clamp(t, 0.0, 1.0);
    }
    const QPointF target = a + ab * t;
    if (closest)
        *closest = target;
    return QLineF(point, target).length();
}

} // namespace

MapView::MapView(QGraphicsScene *scene, QWidget *parent)
    : QGraphicsView(scene, parent)
{
    setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing);
    setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    setResizeAnchor(QGraphicsView::AnchorViewCenter);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setMode(Mode::Explore);
}

void MapView::setMode(Mode mode)
{
    m_mode = mode;
    cancelDraft();
    setDragMode(mode == Mode::Explore ? QGraphicsView::ScrollHandDrag : QGraphicsView::NoDrag);
    updateCursor();
}

void MapView::setTool(Tool tool)
{
    m_tool = tool;
    m_snapValid = false;
    cancelDraft();
    updateCursor();
}

void MapView::setWorldSize(const QSizeF &size)
{
    scene()->setSceneRect(QRectF(QPointF(0, 0), size));
}

void MapView::fitWorld()
{
    fitInView(scene()->sceneRect(), Qt::KeepAspectRatio);
    emitZoom();
}

void MapView::zoomToRect(const QRectF &rect)
{
    if (!rect.isValid() || rect.isEmpty())
        return;

    fitInView(rect.adjusted(-24, -24, 24, 24), Qt::KeepAspectRatio);
    if (transform().m11() > 3.0) {
        const double factor = 3.0 / transform().m11();
        scale(factor, factor);
        centerOn(rect.center());
    }
    emitZoom();
}

void MapView::zoomBy(double factor)
{
    applyZoom(factor, QPoint(viewport()->width() / 2, viewport()->height() / 2));
}

void MapView::updateCursor()
{
    if (m_mode == Mode::Explore)
        viewport()->setCursor(Qt::OpenHandCursor);
    else
        viewport()->setCursor(Qt::CrossCursor);
}

void MapView::emitZoom()
{
    emit zoomChanged(transform().m11());
}

void MapView::applyZoom(double factor, const QPoint &anchor)
{
    const double oldScale = transform().m11();
    const double newScale = std::clamp(oldScale * factor, 0.05, 20.0);
    const double realFactor = newScale / oldScale;
    if (qFuzzyCompare(realFactor, 1.0))
        return;

    setTransformationAnchor(QGraphicsView::NoAnchor);
    const QPointF before = mapToScene(anchor);
    scale(realFactor, realFactor);
    const QPointF after = mapToScene(anchor);
    translate(after.x() - before.x(), after.y() - before.y());
    setTransformationAnchor(QGraphicsView::AnchorUnderMouse);

    emitZoom();
}

void MapView::wheelEvent(QWheelEvent *event)
{
    const double steps = event->angleDelta().y() / 120.0;
    if (qFuzzyIsNull(steps))
        return;
    applyZoom(std::pow(1.18, steps), event->position().toPoint());
    event->accept();
}

void MapView::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::MiddleButton) {
        m_panning = true;
        m_lastPan = event->pos();
        viewport()->setCursor(Qt::ClosedHandCursor);
        event->accept();
        return;
    }

    if (m_mode == Mode::Explore && event->button() == Qt::LeftButton) {
        m_clickCandidate = true;
        m_pressPos = event->pos();
    }

    if (m_mode == Mode::Edit && m_tool != Tool::None) {
        const QPointF scenePos = mapToScene(event->pos());
        updateSnap(scenePos);
        if (event->button() == Qt::LeftButton) {
            const QPointF point = m_snapValid ? m_snapPoint : scenePos;
            if (m_tool == Tool::Settlement) {
                emit pointChosen(m_tool, point);
            } else {
                m_draft.append(point);
                m_cursor = point;
                m_drafting = true;
                viewport()->update();
            }
            event->accept();
            return;
        }
        if (event->button() == Qt::RightButton && m_drafting) {
            finishDraft();
            event->accept();
            return;
        }
    }

    QGraphicsView::mousePressEvent(event);
}

void MapView::mouseMoveEvent(QMouseEvent *event)
{
    if (m_panning) {
        const QPoint delta = event->pos() - m_lastPan;
        m_lastPan = event->pos();
        horizontalScrollBar()->setValue(horizontalScrollBar()->value() - delta.x());
        verticalScrollBar()->setValue(verticalScrollBar()->value() - delta.y());
        event->accept();
        return;
    }

    if (m_mode == Mode::Edit && m_tool != Tool::None) {
        m_cursor = mapToScene(event->pos());
        updateSnap(m_cursor);
        viewport()->update();
    }

    QGraphicsView::mouseMoveEvent(event);
}

void MapView::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (m_mode == Mode::Edit && event->button() == Qt::LeftButton && m_drafting) {
        if (m_draft.size() >= 2
            && QLineF(m_draft.at(m_draft.size() - 2), m_draft.last()).length() < 0.01) {
            m_draft.removeLast();
        }
        finishDraft();
        event->accept();
        return;
    }
    QGraphicsView::mouseDoubleClickEvent(event);
}

void MapView::keyPressEvent(QKeyEvent *event)
{
    if (m_drafting) {
        if (event->key() == Qt::Key_Escape) {
            cancelDraft();
            event->accept();
            return;
        }
        if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
            finishDraft();
            event->accept();
            return;
        }
    }
    QGraphicsView::keyPressEvent(event);
}

void MapView::drawForeground(QPainter *painter, const QRectF &rect)
{
    QGraphicsView::drawForeground(painter, rect);

    const double scale = painter->worldTransform().m11();
    if (scale <= 0.0)
        return;

    if (!m_drafting && !m_snapValid)
        return;

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);

    if (m_drafting && !m_draft.isEmpty()) {
        QPen outline(QColor(0x20, 0x30, 0x3c), 0);
        outline.setStyle(Qt::DashLine);
        painter->setPen(outline);
        painter->setBrush(QColor(255, 255, 255, 45));

        if (m_draft.size() >= 3)
            painter->drawPolygon(m_draft);
        else
            painter->drawPolyline(m_draft);

        painter->setBrush(Qt::NoBrush);
        painter->drawLine(m_draft.last(), m_snapValid ? m_snapPoint : m_cursor);

        const double radius = 4.0 / scale;
        painter->setPen(QPen(QColor(0x20, 0x30, 0x3c), 0));
        painter->setBrush(Qt::white);
        for (const QPointF &point : m_draft)
            painter->drawEllipse(point, radius, radius);
    }

    if (m_snapValid) {
        const double ring = 7.0 / scale;
        painter->setPen(QPen(QColor(0xff, 0x8f, 0x1c), 2.0 / scale));
        painter->setBrush(QColor(0xff, 0x8f, 0x1c, 60));
        painter->drawEllipse(m_snapPoint, ring, ring);

        QFont font = painter->font();
        font.setPixelSize(qMax(1, qRound(11.0 / scale)));
        painter->setFont(font);

        const QString text = tr("click to snap: %1").arg(m_snapLabel);
        const QFontMetricsF metrics(font);
        const QRectF textRect(0, 0, metrics.horizontalAdvance(text) + 10.0 / scale,
                              metrics.height() + 4.0 / scale);
        const QPointF origin = m_snapPoint + QPointF(10.0 / scale, -10.0 / scale - textRect.height());

        painter->setPen(Qt::NoPen);
        painter->setBrush(QColor(255, 255, 255, 220));
        painter->drawRoundedRect(textRect.translated(origin), 3.0 / scale, 3.0 / scale);
        painter->setPen(QColor(0x32, 0x2d, 0x26));
        painter->drawText(textRect.translated(origin), Qt::AlignCenter, text);
    }

    painter->restore();
}

void MapView::updateSnap(const QPointF &scenePos)
{
    m_snapValid = false;
    m_snapLabel.clear();

    if (m_mode != Mode::Edit || m_tool == Tool::None)
        return;

    const double threshold = 10.0 / qMax(zoomScale(), 0.001);
    bool hasVertex = false;
    bool hasEdge = false;
    double bestVertex = threshold;
    double bestEdge = threshold;
    QPointF vertexPoint;
    QPointF edgePoint;
    QString vertexLabel;
    QString edgeLabel;

    for (QGraphicsItem *item : scene()->items()) {
        auto *feature = dynamic_cast<MapFeature *>(item);
        if (!feature || !feature->isVisible())
            continue;

        if (feature->kind() == FeatureKind::Settlement) {
            const QPointF point = feature->point();
            const double distance = QLineF(scenePos, point).length();
            if (distance <= bestVertex) {
                bestVertex = distance;
                vertexPoint = point;
                vertexLabel = feature->name.isEmpty() ? feature->kindName() : feature->name;
                hasVertex = true;
            }
            continue;
        }

        const QPolygonF polygon = feature->polygon();
        if (polygon.size() < 2)
            continue;

        const QString label = feature->kindName();
        for (int i = 0; i < polygon.size(); ++i) {
            const QPointF &a = polygon.at(i);
            const QPointF &b = polygon.at((i + 1) % polygon.size());

            const double vertexDistance = QLineF(scenePos, a).length();
            if (vertexDistance <= bestVertex) {
                bestVertex = vertexDistance;
                vertexPoint = a;
                vertexLabel = tr("%1 corner").arg(label);
                hasVertex = true;
            }

            QPointF closest;
            const double edgeDistance = distanceToSegment(scenePos, a, b, &closest);
            if (edgeDistance <= bestEdge) {
                bestEdge = edgeDistance;
                edgePoint = closest;
                edgeLabel = tr("%1 border").arg(label);
                hasEdge = true;
            }
        }
    }

    if (hasVertex) {
        m_snapPoint = vertexPoint;
        m_snapLabel = vertexLabel;
        m_snapValid = true;
    } else if (hasEdge) {
        m_snapPoint = edgePoint;
        m_snapLabel = edgeLabel;
        m_snapValid = true;
    }
}

void MapView::finishDraft()
{
    if (m_draft.size() < 3) {
        cancelDraft();
        return;
    }
    const QPolygonF polygon = m_draft;
    m_draft.clear();
    m_drafting = false;
    viewport()->update();
    emit polygonFinished(m_tool, polygon);
}

void MapView::cancelDraft()
{
    if (m_draft.isEmpty() && !m_drafting)
        return;
    m_draft.clear();
    m_drafting = false;
    viewport()->update();
}

void MapView::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::MiddleButton && m_panning) {
        m_panning = false;
        updateCursor();
        event->accept();
        return;
    }

    if (m_mode == Mode::Explore && event->button() == Qt::LeftButton && m_clickCandidate) {
        m_clickCandidate = false;
        if ((event->pos() - m_pressPos).manhattanLength() <= 4) {
            MapFeature *feature = nullptr;
            const QList<QGraphicsItem *> items = scene()->items(mapToScene(event->pos()));
            for (QGraphicsItem *item : items) {
                if ((feature = dynamic_cast<MapFeature *>(item)))
                    break;
            }
            emit featureClicked(feature);
        }
    }

    QGraphicsView::mouseReleaseEvent(event);
}
