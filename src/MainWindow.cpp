#include "MainWindow.h"

#include "MapFeature.h"

#include <QAction>
#include <QActionGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDockWidget>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFont>
#include <QFormLayout>
#include <QGroupBox>
#include <QGraphicsScene>
#include <QHBoxLayout>
#include <QIcon>
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QLineF>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPainterPath>
#include <QPainterPathStroker>
#include <QPixmap>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRandomGenerator>
#include <QSpinBox>
#include <QStatusBar>
#include <QTabWidget>
#include <QTimer>
#include <QToolBar>
#include <QVBoxLayout>
#include <QWidget>

#include <algorithm>
#include <cmath>

namespace {
const QColor kSeaColor(0x3d, 0x6a, 0x93);

double polygonArea(const QPolygonF &polygon)
{
    double area = 0.0;
    for (int i = 0; i < polygon.size(); ++i) {
        const QPointF &a = polygon.at(i);
        const QPointF &b = polygon.at((i + 1) % polygon.size());
        area += a.x() * b.y() - b.x() * a.y();
    }
    return std::abs(area) / 2.0;
}

QPointF polygonCenter(const QPolygonF &polygon)
{
    double area = 0.0;
    double cx = 0.0;
    double cy = 0.0;
    for (int i = 0; i < polygon.size(); ++i) {
        const QPointF &a = polygon.at(i);
        const QPointF &b = polygon.at((i + 1) % polygon.size());
        const double cross = a.x() * b.y() - b.x() * a.y();
        area += cross;
        cx += (a.x() + b.x()) * cross;
        cy += (a.y() + b.y()) * cross;
    }
    if (qFuzzyIsNull(area))
        return polygon.boundingRect().center();
    area *= 0.5;
    return QPointF(cx / (6.0 * area), cy / (6.0 * area));
}

/// Move vertices that are within tolerance of another polygon's edge exactly
/// onto that edge. Shared borders are computed independently and can differ by
/// tiny fractions, which makes QPainterPath's boolean operations fail.
void snapToEdges(QPolygonF &polygon, const QPolygonF &target, double tolerance)
{
    for (QPointF &point : polygon) {
        double bestDistance = tolerance;
        QPointF bestPoint = point;
        for (int i = 0; i < target.size(); ++i) {
            const QPointF a = target.at(i);
            const QPointF b = target.at((i + 1) % target.size());
            const QPointF edge = b - a;
            const double lengthSquared = edge.x() * edge.x() + edge.y() * edge.y();
            if (lengthSquared <= 0.0)
                continue;
            const QPointF offset = point - a;
            double t = (offset.x() * edge.x() + offset.y() * edge.y()) / lengthSquared;
            t = qBound(0.0, t, 1.0);
            const QPointF candidate = a + edge * t;
            const double distance = QLineF(point, candidate).length();
            if (distance < bestDistance) {
                bestDistance = distance;
                bestPoint = candidate;
            }
        }
        point = bestPoint;
    }
}

/// True when two terrain areas share a stretch of border or overlap.
bool polygonsTouch(const QPolygonF &a, const QPolygonF &b)
{
    if (a.size() < 3 || b.size() < 3)
        return false;

    QPainterPath aPath;
    aPath.addPolygon(a);
    aPath.closeSubpath();
    QPainterPath bPath;
    bPath.addPolygon(b);
    bPath.closeSubpath();

    if (!aPath.boundingRect().adjusted(-2, -2, 2, 2).intersects(bPath.boundingRect()))
        return false;

    QPainterPathStroker stroker;
    stroker.setWidth(2.0);
    return aPath.intersects(stroker.createStroke(bPath))
           || bPath.intersects(stroker.createStroke(aPath));
}

/// Terrain only meets terrain it can fade into. Desert and forest always keep
/// at least grassland, mountains or water between them.
bool terrainCanBorder(TerrainType a, TerrainType b)
{
    if (a == b)
        return true;
    switch (a) {
    case TerrainType::Grass:
    case TerrainType::Water:
        return true;
    case TerrainType::Forest:
        return b != TerrainType::Desert;
    case TerrainType::Desert:
        return b != TerrainType::Forest && b != TerrainType::Swamp;
    case TerrainType::Mountains:
        return b != TerrainType::Swamp;
    case TerrainType::Swamp:
        return b != TerrainType::Desert && b != TerrainType::Mountains;
    }
    return true;
}

/// Pick a random terrain that is compatible with every area it touches.
TerrainType compatibleTerrain(const QPolygonF &polygon, const QVector<MapFeature *> &terrainAreas)
{
    QVector<TerrainType> neighbours;
    for (const MapFeature *area : terrainAreas) {
        if (polygonsTouch(polygon, area->polygon()))
            neighbours.append(area->terrain);
    }

    QVector<TerrainType> candidates;
    for (TerrainType type : terrainTypes()) {
        bool compatible = true;
        for (TerrainType neighbour : neighbours) {
            if (!terrainCanBorder(type, neighbour)) {
                compatible = false;
                break;
            }
        }
        if (compatible)
            candidates.append(type);
    }

    if (candidates.isEmpty())
        return TerrainType::Grass;
    return candidates.at(QRandomGenerator::global()->bounded(candidates.size()));
}

QPainterPath halfPlanePath(const QRectF &bounds, const QPointF &through, const QPointF &direction,
                           bool positive)
{
    const double margin = std::max(bounds.width(), bounds.height()) * 2.0 + 100.0;
    const QRectF big = bounds.adjusted(-margin, -margin, margin, margin);
    const QPolygonF rect{QPointF(big.topLeft()), QPointF(big.topRight()),
                         QPointF(big.bottomRight()), QPointF(big.bottomLeft())};

    const QPointF normal(-direction.y(), direction.x());
    const double sign = positive ? 1.0 : -1.0;

    QPolygonF clipped;
    for (int i = 0; i < rect.size(); ++i) {
        const QPointF &a = rect.at(i);
        const QPointF &b = rect.at((i + 1) % rect.size());
        const double da = QPointF::dotProduct(a - through, normal) * sign;
        const double db = QPointF::dotProduct(b - through, normal) * sign;

        if (da >= 0.0)
            clipped.append(a);
        if ((da >= 0.0) != (db >= 0.0)) {
            const double t = da / (da - db);
            clipped.append(a + (b - a) * t);
        }
    }

    QPainterPath path;
    path.addPolygon(clipped);
    path.closeSubpath();
    return path;
}

bool splitPolygon(const QPolygonF &polygon, QVector<QPolygonF> &pieces)
{
    if (polygon.size() < 3)
        return false;
    QPainterPath subject;
    subject.addPolygon(polygon);
    subject.closeSubpath();

    const QRectF bounds = polygon.boundingRect();
    const QPointF center = polygonCenter(polygon);
    const double area = polygonArea(polygon);
    if (area <= 0.0)
        return false;

    constexpr double kPi = 3.14159265358979323846;
    for (int attempt = 0; attempt < 12; ++attempt) {
        const double angle = QRandomGenerator::global()->generateDouble() * kPi;
        const QPointF direction(std::cos(angle), std::sin(angle));
        const QPointF through = center
                                + QPointF((QRandomGenerator::global()->generateDouble() - 0.5)
                                              * bounds.width() * 0.3,
                                          (QRandomGenerator::global()->generateDouble() - 0.5)
                                              * bounds.height() * 0.3);

        const QList<QPolygonF> sides[2] = {
            subject.intersected(halfPlanePath(bounds, through, direction, true)).toFillPolygons(),
            subject.intersected(halfPlanePath(bounds, through, direction, false)).toFillPolygons(),
        };
        if (sides[0].isEmpty() || sides[1].isEmpty())
            continue;

        double sideAreas[2] = {0.0, 0.0};
        for (int side = 0; side < 2; ++side)
            for (const QPolygonF &part : sides[side])
                sideAreas[side] += polygonArea(part);

        if (sideAreas[0] < area * 0.02 || sideAreas[1] < area * 0.02)
            continue;

        pieces.clear();
        for (const QPolygonF &part : sides[0])
            if (part.size() >= 3)
                pieces.append(part);
        for (const QPolygonF &part : sides[1])
            if (part.size() >= 3)
                pieces.append(part);
        return pieces.size() >= 2;
    }
    return false;
}

void splitPieces(QVector<QPolygonF> &pieces, int target);

QVector<QPolygonF> fillPieces(const QPolygonF &area, const QVector<QPolygonF> &blocked,
                              int target)
{
    QPainterPath freePath;
    freePath.addPolygon(area);
    freePath.closeSubpath();

    for (const QPolygonF &other : blocked) {
        QPainterPath otherPath;
        otherPath.addPolygon(other);
        otherPath.closeSubpath();
        freePath = freePath.subtracted(otherPath);
    }

    QVector<QPolygonF> pieces;
    const double areaSize = polygonArea(area);
    for (const QPolygonF &piece : freePath.toFillPolygons()) {
        if (piece.size() >= 3 && polygonArea(piece) > areaSize * 0.001)
            pieces.append(piece);
    }

    splitPieces(pieces, target);
    return pieces;
}

void splitPieces(QVector<QPolygonF> &pieces, int target)
{
    QVector<bool> stuck;
    stuck.fill(false, pieces.size());

    while (pieces.size() < target) {
        int best = -1;
        double bestArea = 0.0;
        for (int i = 0; i < pieces.size(); ++i) {
            if (stuck.at(i))
                continue;
            const double area = polygonArea(pieces.at(i));
            if (area > bestArea) {
                bestArea = area;
                best = i;
            }
        }
        if (best < 0)
            break;

        QVector<QPolygonF> split;
        if (!splitPolygon(pieces.at(best), split)) {
            stuck[best] = true;
            continue;
        }

        pieces.removeAt(best);
        stuck.removeAt(best);
        for (const QPolygonF &piece : split) {
            pieces.append(piece);
            stuck.append(false);
        }
    }
}

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

