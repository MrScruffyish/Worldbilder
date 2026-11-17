#pragma once

#include <QGraphicsView>
#include <QPolygonF>
#include <QSizeF>
#include <QString>
#include <QVector>

class QGraphicsScene;
class QContextMenuEvent;
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
        Road,
        River,
        Railroad,
    };

    explicit MapView(QGraphicsScene *scene, QWidget *parent = nullptr);

    void setMode(Mode mode);
    Mode mode() const { return m_mode; }

    void setTool(Tool tool);
    Tool tool() const { return m_tool; }
    void setEditSelection(MapFeature *feature);
    MapFeature *editSelection() const { return m_editSelection; }
    void beginPointReplacement(MapFeature *feature, int index);
    double zoomScale() const { return transform().m11(); }

    void setWorldSize(const QSizeF &size);
    void fitWorld();
    void zoomToRect(const QRectF &rect);
    void zoomBy(double factor);

signals:
    void zoomChanged(double scale);
    void featureClicked(MapFeature *feature);
    void featureSelected(MapFeature *feature);
    void featureEdited();
    void deleteRequested();
    void polygonFinished(MapView::Tool tool, const QPolygonF &polygon);
    void pointChosen(MapView::Tool tool, const QPointF &point);

protected:
    void wheelEvent(QWheelEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void drawForeground(QPainter *painter, const QRectF &rect) override;

private:
    void applyZoom(double factor, const QPoint &anchor);
    void updateCursor();
    void emitZoom();
    void finishDraft();
    void cancelDraft();
    void cancelPointReplacement();
    void updateSnap(const QPointF &scenePos, const MapFeature *exclude = nullptr);
    MapFeature *featureAt(const QPointF &scenePos) const;
    int controlPointAt(const MapFeature *feature, const QPointF &scenePos,
                       double threshold) const;
    int segmentAt(const MapFeature *feature, const QPointF &scenePos, double threshold,
                  QPointF *closest) const;
    int edgeContaining(const MapFeature *feature, const QPointF &scenePos,
                       double threshold) const;
    void moveCoincidentPoints(const QPointF &from, const QPointF &to);
    bool keepFeaturesInsideLand();
    void insertControlPoint(MapFeature *feature, int segmentIndex, const QPointF &point);
    bool isLineTool() const
    {
        return m_tool == Tool::Road || m_tool == Tool::River || m_tool == Tool::Railroad;
    }
    int minimumDraftPoints() const { return isLineTool() ? 2 : 3; }

    Mode m_mode = Mode::Explore;
    Tool m_tool = Tool::None;
    bool m_panning = false;
    QPoint m_lastPan;
    QPolygonF m_draft;
    QPointF m_cursor;
    bool m_drafting = false;
    bool m_clickCandidate = false;
    QPoint m_pressPos;
    bool m_snapValid = false;
    QPointF m_snapPoint;
    QString m_snapLabel;
    MapFeature *m_editSelection = nullptr;
    bool m_draggingSelection = false;
    int m_dragIndex = -1;
    QPointF m_dragStartScene;
    QPolygonF m_dragOriginalPolygon;
    QPointF m_dragOriginalPoint;
    QPointF m_dragVertex;
    struct SharedPolygon {
        MapFeature *feature = nullptr;
        QPolygonF polygon;
    };
    struct SharedEdge {
        MapFeature *feature = nullptr;
        QPolygonF polygon;
        int segment = -1;
    };
    struct Backup {
        MapFeature *feature = nullptr;
        QPolygonF polygon;
        QPointF point;
    };
    QVector<SharedPolygon> m_dragSharedPolygons;
    QVector<SharedEdge> m_dragSharedEdges;
    QVector<Backup> m_dragClampedFeatures;
    bool m_geometryChanged = false;
    bool m_replacingPoint = false;
    int m_replaceIndex = -1;
    bool m_insertPreviewValid = false;
    QPointF m_insertPreviewPoint;
    QPointF m_insertPreviewStart;
    QPointF m_insertPreviewEnd;
};
