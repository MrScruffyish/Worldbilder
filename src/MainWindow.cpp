#include "MainWindow.h"

#include "MapFeature.h"

#include <QAction>
#include <QActionGroup>
#include <QComboBox>
#include <QDockWidget>
#include <QFont>
#include <QGraphicsScene>
#include <QIcon>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPainterPath>
#include <QPixmap>
#include <QStatusBar>
#include <QTimer>
#include <QToolBar>
#include <QVBoxLayout>
#include <QWidget>

#include <QPushButton>

namespace {
const QColor kSeaColor(0x3d, 0x6a, 0x93);

const QColor kCountryColors[] = {
    QColor(0xd9, 0x6b, 0x5e), QColor(0x4f, 0x9d, 0xd1), QColor(0xe0, 0xb4, 0x4f),
    QColor(0x8a, 0x6f, 0xc0), QColor(0x5a, 0xa8, 0x86), QColor(0xd1, 0x7f, 0xb0),
    QColor(0xc2, 0x7c, 0x42), QColor(0x6f, 0x93, 0x4f),
};
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle(tr("Worldbilder"));
    resize(1200, 800);

    createScene();
    createActions();

    m_view = new MapView(m_scene, this);
    m_view->setWorldSize(m_worldSize);
    setCentralWidget(m_view);

    m_zoomLabel = new QLabel(tr("100%"), this);
    statusBar()->addPermanentWidget(m_zoomLabel);
    statusBar()->showMessage(tr("Explore: drag to pan, wheel to zoom."));

    createInfoDock();

    connect(m_view, &MapView::zoomChanged, this, [this](double scale) {
        m_zoomLabel->setText(QStringLiteral("%1%").arg(qRound(scale * 100)));
    });
    connect(m_view, &MapView::zoomChanged, this, &MainWindow::updateVisibility);
    connect(m_view, &MapView::featureClicked, this, &MainWindow::onFeatureClicked);

    connect(m_view, &MapView::polygonFinished, this, [this](MapView::Tool tool, const QPolygonF &polygon) {
        switch (tool) {
        case MapView::Tool::Land:
            addLand(polygon);
            break;
        case MapView::Tool::Country:
            addCountry(polygon);
            break;
        case MapView::Tool::TerrainArea:
            addTerrainArea(polygon);
            break;
        case MapView::Tool::Region:
            addRegion(polygon);
            break;
        default:
            break;
        }
    });

    connect(m_view, &MapView::pointChosen, this, [this](MapView::Tool tool, const QPointF &point) {
        if (tool == MapView::Tool::Settlement)
            addSettlement(point);
    });

    newWorld(true);
    QTimer::singleShot(0, m_view, &MapView::fitWorld);
}

void MainWindow::createScene()
{
    m_scene = new QGraphicsScene(this);
    m_scene->setBackgroundBrush(kSeaColor);
    m_scene->setSceneRect(QRectF(QPointF(0, 0), m_worldSize));
}

void MainWindow::createInfoDock()
{
    m_infoDock = new QDockWidget(tr("Info"), this);
    m_infoDock->setObjectName(QStringLiteral("infoDock"));
    m_infoDock->setMinimumWidth(250);

    auto *widget = new QWidget(m_infoDock);
    auto *layout = new QVBoxLayout(widget);
    layout->setContentsMargins(10, 10, 10, 10);

    m_infoTitle = new QLabel(tr("Nothing selected"), widget);
    QFont titleFont = m_infoTitle->font();
    titleFont.setBold(true);
    titleFont.setPointSize(titleFont.pointSize() + 3);
    m_infoTitle->setFont(titleFont);
    m_infoTitle->setWordWrap(true);

    m_infoDetails = new QLabel(tr("Click a country, region or settlement in Explore mode."), widget);
    m_infoDetails->setWordWrap(true);

    m_infoDescription = new QLabel(widget);
    m_infoDescription->setWordWrap(true);
    m_infoDescription->setAlignment(Qt::AlignTop | Qt::AlignLeft);

    layout->addWidget(m_infoTitle);
    layout->addWidget(m_infoDetails);
    layout->addSpacing(8);
    layout->addWidget(m_infoDescription);
    layout->addStretch();

    m_infoDock->setWidget(widget);
    addDockWidget(Qt::RightDockWidgetArea, m_infoDock);

    if (m_viewMenu) {
        m_viewMenu->addSeparator();
        m_viewMenu->addAction(m_infoDock->toggleViewAction());
    }
}