    resetGlobalLists();
    createScene();
    createActions();

    m_view = new MapView(m_scene, this);
    m_view->setWorldSize(m_worldSize);
    setCentralWidget(m_view);

    m_zoomLabel = new QLabel(tr("100%"), this);
    statusBar()->addPermanentWidget(m_zoomLabel);
    statusBar()->showMessage(tr("Explore: drag to pan, wheel to zoom."));

    createInfoDock();
    createLogDock();

    connect(m_view, &MapView::zoomChanged, this, [this](double scale) {
        m_zoomLabel->setText(QStringLiteral("%1%").arg(qRound(scale * 100)));
    });
    connect(m_view, &MapView::zoomChanged, this, &MainWindow::updateVisibility);
    connect(m_view, &MapView::featureClicked, this, &MainWindow::onFeatureClicked);
    connect(m_view, &MapView::featureSelected, this, &MainWindow::selectFeature);
    connect(m_view, &MapView::featureEdited, this, [this] {
        if (m_selected)
            selectFeature(m_selected);
        pushHistory();
        statusBar()->showMessage(tr("Item moved."), 1500);
    });
    connect(m_view, &MapView::deleteRequested, this, &MainWindow::deleteSelectedFeature);

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
        case MapView::Tool::Road:
            addRoad(polygon);
            break;
        case MapView::Tool::Railroad:
            addRailroad(polygon);
            break;
        case MapView::Tool::River:
            addRiver(polygon);
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
    layout->addWidget(m_editButton = new QPushButton(tr("Edit..."), widget));
    m_editButton->setEnabled(false);
    connect(m_editButton, &QPushButton::clicked, this, &MainWindow::editSelectedFeature);

    m_deleteButton = new QPushButton(tr("Delete..."), widget);
    m_deleteButton->setEnabled(false);
    connect(m_deleteButton, &QPushButton::clicked, this, &MainWindow::deleteSelectedFeature);
    layout->addWidget(m_deleteButton);

    m_regionsButton = new QPushButton(tr("Fill with..."), widget);
    m_regionsButton->setVisible(false);
    connect(m_regionsButton, &QPushButton::clicked, this,
            &MainWindow::fillSelectedArea);
    layout->addWidget(m_regionsButton);
    layout->addStretch();

    m_infoDock->setWidget(widget);
    addDockWidget(Qt::RightDockWidgetArea, m_infoDock);

    if (m_viewMenu) {
        m_viewMenu->addSeparator();
        m_viewMenu->addAction(m_infoDock->toggleViewAction());
    }
}

void MainWindow::createLogDock()
{
    m_logDock = new QDockWidget(tr("Travel Log"), this);
    m_logDock->setObjectName(QStringLiteral("travelLogDock"));
    m_logDock->setMinimumWidth(250);

    auto *widget = new QWidget(m_logDock);
    auto *layout = new QVBoxLayout(widget);
    layout->setContentsMargins(6, 6, 6, 6);

    m_logView = new QPlainTextEdit(widget);
    m_logView->setReadOnly(true);
    m_logView->setPlaceholderText(tr("Travels and visits appear here."));
    layout->addWidget(m_logView);

    auto *exportButton = new QPushButton(tr("Export..."), widget);
    connect(exportButton, &QPushButton::clicked, this, &MainWindow::exportTravelLog);
    layout->addWidget(exportButton);

    m_logDock->setWidget(widget);
    addDockWidget(Qt::RightDockWidgetArea, m_logDock);

    if (m_viewMenu)
        m_viewMenu->addAction(m_logDock->toggleViewAction());
}

void MainWindow::logTravel(const QString &entry)
{
    if (!m_logView || entry.isEmpty())
        return;
    m_logView->appendPlainText(entry);
}

void MainWindow::exportTravelLog()
{
    if (!m_logView)
        return;

    QString path = QFileDialog::getSaveFileName(this, tr("Export Travel Log"),
                                                QStringLiteral("travel-log.txt"),
                                                tr("Text files (*.txt)"));
    if (path.isEmpty())
        return;
    if (!path.endsWith(QStringLiteral(".txt"), Qt::CaseInsensitive))
        path += QStringLiteral(".txt");

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, tr("Export Travel Log"),
                             tr("Could not write %1.").arg(path));
        return;
    }
    file.write(m_logView->toPlainText().toUtf8());
    statusBar()->showMessage(tr("Travel log exported to %1.").arg(QFileInfo(path).fileName()), 2500);
}

void MainWindow::selectFeature(MapFeature *feature)
{
    if (m_view)
        m_view->setEditSelection(feature);

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
        if (m_editButton)
            m_editButton->setEnabled(false);
        if (m_deleteButton)
            m_deleteButton->setEnabled(false);
        if (m_regionsButton)
            m_regionsButton->setVisible(false);
        return;
    }

    if (m_editButton)
        m_editButton->setEnabled(true);
    if (m_deleteButton)
        m_deleteButton->setEnabled(true);
    if (m_regionsButton) {
        const bool isCountry = feature->kind() == FeatureKind::Country;
        const bool isLand = feature->kind() == FeatureKind::Land;
        m_regionsButton->setVisible(isCountry || isLand);
        m_regionsButton->setText(isLand ? tr("Add Random Countries...")
                                        : tr("Add Random Regions..."));
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
    case FeatureKind::Road: {
        double length = 0.0;
        for (int i = 0; i + 1 < feature->polygon().size(); ++i)
            length += QLineF(feature->polygon().at(i), feature->polygon().at(i + 1)).length();
        m_infoDetails->setText(tr("Road, %1 px, cost %2")
                                   .arg(qRound(length))
                                   .arg(qRound(roadTravelCost(feature->polygon()))));
        break;
    }
    case FeatureKind::River:
        m_infoDetails->setText(tr("River"));
        break;
    case FeatureKind::Railroad: {
        double length = 0.0;
        for (int i = 0; i + 1 < feature->polygon().size(); ++i)
            length += QLineF(feature->polygon().at(i), feature->polygon().at(i + 1)).length();
        m_infoDetails->setText(tr("Railroad, %1 px").arg(qRound(length)));
        break;
    }
    }

    QStringList blocks;
    if (!feature->description.isEmpty())
        blocks.append(feature->description);
    if (!feature->history.isEmpty())
        blocks.append(tr("History: %1").arg(feature->history));
    if (!feature->facts.isEmpty())
        blocks.append(tr("Facts: %1").arg(feature->facts));
    if (!feature->languages.isEmpty())
        blocks.append(tr("Languages: %1").arg(feature->languages.join(QStringLiteral(", "))));
    if (!feature->characteristics.isEmpty()) {
        blocks.append(tr("Characteristics: %1")
                          .arg(feature->characteristics.join(QStringLiteral(", "))));
    }
    m_infoDescription->setText(blocks.isEmpty() ? tr("No description.") : blocks.join("\n\n"));
}

void MainWindow::onFeatureClicked(MapFeature *feature)
{
    selectFeature(feature);
    if (!feature)
        return;

    const bool travelTarget = feature->kind() == FeatureKind::Settlement
                              || feature->kind() == FeatureKind::Region
                              || feature->kind() == FeatureKind::Country;
    if (m_view->mode() == MapView::Mode::Explore && travelTarget) {
        const QString title = feature->name.isEmpty() ? feature->kindName() : feature->name;

        if (!m_travelOrigin) {
            if (feature->kind() == FeatureKind::Settlement) {
                m_travelOrigin = feature;
                logTravel(tr("Started at %1.").arg(title));
                statusBar()->showMessage(tr("Travel started at %1.").arg(title), 2500);
            }
        } else if (feature != m_travelOrigin) {
            const QPointF target = feature->kind() == FeatureKind::Settlement
                                       ? feature->point()
                                       : feature->centroid();
            const QPointF origin = m_travelOrigin->kind() == FeatureKind::Settlement
                                       ? m_travelOrigin->point()
                                       : m_travelOrigin->centroid();
            const int days = qMax(1, qRound(lineTravelCost(origin, target) / 30.0));
            logTravel(tr("Walked %1 days, came to %2.").arg(days).arg(title));
            statusBar()->showMessage(tr("Walked %1 days to %2.").arg(days).arg(title), 3000);
            m_travelOrigin = feature;
        }
    }

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
    case FeatureKind::Road:
    case FeatureKind::River:
    case FeatureKind::Railroad: {
        const QRectF bounds = feature->polygon().boundingRect();
        m_view->zoomToRect(bounds.adjusted(-40, -40, 40, 40));
        break;
    }
    default:
        break;
    }
}

