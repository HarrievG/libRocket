#include <QApplication>
#include <QMainWindow>
#include <QMessageBox>
#include <QFileSystemWatcher>
#include <QFileDialog>
#include <QGridLayout>
#include <QLabel>
#include <QCheckBox>
#include <QComboBox>
#include <QStatusBar>
#include <QToolBar>
#include <QMenuBar>
#include <QMenu>
#include <QDebug>

#include "Rocket/Debugger.h"

#include "qtrocketsystem.h"
#include "qtrocketfileinterface.h"
#include "mainwindow.h"
#include "documents.h"
#include "settings.h"
#include "tools.h"
#include "renderingview.h"
#include "qlogwidgets.h"


struct LocalScreenSizeItem
{
    LocalScreenSizeItem() : width(0), height(0)
    {
    }

    LocalScreenSizeItem(const int _width, const int _height, const char *text=NULL)
    {
        width = _width;
        height = _height;
        displayedString = QString::number(width) + "x" + QString::number(height);
        displayedTwoString = displayedString;

        if (text)
        {
            labelString = text;
            displayedString += " (";
            displayedString += text;
            displayedString += ")";
            displayedTwoString += "\n(";
            displayedTwoString += text;
            displayedTwoString += ")";
        }
    }

    QString labelString;
    QString displayedString;
    QString displayedTwoString; // two line label
    int width;
    int height;
};

