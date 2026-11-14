#pragma once

#include <QGraphicsView>
#include <QSizeF>

class QGraphicsScene;

/// Full-window map canvas: pan, zoom and (later) feature drawing.
class MapView : public QGraphicsView
{
    Q_OBJECT

public:
    enum class Mode {
        Explore,
        Edit,
    };

    explicit MapView(QGraphicsScene *scene, QWidget *parent = nullptr);

    void setMode(Mode mode);
    Mode mode() const { return m_mode; }

    void setWorldSize(const QSizeF &size);
    void fitWorld();

signals:
    void zoomChanged(double scale);

protected:
    void wheelEvent(QWheelEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    void applyZoom(double factor, const QPoint &anchor);
    void updateCursor();
    void emitZoom();

    Mode m_mode = Mode::Explore;
    bool m_panning = false;
    QPoint m_lastPan;
};