void MainWindow::updateVisibility(double scale)
{
    const bool showDetails = m_view->mode() == MapView::Mode::Edit || scale >= 0.5;
    for (QGraphicsItem *item : m_scene->items()) {
        auto *feature = dynamic_cast<MapFeature *>(item);
        if (feature && (feature->kind() == FeatureKind::Region
                        || feature->kind() == FeatureKind::Settlement)) {
            feature->setVisible(showDetails);
        }
    }
}

QListWidget *makeCheckList(QWidget *parent, const QStringList &available,
                           const QStringList &selected)
{
    auto *list = new QListWidget(parent);
    QStringList values = available;
    for (const QString &value : selected) {
        if (!values.contains(value))
            values.append(value);
    }
    for (const QString &value : values) {
        auto *item = new QListWidgetItem(value, list);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(selected.contains(value) ? Qt::Checked : Qt::Unchecked);
    }
    return list;
}

QStringList checkedValues(const QListWidget *list)
{
    QStringList values;
    for (int i = 0; i < list->count(); ++i) {
        const QListWidgetItem *item = list->item(i);
        if (item->checkState() == Qt::Checked)
            values.append(item->text());
    }
    return values;
}

void MainWindow::resetGlobalLists()
{
    m_languages = {tr("Common"), tr("Elvish"), tr("Dwarvish")};
    m_characteristics = {tr("Coastal"), tr("Mountainous"), tr("Fertile"), tr("Trade hub"),
                         tr("Warlike")};
}

void MainWindow::editGlobalLists()
{
    QDialog dialog(this);
    dialog.setWindowTitle(tr("Languages and Characteristics"));
    dialog.resize(520, 420);

    auto *layout = new QHBoxLayout(&dialog);
    QListWidget *languageList = nullptr;
    QListWidget *characteristicList = nullptr;

    auto makeEditor = [&](const QString &title, const QStringList &values,
                          QListWidget **result) {
        auto *box = new QGroupBox(title, &dialog);
        auto *boxLayout = new QVBoxLayout(box);

        auto *list = new QListWidget(box);
        list->addItems(values);
        boxLayout->addWidget(list);

        auto *row = new QHBoxLayout;
        auto *edit = new QLineEdit(box);
        edit->setPlaceholderText(tr("New..."));
        auto *add = new QPushButton(tr("Add"), box);
        auto *remove = new QPushButton(tr("Remove"), box);
        row->addWidget(edit);
        row->addWidget(add);
        row->addWidget(remove);
        boxLayout->addLayout(row);

        auto addValue = [list, edit] {
            const QString text = edit->text().trimmed();
            if (text.isEmpty())
                return;
            if (list->findItems(text, Qt::MatchFixedString).isEmpty())
                list->addItem(text);
            edit->clear();
        };
        connect(add, &QPushButton::clicked, box, addValue);
        connect(edit, &QLineEdit::returnPressed, box, addValue);
        connect(remove, &QPushButton::clicked, box, [list] { delete list->currentItem(); });

        layout->addWidget(box);
        *result = list;
    };

    makeEditor(tr("Languages"), m_languages, &languageList);
    makeEditor(tr("Characteristics"), m_characteristics, &characteristicList);

    auto *buttonRow = new QVBoxLayout;
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    buttonRow->addStretch();
    buttonRow->addWidget(buttons);
    layout->addLayout(buttonRow);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    if (dialog.exec() != QDialog::Accepted)
        return;

    m_languages.clear();
    for (int i = 0; i < languageList->count(); ++i)
        m_languages.append(languageList->item(i)->text());
    m_characteristics.clear();
    for (int i = 0; i < characteristicList->count(); ++i)
        m_characteristics.append(characteristicList->item(i)->text());
    pushHistory();
}

void MainWindow::editSelectedFeature()
{
    if (!m_selected)
        return;

    MapFeature *feature = m_selected;
    const QString oldName = feature->name;

    QDialog dialog(this);
    dialog.setWindowTitle(tr("Edit %1").arg(feature->kindName()));
    dialog.resize(480, 560);

    auto *layout = new QVBoxLayout(&dialog);
    auto *tabs = new QTabWidget(&dialog);

    auto *basicTab = new QWidget(tabs);
    auto *basicForm = new QFormLayout(basicTab);
    auto *nameEdit = new QLineEdit(feature->name, basicTab);
    auto *descriptionEdit = new QPlainTextEdit(feature->description, basicTab);
    descriptionEdit->setMinimumSize(340, 150);
    basicForm->addRow(tr("Name:"), nameEdit);
    basicForm->addRow(tr("Description:"), descriptionEdit);
    tabs->addTab(basicTab, tr("Basic"));

    auto *loreTab = new QWidget(tabs);
    auto *loreForm = new QFormLayout(loreTab);
    auto *historyEdit = new QPlainTextEdit(feature->history, loreTab);
    historyEdit->setMinimumSize(340, 120);
    auto *factsEdit = new QPlainTextEdit(feature->facts, loreTab);
    factsEdit->setMinimumSize(340, 120);
    loreForm->addRow(tr("History:"), historyEdit);
    loreForm->addRow(tr("Facts:"), factsEdit);
    tabs->addTab(loreTab, tr("History and facts"));

    auto *cultureTab = new QWidget(tabs);
    auto *cultureLayout = new QVBoxLayout(cultureTab);
    cultureLayout->addWidget(new QLabel(tr("Languages:"), cultureTab));
    auto *languageList = makeCheckList(cultureTab, m_languages, feature->languages);
    cultureLayout->addWidget(languageList);
    cultureLayout->addWidget(new QLabel(tr("Characteristics:"), cultureTab));
    auto *characteristicList =
        makeCheckList(cultureTab, m_characteristics, feature->characteristics);
    cultureLayout->addWidget(characteristicList);
    auto *hint = new QLabel(
        tr("Edit the choices from Edit > Languages and Characteristics..."), cultureTab);
    hint->setWordWrap(true);
    cultureLayout->addWidget(hint);
    tabs->addTab(cultureTab, tr("Culture"));

    layout->addWidget(tabs);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    if (dialog.exec() != QDialog::Accepted)
        return;

    feature->name = nameEdit->text().trimmed();
    feature->description = descriptionEdit->toPlainText().trimmed();
    feature->history = historyEdit->toPlainText().trimmed();
    feature->facts = factsEdit->toPlainText().trimmed();
    feature->languages = checkedValues(languageList);
    feature->characteristics = checkedValues(characteristicList);

    if (feature->kind() == FeatureKind::Country && feature->name != oldName) {
        for (QGraphicsItem *item : m_scene->items()) {
            auto *region = dynamic_cast<MapFeature *>(item);
            if (region && region->kind() == FeatureKind::Region && region->countryName == oldName) {
                region->countryName = feature->name;
                region->refresh();
            }
        }
    }

    feature->refresh();
    selectFeature(feature);
    pushHistory();
    statusBar()->showMessage(tr("Updated %1.").arg(feature->kindName().toLower()), 2000);
}

