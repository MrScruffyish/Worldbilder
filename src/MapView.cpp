#include "MapView.h"

#include <QGraphicsScene>
#include <QMouseEvent>
#include <QPainter>
#include <QScrollBar>
#include <QWheelEvent>

#include <algorithm>

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
    setDragMode(mode == Mode::Explore ? QGraphicsView::ScrollHandDrag : QGraphicsView::NoDrag);
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
    QGraphicsView::mouseMoveEvent(event);
}

void MapView::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::MiddleButton && m_panning) {
        m_panning = false;
        updateCursor();
        event->accept();
        return;
    }
    QGraphicsView::mouseReleaseEvent(event);
}
