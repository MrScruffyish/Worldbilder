#pragma once

#include "MapFeature.h"
#include "MapView.h"

#include <QJsonObject>
#include <QList>
#include <QMainWindow>
#include <QSizeF>

class QAction;
class QComboBox;
class QDockWidget;
class QGraphicsScene;
class QLabel;
class QMenu;
class QPushButton;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

private:
    void createScene();
    void createActions();
    void createInfoDock();
    void setMode(MapView::Mode mode);
    void chooseNewWorld();
    void newWorld(bool land);
    MapFeature *addBigLand();
    void addLand(const QPolygonF &polygon);
    void addCountry(const QPolygonF &polygon);
    void addTerrainArea(const QPolygonF &polygon);
    void addRegion(const QPolygonF &polygon);
    void addSettlement(const QPointF &point);
    MapFeature *findCountryAt(const QPointF &point) const;
    QPolygonF clipToLand(const QPolygonF &polygon) const;
    TerrainType currentTerrain() const;
    int featureCount(FeatureKind kind) const;
    QColor nextCountryColor() const;
    void selectFeature(MapFeature *feature);
    void onFeatureClicked(MapFeature *feature);
    void updateVisibility(double scale);
    void editSelectedFeature();
    void saveWorld();
    void saveWorldAs();
    bool writeWorld(const QString &path);
    void loadWorld();
    bool readWorld(const QString &path);
    void resizeWorld();
    void updateTitle();
    QJsonObject worldToJson() const;
    void restoreFromJson(const QJsonObject &root);
    void resetHistory();
    void pushHistory();
    void undo();
    void redo();
    void updateHistoryActions();

    QGraphicsScene *m_scene;
    MapView *m_view;
    QAction *m_exploreAction = nullptr;
    QAction *m_editAction = nullptr;
    QAction *m_landAction = nullptr;
    QAction *m_countryAction = nullptr;
    QAction *m_terrainAction = nullptr;
    QAction *m_regionAction = nullptr;
    QAction *m_settlementAction = nullptr;
    QAction *m_undoAction = nullptr;
    QAction *m_redoAction = nullptr;
    QComboBox *m_terrainCombo = nullptr;
    QLabel *m_zoomLabel = nullptr;
    QDockWidget *m_infoDock = nullptr;
    QLabel *m_infoTitle = nullptr;
    QLabel *m_infoDetails = nullptr;
    QLabel *m_infoDescription = nullptr;
    QPushButton *m_editButton = nullptr;
    QMenu *m_viewMenu = nullptr;
    MapFeature *m_selected = nullptr;
    QString m_currentFile;
    QList<QJsonObject> m_history;
    int m_historyIndex = -1;
    bool m_restoring = false;
    QSizeF m_worldSize{2000, 1500};
};