void MainWindow::deleteSelectedFeature()
{
    if (!m_selected) {
        statusBar()->showMessage(tr("Select an item first."), 2000);
        return;
    }

    MapFeature *feature = m_selected;
    const FeatureKind kind = feature->kind();
    const QPolygonF vacated = feature->polygon();
    const QString countryName = feature->countryName;
    QString title = feature->name;
    if (title.isEmpty())
        title = kind == FeatureKind::TerrainArea ? terrainName(feature->terrain)
                                                 : feature->kindName();

    const bool area = kind == FeatureKind::Land || kind == FeatureKind::Country
                      || kind == FeatureKind::Region || kind == FeatureKind::TerrainArea;

    QVector<MapFeature *> siblings;
    if (area) {
        for (QGraphicsItem *item : m_scene->items()) {
            auto *other = dynamic_cast<MapFeature *>(item);
            if (!other || other == feature)
                continue;
            if (kind == FeatureKind::Region && other->kind() == FeatureKind::Region
                && other->countryName == countryName) {
                siblings.append(other);
            } else if (kind == FeatureKind::Country && other->kind() == FeatureKind::Country) {
                siblings.append(other);
            } else if (kind == FeatureKind::Land && other->kind() == FeatureKind::Land) {
                siblings.append(other);
            } else if (kind == FeatureKind::TerrainArea
                       && other->kind() == FeatureKind::TerrainArea) {
                siblings.append(other);
            }
        }
    }

    selectFeature(nullptr);
    delete feature;

    if (kind == FeatureKind::Country) {
        for (QGraphicsItem *item : m_scene->items()) {
            auto *region = dynamic_cast<MapFeature *>(item);
            if (region && region->kind() == FeatureKind::Region
                && region->countryName == countryName) {
                delete region;
            }
        }
    }

    bool expand = false;
    if (area && !siblings.isEmpty()) {
        const auto answer = QMessageBox::question(
            this, tr("Delete Item"),
            tr("Deleted %1.\n\nExpand bordering items to fill the vacated space?").arg(title),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
        expand = answer == QMessageBox::Yes;
    }

    if (expand)
        expandIntoVacated(vacated, siblings);

    pushHistory();
    statusBar()->showMessage(tr("Deleted %1.").arg(title), 2500);
}

void MainWindow::expandIntoVacated(const QPolygonF &vacated,
                                   const QVector<MapFeature *> &siblings)
{
    if (vacated.size() < 3 || siblings.isEmpty())
        return;

    QPainterPath vacatedPath;
    vacatedPath.addPolygon(vacated);
    vacatedPath.closeSubpath();

    // Only a neighbour that shares a real stretch of border can absorb the
    // area. Touching at a single point would produce a broken outline.
    QPainterPathStroker stroker;
    stroker.setWidth(2.0);

    QVector<QPair<double, MapFeature *>> candidates;
    for (MapFeature *sibling : siblings) {
        QPainterPath siblingPath;
        siblingPath.addPolygon(sibling->polygon());
        siblingPath.closeSubpath();

        const QPainterPath sharedBorder = vacatedPath.intersected(stroker.createStroke(siblingPath));
        const double border = polygonArea(sharedBorder.toFillPolygon()) * 0.5;
        if (border >= 5.0)
            candidates.append({border, sibling});
    }
    std::sort(candidates.begin(), candidates.end(),
              [](const auto &a, const auto &b) { return a.first > b.first; });

    for (const auto &candidate : candidates) {
        MapFeature *sibling = candidate.second;
        QPolygonF siblingPolygon = sibling->polygon();
        QPolygonF vacatedPolygon = vacated;
        // Neighbouring polygons come from separate clipping operations, so
        // their shared border can be off by a small fraction. Snap them
        // together or QPainterPath's union silently drops one side.
        snapToEdges(vacatedPolygon, siblingPolygon, 1.0);
        snapToEdges(siblingPolygon, vacated, 1.0);

        QPainterPath siblingPath;
        siblingPath.addPolygon(siblingPolygon);
        siblingPath.closeSubpath();
        QPainterPath vacatedSnappedPath;
        vacatedSnappedPath.addPolygon(vacatedPolygon);
        vacatedSnappedPath.closeSubpath();

        const QPolygonF result =
            siblingPath.united(vacatedSnappedPath).simplified().toFillPolygon();
        const double expected = polygonArea(siblingPolygon) + polygonArea(vacatedPolygon);
        if (result.size() < 3 || polygonArea(result) < expected * 0.95)
            continue;

        sibling->setPolygon(result);
        sibling->refresh();
        return;
    }
}

void MainWindow::saveWorld()
{
    if (m_currentFile.isEmpty()) {
        saveWorldAs();
        return;
    }
    if (writeWorld(m_currentFile))
        statusBar()->showMessage(tr("Saved %1").arg(QFileInfo(m_currentFile).fileName()), 2500);
}

void MainWindow::saveWorldAs()
{
    const QString start = m_currentFile.isEmpty() ? QStringLiteral("world.world") : m_currentFile;
    QString selectedFilter;
    QString path = QFileDialog::getSaveFileName(this, tr("Save World"), start,
                                                tr("World files (*.world);;JSON files (*.json)"),
                                                &selectedFilter);
    if (path.isEmpty())
        return;
    if (!path.endsWith(QStringLiteral(".world"), Qt::CaseInsensitive)
        && !path.endsWith(QStringLiteral(".json"), Qt::CaseInsensitive)) {
        path += selectedFilter.contains(QStringLiteral("JSON")) ? QStringLiteral(".json")
                                                                : QStringLiteral(".world");
    }

    if (writeWorld(path)) {
        m_currentFile = path;
        updateTitle();
        statusBar()->showMessage(tr("Saved %1").arg(QFileInfo(path).fileName()), 2500);
    }
}

bool MainWindow::writeWorld(const QString &path)
{
    const QJsonObject root = worldToJson();

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        QMessageBox::warning(this, tr("Save World"),
                             tr("Could not write %1:\n%2").arg(path, file.errorString()));
        return false;
    }

    const QByteArray json = QJsonDocument(root).toJson(
        path.endsWith(QStringLiteral(".json"), Qt::CaseInsensitive) ? QJsonDocument::Indented
                                                                   : QJsonDocument::Compact);
    const QByteArray data = path.endsWith(QStringLiteral(".json"), Qt::CaseInsensitive)
                                ? json
                                : qCompress(json, 9);
    if (file.write(data) != data.size()) {
        QMessageBox::warning(this, tr("Save World"),
                             tr("Could not write %1:\n%2").arg(path, file.errorString()));
        return false;
    }
    return true;
}

void MainWindow::loadWorld()
{
    const QString path = QFileDialog::getOpenFileName(this, tr("Open World"), m_currentFile,
                                                      tr("World files (*.world *.json);;All files (*)"));
    if (path.isEmpty())
        return;

    if (readWorld(path)) {
        m_currentFile = path;
        updateTitle();
        statusBar()->showMessage(tr("Loaded %1").arg(QFileInfo(path).fileName()), 2500);
    }
}

bool MainWindow::readWorld(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this, tr("Open World"),
                             tr("Could not read %1:\n%2").arg(path, file.errorString()));
        return false;
    }

    const QByteArray data = file.readAll();
    const QByteArray json = path.endsWith(QStringLiteral(".json"), Qt::CaseInsensitive)
                                ? data
                                : qUncompress(data);
    if (json.isEmpty()) {
        QMessageBox::warning(this, tr("Open World"),
                             tr("Could not read %1:\nThe file is empty or not a world file.").arg(path));
        return false;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(json, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        QMessageBox::warning(this, tr("Open World"),
                             tr("Could not read %1:\n%2").arg(path, parseError.errorString()));
        return false;
    }

    const QJsonObject root = document.object();
    const double width = root.value(QStringLiteral("width")).toDouble();
    const double height = root.value(QStringLiteral("height")).toDouble();
    if (width <= 0 || height <= 0) {
        QMessageBox::warning(this, tr("Open World"),
                             tr("Could not read %1:\nThe world size is missing.").arg(path));
        return false;
    }

    const QJsonArray featureArray = root.value(QStringLiteral("features")).toArray();
    for (const QJsonValue &value : featureArray) {
        MapFeature *probe = MapFeature::fromJson(value.toObject());
        if (!probe) {
            QMessageBox::warning(this, tr("Open World"),
                                 tr("Could not read %1:\nIt contains an unknown feature.").arg(path));
            return false;
        }
        delete probe;
    }
    // Validate the whole document before replacing the current world.
    restoreFromJson(root);
    m_view->fitWorld();
    resetHistory();
    return true;
}

QJsonObject MainWindow::worldToJson() const
{
    QJsonArray features;
    const QList<QGraphicsItem *> items = m_scene->items(Qt::AscendingOrder);
    for (QGraphicsItem *item : items) {
        if (auto *feature = dynamic_cast<MapFeature *>(item))
            features.append(feature->toJson());
    }

    QJsonObject root;
    root.insert(QStringLiteral("version"), 1);
    root.insert(QStringLiteral("width"), m_worldSize.width());
    root.insert(QStringLiteral("height"), m_worldSize.height());
    root.insert(QStringLiteral("languages"), QJsonArray::fromStringList(m_languages));
    root.insert(QStringLiteral("characteristics"),
                QJsonArray::fromStringList(m_characteristics));
    root.insert(QStringLiteral("features"), features);
    return root;
}

void MainWindow::restoreFromJson(const QJsonObject &root)
{
    const double width = root.value(QStringLiteral("width")).toDouble();
    const double height = root.value(QStringLiteral("height")).toDouble();

    if (root.contains(QStringLiteral("languages"))
        || root.contains(QStringLiteral("characteristics"))) {
        m_languages.clear();
        for (const QJsonValue &value : root.value(QStringLiteral("languages")).toArray()) {
            const QString language = value.toString();
            if (!language.isEmpty())
                m_languages.append(language);
        }
        m_characteristics.clear();
        for (const QJsonValue &value : root.value(QStringLiteral("characteristics")).toArray()) {
            const QString characteristic = value.toString();
            if (!characteristic.isEmpty())
                m_characteristics.append(characteristic);
        }
    } else {
        resetGlobalLists();
    }

    QVector<MapFeature *> features;
    const QJsonArray featureArray = root.value(QStringLiteral("features")).toArray();
    for (const QJsonValue &value : featureArray) {
        if (MapFeature *feature = MapFeature::fromJson(value.toObject()))
            features.append(feature);
    }

    selectFeature(nullptr);
    m_view->setTool(m_view->tool());
    m_scene->clear();
    m_travelOrigin = nullptr;
    m_worldSize = QSizeF(width, height);
    m_scene->setSceneRect(QRectF(QPointF(0, 0), m_worldSize));
    m_view->setWorldSize(m_worldSize);
    for (MapFeature *feature : features)
        m_scene->addItem(feature);
    updateVisibility(m_view->zoomScale());
}

