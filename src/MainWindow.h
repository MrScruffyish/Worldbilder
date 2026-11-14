#pragma once

#include "MapView.h"

#include <QMainWindow>
#include <QSizeF>

class QAction;
class QGraphicsScene;
class QLabel;
class MapFeature;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

private:
    void createScene();
    void createActions();
    void setMode(MapView::Mode mode);
    void chooseNewWorld();
    void newWorld(bool land);
    MapFeature *addBigLand();
    void addLand(const QPolygonF &polygon);

    QGraphicsScene *m_scene;
    MapView *m_view;
    QAction *m_exploreAction = nullptr;
    QAction *m_editAction = nullptr;
    QAction *m_landAction = nullptr;
    QLabel *m_zoomLabel = nullptr;
    QSizeF m_worldSize{2000, 1500};
};
