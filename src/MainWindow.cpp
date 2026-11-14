#include "MainWindow.h"

#include <QAction>
#include <QActionGroup>
#include <QGraphicsScene>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QStatusBar>
#include <QTimer>
#include <QToolBar>

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

    auto *quitAction = new QAction(tr("Quit"), this);
    quitAction->setShortcut(QKeySequence::Quit);
    connect(quitAction, &QAction::triggered, this, &QWidget::close);

    auto *fileMenu = menuBar()->addMenu(tr("&File"));
    fileMenu->addAction(quitAction);

    auto *modeMenu = menuBar()->addMenu(tr("&Mode"));
    modeMenu->addAction(m_exploreAction);
    modeMenu->addAction(m_editAction);

    auto *viewMenu = menuBar()->addMenu(tr("&View"));
    viewMenu->addAction(fitAction);

    auto *toolbar = addToolBar(tr("Main"));
    toolbar->setMovable(false);
    toolbar->addAction(m_exploreAction);
    toolbar->addAction(m_editAction);
    toolbar->addSeparator();
    toolbar->addAction(fitAction);
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