void MainWindow::resetHistory()
{
    m_history.clear();
    m_history.append(worldToJson());
    m_historyIndex = 0;
    updateHistoryActions();
}

void MainWindow::pushHistory()
{
    if (m_restoring)
        return;

    if (m_historyIndex + 1 < m_history.size())
        m_history.erase(m_history.begin() + m_historyIndex + 1, m_history.end());
    m_history.append(worldToJson());
    if (m_history.size() > 100)
        m_history.removeFirst();
    m_historyIndex = m_history.size() - 1;
    updateHistoryActions();
}

void MainWindow::undo()
{
    if (m_historyIndex <= 0)
        return;
    --m_historyIndex;
    const int index = m_historyIndex;
    m_restoring = true;
    restoreFromJson(m_history.at(index));
    m_restoring = false;
    updateHistoryActions();
    statusBar()->showMessage(tr("Undo."), 1200);
}

void MainWindow::redo()
{
    if (m_historyIndex < 0 || m_historyIndex + 1 >= m_history.size())
        return;
    ++m_historyIndex;
    const int index = m_historyIndex;
    m_restoring = true;
    restoreFromJson(m_history.at(index));
    m_restoring = false;
    updateHistoryActions();
    statusBar()->showMessage(tr("Redo."), 1200);
}

void MainWindow::updateHistoryActions()
{
    if (m_undoAction)
        m_undoAction->setEnabled(m_historyIndex > 0);
    if (m_redoAction)
        m_redoAction->setEnabled(m_historyIndex >= 0 && m_historyIndex + 1 < m_history.size());
}

