#pragma once

#include <QGraphicsView>
#include <QPolygonF>
#include <QSizeF>

class QGraphicsScene;
class QKeyEvent;
class QPainter;
class QWheelEvent;
class MapFeature;

/// Full-window map canvas: pan, zoom and (later) feature drawing.
class MapView : public QGraphicsView
{
    Q_OBJECT

public:
    enum class Mode {
        Explore,
        Edit,
    };

    enum class Tool {
        None,
        Land,
        Country,
        TerrainArea,
        Region,
        Settlement,
    };

    explicit MapView(QGraphicsScene *scene, QWidget *parent = nullptr);

    void setMode(Mode mode);
    Mode mode() const { return m_mode; }

    void setTool(Tool tool);
    Tool tool() const { return m_tool; }
    double zoomScale() const { return transform().m11(); }

    void setWorldSize(const QSizeF &size);
    void fitWorld();
    void zoomToRect(const QRectF &rect);
    void zoomBy(double factor);

signals:
    void zoomChanged(double scale);
    void featureClicked(MapFeature *feature);
    void polygonFinished(MapView::Tool tool, const QPolygonF &polygon);
    void pointChosen(MapView::Tool tool, const QPointF &point);

protected:
    void wheelEvent(QWheelEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void drawForeground(QPainter *painter, const QRectF &rect) override;

private:
    void applyZoom(double factor, const QPoint &anchor);
    void updateCursor();
    void emitZoom();
    void finishDraft();
    void cancelDraft();

    Mode m_mode = Mode::Explore;
    Tool m_tool = Tool::None;
    bool m_panning = false;
    QPoint m_lastPan;
    QPolygonF m_draft;
    QPointF m_cursor;
    bool m_drafting = false;
    bool m_clickCandidate = false;
    QPoint m_pressPos;
};