// available frames:
struct TestFrameInfo {
    const char *image;
    bool tool, toolbar;
    LocalScreenSizeItem size;
} testFrames[] = {
    { ":/res/frame-android-xsmall.png", true, false, LocalScreenSizeItem(320, 480, "Android") },
    { ":/res/frame-android-small.png", true, false, LocalScreenSizeItem(480, 640, "Android VGA") },
    { ":/res/frame-android-medium.png", true, false, LocalScreenSizeItem(480, 854, "Android FWVGA") },
    { ":/res/frame-iphone.png", true, false, LocalScreenSizeItem(320, 480, "iPhone3") },
    { ":/res/frame-monitor.png", false, true, LocalScreenSizeItem(800, 480, "WVGA ") },
    { ":/res/frame-monitor.png", true, false, LocalScreenSizeItem(800, 600) },
    { ":/res/frame-iphone.png", true, true, LocalScreenSizeItem(640, 960, "iPhone4") },
    { ":/res/frame-iphone5.png", true, true, LocalScreenSizeItem(1136, 640, "iPhone5") },
    { ":/res/frame-ipad.png", true, true, LocalScreenSizeItem(768, 1024, "iPad") },
    { ":/res/frame-monitor.png", false, false, LocalScreenSizeItem(1024, 768) },
    { ":/res/frame-playbook.png", true, true, LocalScreenSizeItem(1024, 600, "Playbook") },
    { ":/res/frame-z10.png", true, true, LocalScreenSizeItem(768, 1280, "BB Z10") },
    { ":/res/frame-monitor.png", false, false, LocalScreenSizeItem(1280, 720) },
    { ":/res/frame-monitor.png", false, false, LocalScreenSizeItem(1600, 1024) },
    { ":/res/frame-monitor.png", true, false, LocalScreenSizeItem(1920, 1080, "HD") },
    { ":/res/frame-ipad.png", true, false, LocalScreenSizeItem(1536, 2048, "iPad3") },
    { NULL }
};

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent)
{
    instance = this;

    log = new QLogTable(3);

    addLogMsg(QString("Qt;%1").arg(qVersion()));

    renderingView = new RenderingView(this);
    connect(renderingView, SIGNAL(emitMouseSize(QPoint)), SLOT(setMousePos(QPoint)));
    connect(renderingView, SIGNAL(emitLogMsg(QString)), SLOT(addLogMsg(QString)));
    connect(&RocketSystem::getInstance(), SIGNAL(emitContextSize(QSize)), SLOT(setContextSize(QSize)));

    toolbar = addToolBar("Main");
    toolbar->setIconSize(QSize(16,16));

    setStatusBar(new QStatusBar(this));
    statusBar()->setAttribute(Qt::WA_MacSmallSize);
    labelZoom = new QLabel;
    labelMousePos = new QLabel;
    labelScreenSize = new QLabel;
    statusBar()->addPermanentWidget(labelZoom);
    statusBar()->addPermanentWidget(labelMousePos);
    statusBar()->addPermanentWidget(labelScreenSize);

    ToolManager::getInstance().initialize();

    setupFileMenu();
    setupToolsMenu();

    // quick resolution change:
    QMenu *dimMenu = new QMenu(tr("Dimension"));
    menuBar()->addMenu(dimMenu);

    QComboBox *dimCombo = new QComboBox; dimCombo->setAttribute(Qt::WA_MacSmallSize);
    TestFrameInfo *e = &testFrames[0]; int x = 0; while(e->image) {
        QAction *res_action = new QAction(QIcon(e->image), e->size.displayedString, this);
        res_action->setProperty("index", x);
        connect(res_action, SIGNAL(triggered()), (QObject*)this, SLOT(newScreenSizeAction()));
        if (e->toolbar)
            toolbar->addAction(res_action);
        dimMenu->addAction(res_action);
        dimCombo->addItem(QIcon(e->image), e->size.displayedString, (quintptr)res_action);
        ++e; ++x;
    }
    toolbar->addWidget(dimCombo);
    QCheckBox *fitWin = new QCheckBox(tr("Follow window size")); fitWin->setAttribute(Qt::WA_MacSmallSize);
    connect(fitWin, &QCheckBox::stateChanged, [=](int state){
        renderingView->setAutoResize(state);
    });
    toolbar->addWidget(fitWin);
    toolbar->addSeparator();

    int orient = Settings::getInt("Rocket/ScreenOrient", 0);

    QAction *portrait_action = new QAction(QIcon(":/res/orientation-portrait.png"), "Portrait", this);
    portrait_action->setProperty("orientation", 0);
    portrait_action->setCheckable(true);
    if (orient == 0) portrait_action->setChecked( true );
    QAction *landscape_action = new QAction(QIcon(":/res/orientation-landscapeleft.png"), "Landscape", this);
    landscape_action->setProperty("orientation", 1);
    landscape_action->setCheckable(true);
    if (orient == 1) landscape_action->setChecked( true );
    QActionGroup * group = new QActionGroup( this );
    group->addAction(portrait_action);
    group->addAction(landscape_action);
    connect(group, SIGNAL(triggered(QAction*)), this, SLOT(orientationChange(QAction*)));
    toolbar->addActions(group->actions());
    toolbar->addSeparator();

    setupHelpMenu();
    buildCentralWidget();

    connect(&RocketSystem::getInstance(), &RocketSystem::emitRocketEvent, [](Rocket::Core::Event &event){
        ToolManager::getInstance().getCurrentTool()->onElementClicked(event.GetTargetElement());
    });

    setWindowTitle(tr("RML Viewer"));

    setZoomLevel(1);
    setMousePos(QPoint(0,0));

    resize(appSettings->value("MainWindow/size", QSize(800, 600)).toSize());
    move(appSettings->value("MainWindow/position", QPoint(200, 200)).toPoint());
    setWindowState((Qt::WindowStates)appSettings->value("MainWindow/windowState", Qt::WindowActive).toInt());
    restoreState(appSettings->value("MainWindow/widgetsState").toByteArray());

    fileWatcher = new QFileSystemWatcher();

    connect(fileWatcher, SIGNAL(fileChanged(const QString &)), (QObject*)this, SLOT(fileHasChanged(const QString &)));
}