void MainWindow::resizeWorld()
{
    QDialog dialog(this);
    dialog.setWindowTitle(tr("Resize World"));
    auto *layout = new QFormLayout(&dialog);

    auto *widthSpin = new QSpinBox(&dialog);
    widthSpin->setRange(200, 50000);
    widthSpin->setSuffix(tr(" units"));
    widthSpin->setValue(qRound(m_worldSize.width()));

    auto *heightSpin = new QSpinBox(&dialog);
    heightSpin->setRange(200, 50000);
    heightSpin->setSuffix(tr(" units"));
    heightSpin->setValue(qRound(m_worldSize.height()));

    layout->addRow(tr("Width:"), widthSpin);
    layout->addRow(tr("Height:"), heightSpin);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    layout->addRow(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    if (dialog.exec() != QDialog::Accepted)
        return;

    m_worldSize = QSizeF(widthSpin->value(), heightSpin->value());
    m_scene->setSceneRect(QRectF(QPointF(0, 0), m_worldSize));
    m_view->setWorldSize(m_worldSize);
    m_view->fitWorld();
    pushHistory();
    statusBar()->showMessage(tr("World resized to %1 x %2.").arg(widthSpin->value()).arg(heightSpin->value()),
                             2500);
}

void MainWindow::updateTitle()
{
    setWindowTitle(m_currentFile.isEmpty()
                       ? tr("Worldbilder - Untitled")
                       : tr("Worldbilder - %1").arg(QFileInfo(m_currentFile).fileName()));
}

void MainWindow::showRandomDialog()
{
    QDialog dialog(this);
    dialog.setWindowTitle(tr("Random Additions"));

    auto *layout = new QVBoxLayout(&dialog);
    auto *countRow = new QHBoxLayout;
    countRow->addWidget(new QLabel(tr("Count:"), &dialog));
    auto *countSpin = new QSpinBox(&dialog);
    countSpin->setRange(1, 50);
    countSpin->setValue(5);
    countRow->addWidget(countSpin);
    countRow->addStretch();
    layout->addLayout(countRow);

    auto *fillCheck = new QCheckBox(tr("Fill the target area completely"), &dialog);
    fillCheck->setToolTip(tr("Countries, regions and terrain fill the selected or first "
                             "matching area instead of being placed as separate blobs."));
    layout->addWidget(fillCheck);

    auto addButton = [&](const QString &text, void (MainWindow::*method)(int, bool)) {
        auto *button = new QPushButton(text, &dialog);
        connect(button, &QPushButton::clicked, this,
                [this, countSpin, fillCheck, method] {
                    (this->*method)(countSpin->value(), fillCheck->isChecked());
                });
        layout->addWidget(button);
    };

    addButton(tr("Add random land"), &MainWindow::addRandomLand);
    addButton(tr("Add random countries"), &MainWindow::addRandomCountries);
    addButton(tr("Add random regions"), &MainWindow::addRandomRegions);
    addButton(tr("Add random terrain"), &MainWindow::addRandomTerrainAreas);
    addButton(tr("Add random settlements"), &MainWindow::addRandomSettlements);

    auto *closeButton = new QPushButton(tr("Close"), &dialog);
    connect(closeButton, &QPushButton::clicked, &dialog, &QDialog::accept);
    layout->addWidget(closeButton);

    dialog.exec();
}

QPointF MainWindow::randomWorldPoint() const
{
    return QPointF(QRandomGenerator::global()->generateDouble() * m_worldSize.width(),
                   QRandomGenerator::global()->generateDouble() * m_worldSize.height());
}

QPolygonF MainWindow::randomBlob(const QPointF &center, double radius) const
{
    const int points = 6 + QRandomGenerator::global()->bounded(4);
    QPolygonF polygon;
    constexpr double kPi = 3.14159265358979323846;
    for (int i = 0; i < points; ++i) {
        const double angle = 2.0 * kPi * i / points
                             + (QRandomGenerator::global()->generateDouble() - 0.5) * 0.5;
        const double r = radius * (0.55 + 0.45 * QRandomGenerator::global()->generateDouble());
        polygon.append(center + QPointF(std::cos(angle) * r, std::sin(angle) * r));
    }
    return polygon;
}

MapFeature *MainWindow::randomCountry() const
{
    QVector<MapFeature *> countries;
    for (QGraphicsItem *item : m_scene->items()) {
        auto *feature = dynamic_cast<MapFeature *>(item);
        if (feature && feature->kind() == FeatureKind::Country)
            countries.append(feature);
    }
    if (countries.isEmpty())
        return nullptr;
    return countries.at(QRandomGenerator::global()->bounded(countries.size()));
}

QPolygonF MainWindow::randomPolygonInCountry(MapFeature *country) const
{
    const QRectF bounds = country->polygon().boundingRect();
    QPainterPath countryPath;
    countryPath.addPolygon(country->polygon());
    countryPath.closeSubpath();

    for (int attempt = 0; attempt < 40; ++attempt) {
        const QPointF center(bounds.left() + QRandomGenerator::global()->generateDouble() * bounds.width(),
                             bounds.top() + QRandomGenerator::global()->generateDouble() * bounds.height());
        if (!country->polygon().containsPoint(center, Qt::OddEvenFill))
            continue;

        const double radius = qMin(bounds.width(), bounds.height())
                              * (0.08 + 0.15 * QRandomGenerator::global()->generateDouble());
        QPainterPath blob;
        blob.addPolygon(randomBlob(center, qMax(radius, 20.0)));
        blob.closeSubpath();

        const QPolygonF clipped = blob.intersected(countryPath).toFillPolygon();
        if (clipped.size() >= 3)
            return clipped;
    }
    return {};
}

void MainWindow::addRandomLand(int count, bool fill)
{
    Q_UNUSED(fill);
    int added = 0;
    for (int i = 0; i < count; ++i) {
        const QPointF center = randomWorldPoint();
        const double radius = 120.0 + QRandomGenerator::global()->generateDouble() * 280.0;
        addLand(randomBlob(center, radius));
        ++added;
    }
    statusBar()->showMessage(tr("Added %1 random land areas.").arg(added), 2500);
}

void MainWindow::addRandomCountries(int count, bool fill)
{
    if (fill) {
        MapFeature *land = nullptr;
        if (m_selected && m_selected->kind() == FeatureKind::Land) {
            land = m_selected;
        } else {
            for (QGraphicsItem *item : m_scene->items()) {
                auto *feature = dynamic_cast<MapFeature *>(item);
                if (feature && feature->kind() == FeatureKind::Land) {
                    land = feature;
                    break;
                }
            }
        }
        if (!land) {
            statusBar()->showMessage(tr("Draw some land first."), 2500);
            return;
        }
        fillLandWithCountries(land, count);
        return;
    }

    int added = 0;
    for (int i = 0; i < count; ++i) {
        QPolygonF polygon;
        for (int attempt = 0; attempt < 20 && polygon.size() < 3; ++attempt) {
            polygon = clipToLand(randomBlob(randomWorldPoint(),
                                            80.0 + QRandomGenerator::global()->generateDouble() * 220.0));
        }
        if (polygon.size() < 3)
            continue;

        auto *country = new MapFeature(FeatureKind::Country);
        country->setPolygon(polygon);
        country->name = tr("Country %1").arg(featureCount(FeatureKind::Country) + 1);
        country->color = nextCountryColor();
        m_scene->addItem(country);
        ++added;
    }
    if (added > 0)
        pushHistory();
    statusBar()->showMessage(added > 0 ? tr("Added %1 random countries.").arg(added)
                                       : tr("Draw some land first."),
                             2500);
}

void MainWindow::addRandomRegions(int count, bool fill)
{
    if (fill) {
        MapFeature *country = nullptr;
        if (m_selected && m_selected->kind() == FeatureKind::Country)
            country = m_selected;
        else
            country = randomCountry();
        if (!country) {
            statusBar()->showMessage(tr("Draw a country first."), 2500);
            return;
        }
        fillCountryWithRegions(country, count);
        return;
    }

    int added = 0;
    for (int i = 0; i < count; ++i) {
        MapFeature *country = randomCountry();
        if (!country)
            break;

        const QPolygonF polygon = randomPolygonInCountry(country);
        if (polygon.size() < 3)
            continue;

        auto *region = new MapFeature(FeatureKind::Region);
        region->setPolygon(polygon);
        region->name = tr("Region %1").arg(featureCount(FeatureKind::Region) + 1);
        region->countryName = country->name;
        region->color = country->color;
        m_scene->addItem(region);
        ++added;
    }
    if (added > 0)
        pushHistory();
    statusBar()->showMessage(added > 0 ? tr("Added %1 random regions.").arg(added)
                                       : tr("Draw a country first."),
                             2500);
}

void MainWindow::addRandomRegionsToSelectedCountry()
{
    if (!m_selected || m_selected->kind() != FeatureKind::Country) {
        QMessageBox::information(this, tr("Regions"),
                                 tr("Select a country first, then add regions to it."));
        return;
    }

    bool ok = false;
    const int count = QInputDialog::getInt(this, tr("Add Random Regions"),
                                           tr("How many regions?"), 5, 1, 100, 1, &ok);
    if (!ok)
        return;
    fillCountryWithRegions(m_selected, count);
}

void MainWindow::fillSelectedArea()
{
    if (!m_selected) {
        QMessageBox::information(this, tr("Fill Area"),
                                 tr("Select a country or land area first."));
        return;
    }

    if (m_selected->kind() == FeatureKind::Country) {
        addRandomRegionsToSelectedCountry();
    } else if (m_selected->kind() == FeatureKind::Land) {
        addRandomCountriesToSelectedLand();
    } else {
        QMessageBox::information(this, tr("Fill Area"),
                                 tr("Select a country or land area first."));
    }
}

void MainWindow::addRandomCountriesToSelectedLand()
{
    if (!m_selected || m_selected->kind() != FeatureKind::Land) {
        QMessageBox::information(this, tr("Countries"),
                                 tr("Select a land area first, then add countries to it."));
        return;
    }

    bool ok = false;
    const int count = QInputDialog::getInt(this, tr("Add Random Countries"),
                                           tr("How many countries?"), 5, 1, 100, 1, &ok);
    if (!ok)
        return;
    fillLandWithCountries(m_selected, count);
}

void MainWindow::fillCountryWithRegions(MapFeature *country, int count)
{
    const QString title = country->name.isEmpty() ? tr("this country") : country->name;

    QVector<QPolygonF> blocked;
    for (QGraphicsItem *item : m_scene->items()) {
        auto *feature = dynamic_cast<MapFeature *>(item);
        if (feature && feature->kind() == FeatureKind::Region
            && feature->countryName == country->name) {
            blocked.append(feature->polygon());
        }
    }

    QVector<QPolygonF> pieces = fillPieces(country->polygon(), blocked, count);
    if (pieces.isEmpty()) {
        statusBar()->showMessage(tr("%1 is already full of regions.").arg(title), 3000);
        return;
    }

    for (const QPolygonF &piece : pieces) {
        auto *region = new MapFeature(FeatureKind::Region);
        region->setPolygon(piece);
        region->name = tr("Region %1").arg(featureCount(FeatureKind::Region) + 1);
        region->countryName = country->name;
        region->color = country->color;
        m_scene->addItem(region);
    }

    pushHistory();
    statusBar()->showMessage(tr("Added %1 regions to %2.").arg(pieces.size()).arg(title), 3000);
}

void MainWindow::fillLandWithCountries(MapFeature *land, int count)
{
    const QString title = land->name.isEmpty() ? tr("this land") : land->name;

    QVector<QPolygonF> blocked;
    for (QGraphicsItem *item : m_scene->items()) {
        auto *feature = dynamic_cast<MapFeature *>(item);
        if (feature && feature->kind() == FeatureKind::Country)
            blocked.append(feature->polygon());
    }

    QVector<QPolygonF> pieces = fillPieces(land->polygon(), blocked, count);
    if (pieces.isEmpty()) {
        statusBar()->showMessage(tr("%1 is already full of countries.").arg(title), 3000);
        return;
    }

    for (const QPolygonF &piece : pieces) {
        auto *country = new MapFeature(FeatureKind::Country);
        country->setPolygon(piece);
        country->name = tr("Country %1").arg(featureCount(FeatureKind::Country) + 1);
        country->color = nextCountryColor();
        m_scene->addItem(country);
    }

    pushHistory();
    statusBar()->showMessage(tr("Added %1 countries to %2.").arg(pieces.size()).arg(title), 3000);
}

void MainWindow::addRandomTerrainAreas(int count, bool fill)
{
    QVector<MapFeature *> terrainAreas;
    for (QGraphicsItem *item : m_scene->items()) {
        auto *feature = dynamic_cast<MapFeature *>(item);
        if (feature && feature->kind() == FeatureKind::TerrainArea)
            terrainAreas.append(feature);
    }

    if (fill) {
        MapFeature *target = nullptr;
        MapFeature *land = nullptr;
        MapFeature *country = nullptr;
        if (m_selected
            && (m_selected->kind() == FeatureKind::Land
                || m_selected->kind() == FeatureKind::Country)) {
            target = m_selected;
        } else {
            for (QGraphicsItem *item : m_scene->items()) {
                auto *feature = dynamic_cast<MapFeature *>(item);
                if (!feature)
                    continue;
                if (feature->kind() == FeatureKind::Land && !land)
                    land = feature;
                else if (feature->kind() == FeatureKind::Country && !country)
                    country = feature;
            }
            target = land ? land : country;
        }
        if (!target) {
            statusBar()->showMessage(tr("Draw some land first."), 2500);
            return;
        }

        QVector<QPolygonF> blocked;
        for (MapFeature *area : terrainAreas)
            blocked.append(area->polygon());

        const QVector<QPolygonF> pieces = fillPieces(target->polygon(), blocked, count);
        if (pieces.isEmpty()) {
            statusBar()->showMessage(tr("The area is already full of terrain."), 2500);
            return;
        }

        for (const QPolygonF &piece : pieces) {
            auto *area = new MapFeature(FeatureKind::TerrainArea);
            area->setPolygon(piece);
            area->terrain = compatibleTerrain(piece, terrainAreas);
            m_scene->addItem(area);
            terrainAreas.append(area);
        }
        pushHistory();
        statusBar()->showMessage(tr("Filled the area with %1 terrain areas.").arg(pieces.size()),
                                 2500);
        return;
    }

    for (int i = 0; i < count; ++i) {
        const QPolygonF polygon =
            randomBlob(randomWorldPoint(), 50.0 + QRandomGenerator::global()->generateDouble() * 140.0);
        auto *area = new MapFeature(FeatureKind::TerrainArea);
        area->setPolygon(polygon);
        area->terrain = compatibleTerrain(polygon, terrainAreas);
        m_scene->addItem(area);
        terrainAreas.append(area);
    }
    pushHistory();
    statusBar()->showMessage(tr("Added %1 random terrain areas.").arg(count), 2500);
}

void MainWindow::addRandomSettlements(int count, bool fill)
{
    Q_UNUSED(fill);
    for (int i = 0; i < count; ++i) {
        const auto type = static_cast<SettlementType>(QRandomGenerator::global()->bounded(3));
        const QString typeName = settlementName(type);

        int sameType = 0;
        for (QGraphicsItem *item : m_scene->items()) {
            auto *feature = dynamic_cast<MapFeature *>(item);
            if (feature && feature->kind() == FeatureKind::Settlement && feature->settlement == type)
                ++sameType;
        }

        auto *settlement = new MapFeature(FeatureKind::Settlement);
        settlement->setPoint(randomWorldPoint());
        settlement->settlement = type;
        settlement->name = QStringLiteral("%1 %2").arg(typeName).arg(sameType + 1);
        m_scene->addItem(settlement);
    }
    pushHistory();
    statusBar()->showMessage(tr("Added %1 random settlements.").arg(count), 2500);
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

    m_selectAction = new QAction(tr("Select / Move"), this);
    m_selectAction->setCheckable(true);
    m_selectAction->setChecked(true);
    m_selectAction->setShortcut(Qt::Key_M);
    connect(m_selectAction, &QAction::triggered, this, [this] {
        setMode(MapView::Mode::Edit);
        m_view->setTool(MapView::Tool::None);
        statusBar()->showMessage(tr("Select an item or a control point and drag it. Esc cancels a drag."));
    });
    toolGroup->addAction(m_selectAction);

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
        statusBar()->showMessage(tr("Click on the map to place the settlement type chosen on the left."));
    });
    toolGroup->addAction(m_settlementAction);

    m_roadAction = new QAction(tr("Road"), this);
    m_roadAction->setCheckable(true);
    m_roadAction->setShortcut(Qt::Key_D);
    connect(m_roadAction, &QAction::triggered, this, [this] {
        setMode(MapView::Mode::Edit);
        m_view->setTool(MapView::Tool::Road);
        statusBar()->showMessage(tr("Draw a road: click points, double-click or Enter to finish. Terrain affects its cost."));
    });
    toolGroup->addAction(m_roadAction);

    m_railroadAction = new QAction(tr("Railroad"), this);
    m_railroadAction->setCheckable(true);
    m_railroadAction->setShortcut(Qt::Key_I);
    connect(m_railroadAction, &QAction::triggered, this, [this] {
        setMode(MapView::Mode::Edit);
        m_view->setTool(MapView::Tool::Railroad);
        statusBar()->showMessage(tr("Draw a railroad: click points, double-click or Enter to finish."));
    });
    toolGroup->addAction(m_railroadAction);

    m_riverAction = new QAction(tr("River"), this);
    m_riverAction->setCheckable(true);
    m_riverAction->setShortcut(Qt::Key_V);
    connect(m_riverAction, &QAction::triggered, this, [this] {
        setMode(MapView::Mode::Edit);
        m_view->setTool(MapView::Tool::River);
        statusBar()->showMessage(tr("Draw a river: click points, double-click or Enter to finish. Roads crossing it get bridges."));
    });
    toolGroup->addAction(m_riverAction);

    m_terrainCombo = new QComboBox(this);
    m_terrainCombo->setToolTip(tr("Terrain painted by the terrain tool"));
    for (TerrainType terrain : terrainTypes()) {
        QPixmap swatch(14, 14);
        swatch.fill(terrainColor(terrain));
        m_terrainCombo->addItem(QIcon(swatch), terrainName(terrain), static_cast<int>(terrain));
    }

    m_settlementCombo = new QComboBox(this);
    m_settlementCombo->setToolTip(tr("Settlement type placed by the settlement tool"));
    for (SettlementType type : {SettlementType::City, SettlementType::Town, SettlementType::Village})
        m_settlementCombo->addItem(settlementName(type), static_cast<int>(type));

    auto *fitAction = new QAction(tr("Fit World"), this);
    fitAction->setShortcut(Qt::Key_F);
    connect(fitAction, &QAction::triggered, this, [this] { m_view->fitWorld(); });

    m_undoAction = new QAction(tr("Undo"), this);
    m_undoAction->setShortcut(QKeySequence::Undo);
    m_undoAction->setEnabled(false);
    connect(m_undoAction, &QAction::triggered, this, &MainWindow::undo);

    m_redoAction = new QAction(tr("Redo"), this);
    m_redoAction->setShortcut(QKeySequence::Redo);
    m_redoAction->setEnabled(false);
    connect(m_redoAction, &QAction::triggered, this, &MainWindow::redo);

    auto *deleteAction = new QAction(tr("Delete Selected Item..."), this);
    connect(deleteAction, &QAction::triggered, this, &MainWindow::deleteSelectedFeature);

    auto *globalListsAction = new QAction(tr("Languages and Characteristics..."), this);
    connect(globalListsAction, &QAction::triggered, this, &MainWindow::editGlobalLists);

    auto *zoomInAction = new QAction(tr("Zoom In"), this);
    zoomInAction->setShortcut(QKeySequence::ZoomIn);
    connect(zoomInAction, &QAction::triggered, this, [this] { m_view->zoomBy(1.25); });

    auto *zoomOutAction = new QAction(tr("Zoom Out"), this);
    zoomOutAction->setShortcut(QKeySequence::ZoomOut);
    connect(zoomOutAction, &QAction::triggered, this, [this] { m_view->zoomBy(0.8); });

    auto *newAction = new QAction(tr("New World..."), this);
    newAction->setShortcut(QKeySequence::New);
    connect(newAction, &QAction::triggered, this, &MainWindow::chooseNewWorld);

    auto *saveAction = new QAction(tr("Save"), this);
    saveAction->setShortcut(QKeySequence::Save);
    connect(saveAction, &QAction::triggered, this, &MainWindow::saveWorld);

    auto *saveAsAction = new QAction(tr("Save As..."), this);
    saveAsAction->setShortcut(QKeySequence::SaveAs);
    connect(saveAsAction, &QAction::triggered, this, &MainWindow::saveWorldAs);

    auto *openAction = new QAction(tr("Open World..."), this);
    openAction->setShortcut(QKeySequence::Open);
    connect(openAction, &QAction::triggered, this, &MainWindow::loadWorld);

    auto *exportLogAction = new QAction(tr("Export Travel Log..."), this);
    connect(exportLogAction, &QAction::triggered, this, &MainWindow::exportTravelLog);

    auto *resizeAction = new QAction(tr("Resize World..."), this);
    connect(resizeAction, &QAction::triggered, this, &MainWindow::resizeWorld);

    auto *randomAction = new QAction(tr("Random Additions..."), this);
    connect(randomAction, &QAction::triggered, this, &MainWindow::showRandomDialog);

    auto *fillRegionsAction = new QAction(tr("Fill Selected Area..."), this);
    connect(fillRegionsAction, &QAction::triggered, this,
            &MainWindow::fillSelectedArea);

    auto *quitAction = new QAction(tr("Quit"), this);
    quitAction->setShortcut(QKeySequence::Quit);
    connect(quitAction, &QAction::triggered, this, &QWidget::close);

    auto *fileMenu = menuBar()->addMenu(tr("&File"));
    fileMenu->addAction(newAction);
    fileMenu->addAction(openAction);
    fileMenu->addSeparator();
    fileMenu->addAction(saveAction);
    fileMenu->addAction(saveAsAction);
    fileMenu->addSeparator();
    fileMenu->addAction(exportLogAction);
    fileMenu->addSeparator();
    fileMenu->addAction(resizeAction);
    fileMenu->addSeparator();
    fileMenu->addAction(quitAction);

    auto *modeMenu = menuBar()->addMenu(tr("&Mode"));
    modeMenu->addAction(m_exploreAction);
    modeMenu->addAction(m_editAction);

    auto *editMenu = menuBar()->addMenu(tr("&Edit"));
    editMenu->addAction(m_undoAction);
    editMenu->addAction(m_redoAction);
    editMenu->addSeparator();
    editMenu->addAction(deleteAction);
    editMenu->addSeparator();
    editMenu->addAction(globalListsAction);

    auto *generateMenu = menuBar()->addMenu(tr("&Generate"));
    generateMenu->addAction(randomAction);
    generateMenu->addAction(fillRegionsAction);

    auto *viewMenu = menuBar()->addMenu(tr("&View"));
    m_viewMenu = viewMenu;
    viewMenu->addAction(zoomInAction);
    viewMenu->addAction(zoomOutAction);
    viewMenu->addAction(fitAction);

    auto *toolbar = addToolBar(tr("Main"));
    toolbar->setMovable(false);
    toolbar->addAction(newAction);
    toolbar->addAction(openAction);
    toolbar->addAction(saveAction);
    toolbar->addSeparator();
    toolbar->addAction(m_exploreAction);
    toolbar->addAction(m_editAction);
    toolbar->addSeparator();
    toolbar->addAction(zoomOutAction);
    toolbar->addAction(fitAction);
    toolbar->addAction(zoomInAction);

    auto *toolPalette = new QToolBar(tr("Tools"), this);
    toolPalette->setObjectName(QStringLiteral("toolPalette"));
    toolPalette->setOrientation(Qt::Vertical);
    toolPalette->setMovable(false);
    toolPalette->setToolButtonStyle(Qt::ToolButtonTextOnly);
    toolPalette->addAction(m_selectAction);
    toolPalette->addAction(m_landAction);
    toolPalette->addAction(m_countryAction);
    toolPalette->addAction(m_terrainAction);
    toolPalette->addAction(m_regionAction);
    toolPalette->addAction(m_settlementAction);
    toolPalette->addAction(m_roadAction);
    toolPalette->addAction(m_railroadAction);
    toolPalette->addAction(m_riverAction);
    toolPalette->addSeparator();
    toolPalette->addWidget(new QLabel(tr(" Terrain "), toolPalette));
    toolPalette->addWidget(m_terrainCombo);
    toolPalette->addWidget(new QLabel(tr(" Settlement "), toolPalette));
    toolPalette->addWidget(m_settlementCombo);
    addToolBar(Qt::LeftToolBarArea, toolPalette);
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
    m_travelOrigin = nullptr;
    resetGlobalLists();
    m_view->setTool(m_view->tool());
    m_scene->clear();
    m_scene->setSceneRect(QRectF(QPointF(0, 0), m_worldSize));
    m_view->setWorldSize(m_worldSize);

    if (land)
        addBigLand();

    m_view->fitWorld();
    m_currentFile.clear();
    updateTitle();
    resetHistory();
}