void MainWindow::selectFeature(MapFeature *feature)
{
    if (m_selected != feature) {
        if (m_selected) {
            m_selected->highlighted = false;
            m_selected->update();
        }
        m_selected = feature;
        if (m_selected) {
            m_selected->highlighted = true;
            m_selected->update();
        }
    }

    if (!feature) {
        m_infoTitle->setText(tr("Nothing selected"));
        m_infoDetails->setText(tr("Click a country, region or settlement in Explore mode."));
        m_infoDescription->clear();
        return;
    }

    QString title = feature->name;
    if (title.isEmpty()) {
        title = feature->kind() == FeatureKind::TerrainArea
                    ? tr("%1 area").arg(terrainName(feature->terrain))
                    : feature->kindName();
    }
    m_infoTitle->setText(title);

    switch (feature->kind()) {
    case FeatureKind::Country:
        m_infoDetails->setText(tr("Country"));
        break;
    case FeatureKind::Region:
        m_infoDetails->setText(tr("Region in %1").arg(feature->countryName));
        break;
    case FeatureKind::Settlement:
        m_infoDetails->setText(settlementName(feature->settlement));
        break;
    case FeatureKind::TerrainArea:
        m_infoDetails->setText(tr("Terrain: %1").arg(terrainName(feature->terrain)));
        break;
    case FeatureKind::Land:
        m_infoDetails->setText(tr("Land"));
        break;
    }

    m_infoDescription->setText(feature->description.isEmpty() ? tr("No description.")
                                                              : feature->description);
}

void MainWindow::onFeatureClicked(MapFeature *feature)
{
    selectFeature(feature);
    if (!feature)
        return;

    switch (feature->kind()) {
    case FeatureKind::Country:
    case FeatureKind::Region:
        m_view->zoomToRect(feature->polygon().boundingRect());
        break;
    case FeatureKind::Settlement: {
        const QPointF point = feature->point();
        m_view->zoomToRect(QRectF(point.x() - 180, point.y() - 140, 360, 280));
        break;
    }
    default:
        break;
    }
}

void MainWindow::updateVisibility(double scale)
{
    const bool showRegions = m_view->mode() == MapView::Mode::Edit || scale >= 0.5;
    for (QGraphicsItem *item : m_scene->items()) {
        auto *feature = dynamic_cast<MapFeature *>(item);
        if (feature && feature->kind() == FeatureKind::Region)
            feature->setVisible(showRegions);
    }
}

