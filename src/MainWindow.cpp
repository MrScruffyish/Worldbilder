#include "MainWindow.h"

#include "MapFeature.h"

#include <QAction>
#include <QActionGroup>
#include <QGraphicsScene>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPainterPath>
#include <QStatusBar>
#include <QTimer>
#include <QToolBar>

#include <QPushButton>

namespace {
const QColor kSeaColor(0x3d, 0x6a, 0x93);
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

    connect(m_view, &MapView::zoomChanged, this, [this](double scale) {
        m_zoomLabel->setText(QStringLiteral("%1%").arg(qRound(scale * 100)));
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

    auto *fitAction = new QAction(tr("Fit World"), this);
    fitAction->setShortcut(Qt::Key_F);
    connect(fitAction, &QAction::triggered, this, [this] { m_view->fitWorld(); });

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
    viewMenu->addAction(fitAction);

    auto *toolbar = addToolBar(tr("Main"));
    toolbar->setMovable(false);
    toolbar->addAction(newAction);
    toolbar->addSeparator();
    toolbar->addAction(m_exploreAction);
    toolbar->addAction(m_editAction);
    toolbar->addSeparator();
    toolbar->addAction(fitAction);
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

void MainWindow::setMode(MapView::Mode mode)
{
    m_view->setMode(mode);
    if (mode == MapView::Mode::Explore) {
        m_exploreAction->setChecked(true);
        statusBar()->showMessage(tr("Explore: drag to pan, wheel to zoom."));
    } else {
        m_editAction->setChecked(true);
        statusBar()->showMessage(tr("Edit mode."));
    }
}