MapFeature *MainWindow::addBigLand()
{
    const double w = m_worldSize.width();
    const double h = m_worldSize.height();

    // An irregular coastline rather than a rounded rectangle.
    const QPolygonF shape{
        QPointF(0.08, 0.20), QPointF(0.16, 0.10),
        QPointF(0.31, 0.06), QPointF(0.43, 0.13),
        QPointF(0.57, 0.07), QPointF(0.73, 0.10),
        QPointF(0.88, 0.19), QPointF(0.92, 0.35),
        QPointF(0.85, 0.48), QPointF(0.93, 0.61),
        QPointF(0.89, 0.77), QPointF(0.76, 0.90),
        QPointF(0.59, 0.93), QPointF(0.43, 0.87),
        QPointF(0.29, 0.94), QPointF(0.14, 0.86),
        QPointF(0.06, 0.71), QPointF(0.12, 0.55),
        QPointF(0.05, 0.38),
    };

    QPolygonF coastline;
    coastline.reserve(shape.size());
    constexpr double spread = 1.07;
    for (const QPointF &point : shape) {
        coastline.append(QPointF((0.5 + (point.x() - 0.5) * spread) * w,
                                 (0.5 + (point.y() - 0.5) * spread) * h));
    }

    auto *land = new MapFeature(FeatureKind::Land);
    land->setPolygon(coastline);
    m_scene->addItem(land);
    return land;
}