void MainWindow::createActions()
{
    auto *modeGroup = new QActionGroup(this);
    modeGroup->setExclusive(true);

    m_exploreAction = new QAction(tr("Explore"), this);
    m_exploreAction->setCheckable(true);
    m_exploreAction->setChecked(true);
    m_exploreAction->setShortcut(Qt::Key_F1);
    modeGroup->addAction(m_exploreAction);

    m_editAction = new QAction(tr("Edit"), this);
    m_editAction->setCheckable(true);
    m_editAction->setShortcut(Qt::Key_F2);
    modeGroup->addAction(m_editAction);

    connect(m_exploreAction, &QAction::triggered, this, [this] { setMode(MapView::Mode::Explore); });
    connect(m_editAction, &QAction::triggered, this, [this] { setMode(MapView::Mode::Edit); });

    m_landAction = new QAction(tr("Draw Land"), this);
    m_landAction->setCheckable(true);
    m_landAction->setShortcut(Qt::Key_L);
    connect(m_landAction, &QAction::triggered, this, [this] {
        setMode(MapView::Mode::Edit);
        m_view->setTool(MapView::Tool::Land);
        statusBar()->showMessage(tr("Draw land: click to add points, double-click or Enter to finish, Esc to cancel."));
    });

    m_countryAction = new QAction(tr("Country"), this);
    m_countryAction->setCheckable(true);
    m_countryAction->setShortcut(Qt::Key_C);
    connect(m_countryAction, &QAction::triggered, this, [this] {
        setMode(MapView::Mode::Edit);
        m_view->setTool(MapView::Tool::Country);
        statusBar()->showMessage(tr("Draw a country: click to add points, double-click or Enter to finish."));
    });

    auto *toolGroup = new QActionGroup(this);
    toolGroup->setExclusive(true);
    toolGroup->addAction(m_landAction);
    toolGroup->addAction(m_countryAction);

    m_terrainAction = new QAction(tr("Terrain Area"), this);
    m_terrainAction->setCheckable(true);
    m_terrainAction->setShortcut(Qt::Key_T);
    connect(m_terrainAction, &QAction::triggered, this, [this] {
        setMode(MapView::Mode::Edit);
        m_view->setTool(MapView::Tool::TerrainArea);
        statusBar()->showMessage(tr("Draw a terrain area: pick a terrain, then click points and finish."));
    });
    toolGroup->addAction(m_terrainAction);

    m_regionAction = new QAction(tr("Region"), this);
    m_regionAction->setCheckable(true);
    m_regionAction->setShortcut(Qt::Key_R);
    connect(m_regionAction, &QAction::triggered, this, [this] {
        setMode(MapView::Mode::Edit);
        m_view->setTool(MapView::Tool::Region);
        statusBar()->showMessage(tr("Draw a region inside a country."));
    });
    toolGroup->addAction(m_regionAction);

    m_settlementAction = new QAction(tr("Settlement"), this);
    m_settlementAction->setCheckable(true);
    m_settlementAction->setShortcut(Qt::Key_S);
    connect(m_settlementAction, &QAction::triggered, this, [this] {
        setMode(MapView::Mode::Edit);
        m_view->setTool(MapView::Tool::Settlement);
        statusBar()->showMessage(tr("Click on the map to place a city, town or village."));
    });
    toolGroup->addAction(m_settlementAction);

    m_terrainCombo = new QComboBox(this);
    m_terrainCombo->setToolTip(tr("Terrain painted by the terrain tool"));
    for (TerrainType terrain : terrainTypes()) {
        QPixmap swatch(14, 14);
        swatch.fill(terrainColor(terrain));
        m_terrainCombo->addItem(QIcon(swatch), terrainName(terrain), static_cast<int>(terrain));
    }

    auto *fitAction = new QAction(tr("Fit World"), this);
    fitAction->setShortcut(Qt::Key_F);
    connect(fitAction, &QAction::triggered, this, [this] { m_view->fitWorld(); });

    auto *zoomInAction = new QAction(tr("Zoom In"), this);
    zoomInAction->setShortcut(QKeySequence::ZoomIn);
    connect(zoomInAction, &QAction::triggered, this, [this] { m_view->zoomBy(1.25); });

    auto *zoomOutAction = new QAction(tr("Zoom Out"), this);
    zoomOutAction->setShortcut(QKeySequence::ZoomOut);
    connect(zoomOutAction, &QAction::triggered, this, [this] { m_view->zoomBy(0.8); });

    auto *newAction = new QAction(tr("New World..."), this);
    newAction->setShortcut(QKeySequence::New);
    connect(newAction, &QAction::triggered, this, &MainWindow::chooseNewWorld);

    auto *quitAction = new QAction(tr("Quit"), this);
    quitAction->setShortcut(QKeySequence::Quit);
    connect(quitAction, &QAction::triggered, this, &QWidget::close);

    auto *fileMenu = menuBar()->addMenu(tr("&File"));
    fileMenu->addAction(newAction);
    fileMenu->addSeparator();
    fileMenu->addAction(quitAction);

    auto *modeMenu = menuBar()->addMenu(tr("&Mode"));
    modeMenu->addAction(m_exploreAction);
    modeMenu->addAction(m_editAction);

    auto *viewMenu = menuBar()->addMenu(tr("&View"));
    m_viewMenu = viewMenu;
    viewMenu->addAction(zoomInAction);
    viewMenu->addAction(zoomOutAction);
    viewMenu->addAction(fitAction);

    auto *toolbar = addToolBar(tr("Main"));
    toolbar->setMovable(false);
    toolbar->addAction(newAction);
    toolbar->addSeparator();
    toolbar->addAction(m_exploreAction);
    toolbar->addAction(m_editAction);
    toolbar->addSeparator();
    toolbar->addAction(m_landAction);
    toolbar->addAction(m_countryAction);
    toolbar->addAction(m_terrainAction);
    toolbar->addAction(m_regionAction);
    toolbar->addAction(m_settlementAction);
    toolbar->addWidget(m_terrainCombo);
    toolbar->addSeparator();
    toolbar->addAction(zoomOutAction);
    toolbar->addAction(fitAction);
    toolbar->addAction(zoomInAction);
}