void MainWindow::buildCentralWidget()
{
    QWidget *w = new QWidget;
    QGridLayout *layout = new QGridLayout;

    layout->setSpacing(0);
    layout->setMargin(0);

    horzRuler = new QDRuler(QDRuler::Horizontal);
    vertRuler = new QDRuler(QDRuler::Vertical);

    QWidget* fake = new QWidget();
    fake->setBackgroundRole(QPalette::Window);
    fake->setFixedSize(RULER_BREADTH, RULER_BREADTH);
    layout->addWidget(fake,0,0);
    layout->addWidget(horzRuler,0,1);
    layout->addWidget(vertRuler,1,0);
    layout->addWidget(renderingView,1,1);

    w->setLayout(layout);

    QWidget *central = new QWidget;
    QVBoxLayout *v = new QVBoxLayout;
    v->setSpacing(0);
    v->setContentsMargins(0, 0, 0, 0);
    v->addWidget(w); v->setStretchFactor(w, 1);
    v->addWidget(log);
    central->setLayout(v);

    setCentralWidget(central);
}

OpenedDocument *MainWindow::getCurrentDocument() { return renderingView->getCurrentDocument(); }
void MainWindow::repaintRenderingView() { return renderingView->update(); }

void MainWindow::selectElement(Element *element)
{
    //if(element != getCurrentDocument()->selectedElement)

    if (element && getCurrentDocument()) {
        // skip debugger elements
        if (Rocket::Debugger::IsVisible() && (
            (element->GetId() == "event-log-button")
            || (element->GetId() == "debug-info-button")
            || (element->GetId() == "outlines-button")
        ))
            return;
        getCurrentDocument()->selectedElement = element;
        repaintRenderingView();
    }
}

void MainWindow::unselectElement()
{
    if (!getCurrentDocument())
        return;

    getCurrentDocument()->selectedElement = NULL;
    ToolManager::getInstance().getCurrentTool()->onUnselect();
    repaintRenderingView();
}

void MainWindow::reloadCurrentDocument()
{
    if (getCurrentDocument())
    {
        renderingView->reloadDocument();
    }
}

void MainWindow::fileHasChanged(const QString &path)
{
    QFileInfo file_info = path;
    reloadCurrentDocument();
}

void MainWindow::resetZoom()
{
    renderingView->zoomReset();
}

void MainWindow::setZoomLevel(float level)
{
    QString string_level;

    string_level.setNum(level * 100.0f);
    string_level += "%";

    labelZoom->setText("Zoom: "+string_level);

    horzRuler->setRulerZoom(level);
    vertRuler->setRulerZoom(level);
}

void MainWindow::setMousePos(QPoint pos)
{
    labelMousePos->setText(QString("Pos: %1x%2").arg(pos.x()).arg(pos.y()));
}

void MainWindow::setContextSize(QSize context)
{
    labelScreenSize->setText(QString("Screen: %1x%2").arg(context.width()).arg(context.height()));
}

void MainWindow::addLogMsg(QString msg)
{
    QStringList list;
    list
        << QDateTime::currentDateTime().toString("dd/MM/yyyy hh:mm:ss")
        << msg.split(';');

    log->appendRow(list);
}

void MainWindow::about()
{
    QMessageBox::about(this, tr("About RML Viewer"),
                tr("<p>The <b>RML Viewer</b> preview RML UI files for <i>libRocket</i> (<a href='https://github.com/ppiecuch/libRocket'>https://github.com/ppiecuch/libRocket</a>).</p>"));
}

void MainWindow::openFile(QString path)
{
    QString fileName = path;

    if (fileName.isNull())
        fileName = QFileDialog::getOpenFileName(this, tr("Open File"), "", "C++ Files (*.cpp *.h)");

    if (!fileName.isEmpty()) {
        openDocument(fileName);
    }
}

bool MainWindow::openDocument()
{
    return openDocument(filePath);
}

