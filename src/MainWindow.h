#pragma once

#include "MapFeature.h"
#include "MapView.h"

#include <QMainWindow>
#include <QSizeF>

class QAction;
class QComboBox;
class QGraphicsScene;
class QLabel;

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
    void addCountry(const QPolygonF &polygon);
    void addTerrainArea(const QPolygonF &polygon);
    TerrainType currentTerrain() const;
    int featureCount(FeatureKind kind) const;
    QColor nextCountryColor() const;

    QGraphicsScene *m_scene;
    MapView *m_view;
    QAction *m_exploreAction = nullptr;
    QAction *m_editAction = nullptr;
    QAction *m_landAction = nullptr;
    QAction *m_countryAction = nullptr;
    QAction *m_terrainAction = nullptr;
    QComboBox *m_terrainCombo = nullptr;
    QLabel *m_zoomLabel = nullptr;
    QSizeF m_worldSize{2000, 1500};
};