void MainWindow::chooseNewWorld()
{
    QMessageBox box(this);
    box.setWindowTitle(tr("New World"));
    box.setText(tr("What should the world start with?"));
    auto *seaButton = box.addButton(tr("Sea"), QMessageBox::AcceptRole);
    auto *landButton = box.addButton(tr("Big land area"), QMessageBox::AcceptRole);
    box.addButton(QMessageBox::Cancel);
    box.exec();

    if (box.clickedButton() == seaButton)
        newWorld(false);
    else if (box.clickedButton() == landButton)
        newWorld(true);
}

void MainWindow::newWorld(bool land)
{
    selectFeature(nullptr);
    m_scene->clear();
    m_scene->setSceneRect(QRectF(QPointF(0, 0), m_worldSize));
    m_view->setWorldSize(m_worldSize);

    if (land)
        addBigLand();

    m_view->fitWorld();
    setWindowTitle(tr("Worldbilder - Untitled"));
}

MapFeature *MainWindow::addBigLand()
{
    const QRectF area(m_worldSize.width() * 0.06, m_worldSize.height() * 0.06,
                      m_worldSize.width() * 0.88, m_worldSize.height() * 0.88);

    QPainterPath path;
    path.addRoundedRect(area, 140, 140);

    auto *land = new MapFeature(FeatureKind::Land);
    land->setPolygon(path.toFillPolygon());
    m_scene->addItem(land);
    return land;
}

void MainWindow::addLand(const QPolygonF &polygon)
{
    auto *land = new MapFeature(FeatureKind::Land);
    land->setPolygon(polygon);
    m_scene->addItem(land);
    statusBar()->showMessage(tr("Land added."), 2000);
}

void MainWindow::addCountry(const QPolygonF &polygon)
{
    bool ok = false;
    const QString name = QInputDialog::getText(this, tr("New Country"), tr("Name:"), QLineEdit::Normal,
                                               tr("Country %1").arg(featureCount(FeatureKind::Country) + 1),
                                               &ok)
                             .trimmed();
    if (!ok || name.isEmpty())
        return;

    auto *country = new MapFeature(FeatureKind::Country);
    country->setPolygon(polygon);
    country->name = name;
    country->color = nextCountryColor();
    m_scene->addItem(country);
    statusBar()->showMessage(tr("%1 created.").arg(name), 2000);
}

void MainWindow::addTerrainArea(const QPolygonF &polygon)
{
    auto *area = new MapFeature(FeatureKind::TerrainArea);
    area->setPolygon(polygon);
    area->terrain = currentTerrain();
    m_scene->addItem(area);
    statusBar()->showMessage(tr("%1 area placed.").arg(terrainName(area->terrain)), 2000);
}