void MainWindow::addLand(const QPolygonF &polygon)
{
    QPainterPath drawn;
    drawn.addPolygon(polygon);
    drawn.closeSubpath();

    QPainterPath merged = drawn;
    QVector<MapFeature *> absorbed;
    QString name;
    for (QGraphicsItem *item : m_scene->items()) {
        auto *feature = dynamic_cast<MapFeature *>(item);
        if (!feature || feature->kind() != FeatureKind::Land)
            continue;

        QPainterPath path;
        path.addPolygon(feature->polygon());
        path.closeSubpath();
        if (!path.intersects(drawn))
            continue;

        merged = merged.united(path);
        absorbed.append(feature);
        if (name.isEmpty() && !feature->name.isEmpty())
            name = feature->name;
    }

    const QPolygonF outline = absorbed.isEmpty() ? polygon : merged.toFillPolygon();
    for (MapFeature *feature : absorbed)
        delete feature;

    auto *land = new MapFeature(FeatureKind::Land);
    land->setPolygon(outline);
    land->name = name;
    m_scene->addItem(land);
    pushHistory();
    statusBar()->showMessage(absorbed.isEmpty() ? tr("Land added.") : tr("Land merged."), 2000);
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
    country->setPolygon(clipToLand(polygon));
    country->name = name;
    country->color = nextCountryColor();
    m_scene->addItem(country);
    pushHistory();
    statusBar()->showMessage(tr("%1 created.").arg(name), 2000);
}

void MainWindow::addTerrainArea(const QPolygonF &polygon)
{
    auto *area = new MapFeature(FeatureKind::TerrainArea);
    area->setPolygon(polygon);
    area->terrain = currentTerrain();
    m_scene->addItem(area);
    pushHistory();
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

    QPainterPath countryPath;
    countryPath.addPolygon(country->polygon());
    countryPath.closeSubpath();

    QPainterPath regionPath;
    regionPath.addPolygon(polygon);
    regionPath.closeSubpath();

    const QPolygonF clipped = regionPath.intersected(countryPath).toFillPolygon();
    if (clipped.size() < 3) {
        QMessageBox::information(this, tr("Region"), tr("The region must overlap its country."));
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
    region->setPolygon(clipped);
    region->name = name;
    region->countryName = country->name;
    region->color = country->color;
    m_scene->addItem(region);
    pushHistory();
    statusBar()->showMessage(tr("%1 added to %2.").arg(name, country->name), 2000);
}

void MainWindow::addRoad(const QPolygonF &polygon)
{
    auto *road = new MapFeature(FeatureKind::Road);
    road->setPolygon(polygon);
    road->name = tr("Road %1").arg(featureCount(FeatureKind::Road) + 1);
    m_scene->addItem(road);
    pushHistory();

    double length = 0.0;
    for (int i = 0; i + 1 < polygon.size(); ++i)
        length += QLineF(polygon.at(i), polygon.at(i + 1)).length();
    statusBar()->showMessage(tr("Road added: %1 px, travel cost %2 (terrain weighted).")
                                 .arg(qRound(length))
                                 .arg(qRound(roadTravelCost(polygon))),
                             3000);
}

void MainWindow::addRailroad(const QPolygonF &polygon)
{
    auto *railroad = new MapFeature(FeatureKind::Railroad);
    railroad->setPolygon(polygon);
    railroad->name = tr("Railroad %1").arg(featureCount(FeatureKind::Railroad) + 1);
    m_scene->addItem(railroad);
    pushHistory();

    double length = 0.0;
    for (int i = 0; i + 1 < polygon.size(); ++i)
        length += QLineF(polygon.at(i), polygon.at(i + 1)).length();
    statusBar()->showMessage(tr("Railroad added: %1 px.").arg(qRound(length)), 2500);
}

void MainWindow::addRiver(const QPolygonF &polygon)
{
    auto *river = new MapFeature(FeatureKind::River);
    river->setPolygon(polygon);
    river->name = tr("River %1").arg(featureCount(FeatureKind::River) + 1);
    m_scene->addItem(river);
    pushHistory();
    statusBar()->showMessage(tr("River added. Roads and railroads crossing it are drawn with bridges."), 2500);
}

TerrainType MainWindow::terrainAt(const QPointF &point) const
{
    const QList<QGraphicsItem *> items = m_scene->items(point);
    for (QGraphicsItem *item : items) {
        auto *feature = dynamic_cast<MapFeature *>(item);
        if (!feature || !feature->isVisible())
            continue;
        if (feature->kind() == FeatureKind::TerrainArea)
            return feature->terrain;
        if (feature->kind() == FeatureKind::Land)
            return TerrainType::Grass;
    }
    return TerrainType::Water;
}

double MainWindow::roadTravelCost(const QPolygonF &polygon) const
{
    double cost = 0.0;
    for (int i = 0; i + 1 < polygon.size(); ++i)
        cost += lineTravelCost(polygon.at(i), polygon.at(i + 1));
    return cost;
}

double MainWindow::lineTravelCost(const QPointF &from, const QPointF &to) const
{
    const double length = QLineF(from, to).length();
    if (length <= 0.0)
        return 0.0;

    const int steps = qMax(1, int(std::ceil(length / 12.0)));
    const double stepLength = length / steps;
    double cost = 0.0;
    for (int step = 0; step < steps; ++step) {
        const QPointF sample = from + (to - from) * ((step + 0.5) / steps);
        cost += stepLength * terrainTravelCost(terrainAt(sample));
    }
    return cost;
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

QPolygonF MainWindow::clipToLand(const QPolygonF &polygon) const
{
    if (polygon.size() < 3)
        return polygon;

    QPainterPath land;
    bool hasLand = false;
    for (QGraphicsItem *item : m_scene->items()) {
        auto *feature = dynamic_cast<MapFeature *>(item);
        if (!feature || feature->kind() != FeatureKind::Land)
            continue;
        land.addPolygon(feature->polygon());
        land.closeSubpath();
        hasLand = true;
    }
    if (!hasLand)
        return polygon;

    QPainterPath shape;
    shape.addPolygon(polygon);
    shape.closeSubpath();
    if (!shape.intersects(land))
        return polygon;

    const QPolygonF clipped = shape.intersected(land).toFillPolygon();
    return clipped.size() >= 3 ? clipped : polygon;
}

void MainWindow::addSettlement(const QPointF &point)
{
    SettlementType type = SettlementType::City;
    if (m_settlementCombo) {
        const int value = m_settlementCombo->currentData().toInt();
        if (value >= static_cast<int>(SettlementType::City)
            && value <= static_cast<int>(SettlementType::Village)) {
            type = static_cast<SettlementType>(value);
        }
    }

    const QString typeName = settlementName(type);

    int sameType = 0;
    for (QGraphicsItem *item : m_scene->items()) {
        auto *feature = dynamic_cast<MapFeature *>(item);
        if (feature && feature->kind() == FeatureKind::Settlement && feature->settlement == type)
            ++sameType;
    }

    auto *settlement = new MapFeature(FeatureKind::Settlement);
    settlement->setPoint(point);
    settlement->settlement = type;
    settlement->name = QStringLiteral("%1 %2").arg(typeName).arg(sameType + 1);
    m_scene->addItem(settlement);
    pushHistory();
    statusBar()->showMessage(tr("%1 placed. Rename it in the Info dock.").arg(settlement->name),
                             2500);
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