bool MainWindow::openDocument(QString file_path)
{
    OpenedDocument *new_document = 0;
    QFileInfo file_info(file_path);

    if (!file_info.exists())
    {
        QMessageBox::information(NULL, "File not found", "File "+QString(file_path)+" not found.", QMessageBox::Ok);
        return false;
    }

    qInfo() << "Open document" << file_path;

    RocketFileInterface::AddToSearchPaths(file_info.path());
    RocketFileInterface::AddToSearchPaths(file_info.dir().filePath("fonts/"));
    RocketFileInterface::AddToSearchPaths(file_info.dir().filePath("../fonts/"));
    RocketFileInterface::AddToSearchPaths(file_info.dir().filePath("textures/"));
    RocketFileInterface::AddToSearchPaths(file_info.dir().filePath("../textures/"));

    foreach( QString path, RocketFileInterface::GetSearchPaths())
    {
        RocketSystem::getInstance().loadFonts(path);
    }

    new_document = new OpenedDocument(file_info);
    new_document->rocketDocument = RocketHelper::loadDocumentFromMemory(new_document->toPlainText());
    new_document->rocketDocument->RemoveReference();

    renderingView->changeCurrentDocument(new_document);

    fileWatcher->addPath(file_info.filePath());

    return true;
}

void MainWindow::orientationChange(QAction *action)
{
    const int index = action->property("orientation").toInt();
    const int curr = Settings::getInt("Rocket/ScreenOrient");
    // reload document if orientation changed
    if (index != curr)
        setScreenSize(RocketSystem::getInstance().context_width(), RocketSystem::getInstance().context_height(), index);
    Settings::setValue("ScreenSizeOrient", index);
}

void MainWindow::newScreenSizeAction()
{
    // get preview index and resize view:
    const QObject *source = QObject::sender();
    const int index = source->property("index").toInt();
    setScreenSize(testFrames[index].size.width, testFrames[index].size.height);
}

void MainWindow::setScreenSize(int width, int height)
{
    setScreenSize(width, height, Settings::getInt("Rocket/ScreenOrient"));
}

void MainWindow::setScreenSize(int width, int height, int orientation)
{
    if (orientation == 1) { // swap for landscape landscape
        if (height > width) {
            int t = width; width = height; height = t;
        }
    } else { // swap for portrait
        if (width > height) {
            int t = width; width = height; height = t;
        }
    };
    if (RocketSystem::getInstance().context_width() != width || RocketSystem::getInstance().context_height() != height)
    {
        RocketSystem::getInstance().resizeContext(width, height);

        if(getCurrentDocument())
        {
            reloadCurrentDocument();
        }
        Settings::setValue("Rocket/ScreenWidth", width);
        Settings::setValue("Rocket/ScreenHeight", height);

        statusBar()->showMessage(QString("Screen: %1x%2").arg(width).arg(height), 2500);

        repaintRenderingView();
    }
}

void MainWindow::setupFileMenu()
{
    QMenu *fileMenu = new QMenu(tr("&File"), this);
    menuBar()->addMenu(fileMenu);

    toolbar->addAction(fileMenu->addAction(QIcon(":/res/reload.png"), tr("&Reload document"), this, SLOT(reloadCurrentDocument())));
    toolbar->addSeparator();
}

void MainWindow::setupToolsMenu()
{
    QMenu *toolsMenu = new QMenu(tr("&Tools"), this);
    menuBar()->addMenu(toolsMenu);

    ToolManager::getInstance().setup(toolbar, toolsMenu);

    toolbar->addAction(toolsMenu->addAction(QIcon(":/res/zoom1.png"), tr("&View 1:1"), this, SLOT(resetZoom())));
    toolbar->addSeparator();
}

void MainWindow::setupHelpMenu()
{
    QMenu *helpMenu = new QMenu(tr("&Help"), this);
    menuBar()->addMenu(helpMenu);

    helpMenu->addAction(tr("&About"), this, SLOT(about()));
    helpMenu->addAction(tr("About &Qt"), qApp, SLOT(aboutQt()));
}

void MainWindow::keyPressEvent(QKeyEvent* event) {
    switch(event->key()) {
        case Qt::Key_Escape: log->setVisible(log->isHidden()); break;
        default: event->ignore();
        break;
    }
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    appSettings->setValue("MainWindow/size", size());
    appSettings->setValue("MainWindow/position", pos());
    appSettings->setValue("MainWindow/windowState", (int)windowState());
    appSettings->setValue("MainWindow/widgetsState", saveState());
}

MainWindow *MainWindow::instance;