TerrainType MainWindow::currentTerrain() const
{
    return static_cast<TerrainType>(m_terrainCombo->currentData().toInt());
}

void MainWindow::addRegion(const QPolygonF &polygon)
{
    MapFeature probe(FeatureKind::Region);
    probe.setPolygon(polygon);
    MapFeature *country = findCountryAt(probe.centroid());
    if (!country) {
        QMessageBox::information(this, tr("Region"), tr("Draw the region inside a country."));
        return;
    }

    bool ok = false;
    const QString name = QInputDialog::getText(this, tr("New Region"), tr("Name:"), QLineEdit::Normal,
                                               tr("Region %1").arg(featureCount(FeatureKind::Region) + 1),
                                               &ok)
                             .trimmed();
    if (!ok || name.isEmpty())
        return;

    auto *region = new MapFeature(FeatureKind::Region);
    region->setPolygon(polygon);
    region->name = name;
    region->countryName = country->name;
    region->color = country->color;
    m_scene->addItem(region);
    statusBar()->showMessage(tr("%1 added to %2.").arg(name, country->name), 2000);
}

MapFeature *MainWindow::findCountryAt(const QPointF &point) const
{
    for (QGraphicsItem *item : m_scene->items()) {
        auto *feature = dynamic_cast<MapFeature *>(item);
        if (feature && feature->kind() == FeatureKind::Country
            && feature->polygon().containsPoint(point, Qt::OddEvenFill)) {
            return feature;
        }
    }
    return nullptr;
}

void MainWindow::addSettlement(const QPointF &point)
{
    QStringList options;
    for (SettlementType type : {SettlementType::City, SettlementType::Town, SettlementType::Village})
        options.append(settlementName(type));

    bool ok = false;
    const QString typeName = QInputDialog::getItem(this, tr("New Settlement"), tr("Type:"), options, 0,
                                                   false, &ok);
    if (!ok)
        return;

    SettlementType type = SettlementType::City;
    for (SettlementType candidate : {SettlementType::City, SettlementType::Town, SettlementType::Village}) {
        if (settlementName(candidate) == typeName)
            type = candidate;
    }

    int sameType = 0;
    for (QGraphicsItem *item : m_scene->items()) {
        auto *feature = dynamic_cast<MapFeature *>(item);
        if (feature && feature->kind() == FeatureKind::Settlement && feature->settlement == type)
            ++sameType;
    }

    const QString name = QInputDialog::getText(this, tr("New Settlement"), tr("Name:"), QLineEdit::Normal,
                                               QStringLiteral("%1 %2").arg(typeName).arg(sameType + 1), &ok)
                             .trimmed();
    if (!ok || name.isEmpty())
        return;

    auto *settlement = new MapFeature(FeatureKind::Settlement);
    settlement->setPoint(point);
    settlement->settlement = type;
    settlement->name = name;
    m_scene->addItem(settlement);
    statusBar()->showMessage(tr("%1 %2 placed.").arg(typeName, name), 2000);
}

int MainWindow::featureCount(FeatureKind kind) const
{
    int count = 0;
    for (QGraphicsItem *item : m_scene->items()) {
        if (auto *feature = dynamic_cast<MapFeature *>(item); feature && feature->kind() == kind)
            ++count;
    }
    return count;
}

QColor MainWindow::nextCountryColor() const
{
    const int count = std::size(kCountryColors);
    return kCountryColors[featureCount(FeatureKind::Country) % count];
}

void MainWindow::setMode(MapView::Mode mode)
{
    m_view->setMode(mode);
    updateVisibility(m_view->zoomScale());
    if (mode == MapView::Mode::Explore) {
        m_exploreAction->setChecked(true);
        statusBar()->showMessage(tr("Explore: drag to pan, wheel to zoom."));
    } else {
        m_editAction->setChecked(true);
        statusBar()->showMessage(tr("Edit mode."));
    }
}
