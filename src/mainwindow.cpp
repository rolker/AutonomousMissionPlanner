#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QFileDialog>
#include <QStandardItemModel>
#include <gdal_priv.h>
#include <cstdint>
#include <QOpenGLWidget>

#include "autonomousvehicleproject.h"
#include "waypoint.h"

#include "roslink.h"

#include <modeltest.h>
#include "backgroundraster.h"
#include "trackline.h"
#include "surveypattern.h"
#include "surveyarea.h"

#include "ais/ais_manager.h"
#include "sound_play/sound_play_widget.h"
#include "sound_play/speech_alerts.h"

#include <QDebug>

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    GDALAllRegister();
    project = new AutonomousVehicleProject(this);
    
    new ModelTest(project,this);

    ui->treeView->setModel(project);
    ui->projectView->setStatusBar(statusBar());
    ui->projectView->setProject(project);

    ui->detailsView->setProject(project);
    connect(ui->treeView->selectionModel(),&QItemSelectionModel::currentChanged,ui->detailsView,&DetailsView::onCurrentItemChanged);

    connect(project, &AutonomousVehicleProject::backgroundUpdated, ui->projectView, &ProjectView::updateBackground);
    connect(project, &AutonomousVehicleProject::aboutToUpdateBackground, ui->projectView, &ProjectView::beforeUpdateBackground);

    connect(ui->projectView,&ProjectView::currentChanged,this,&MainWindow::setCurrent);

    ui->rosDetails->setEnabled(false);
    connect(project->rosLink(), &ROSLink::robotNamespaceUpdated, ui->helmManager, &HelmManager::updateRobotNamespace);
    connect(project->rosLink(), &ROSLink::rosConnected,this,&MainWindow::onROSConnected);
    ui->rosDetails->setROSLink(project->rosLink());

    project->rosLink()->connectROS();

    connect(project->rosLink(), &ROSLink::centerMap, ui->projectView, &ProjectView::centerMap);

    connect(ui->detailsView, &DetailsView::clearTasks, project->rosLink(), &ROSLink::clearTasks);
    
    connect(ui->projectView,&ProjectView::scaleChanged,project,&AutonomousVehicleProject::updateMapScale);

    m_ais_manager = new AISManager();
    connect(project, &AutonomousVehicleProject::backgroundUpdated, m_ais_manager, &AISManager::updateBackground);
    connect(ui->projectView, &ProjectView::viewportChanged, m_ais_manager, &AISManager::updateViewport);

    m_sound_play = new SoundPlay();

    m_speech_alerts = new SpeechAlerts(this);
    connect(m_speech_alerts, &SpeechAlerts::tell, m_sound_play, &SoundPlay::say);
    connect(ui->helmManager, &HelmManager::pilotingModeUpdated, m_speech_alerts, &SpeechAlerts::updatePilotingMode);
}

MainWindow::~MainWindow()
{
    delete ui;
    delete m_ais_manager;
}

void MainWindow::setWorkspace(const QString& path)
{
    m_workspace_path = path;
}

void MainWindow::open(const QString& fname)
{
    setCursor(Qt::WaitCursor);
    project->open(fname);
    unsetCursor();
}

void MainWindow::openBackground(const QString& fname)
{
    setCursor(Qt::WaitCursor);
    project->openBackground(fname);
    unsetCursor();
}



void MainWindow::setCurrent(QModelIndex &index)
{
    ui->treeView->setCurrentIndex(index);
    project->setCurrent(index);
}

void MainWindow::on_actionOpen_triggered()
{
    QString fname = QFileDialog::getOpenFileName(this,tr("Open"),m_workspace_path);
    open(fname);
}

void MainWindow::on_actionImport_triggered()
{
    QString fname = QFileDialog::getOpenFileName(this,tr("Import"),m_workspace_path);

    project->import(fname);
}

void MainWindow::on_actionWaypoint_triggered()
{
    project->setContextMode(false);
    ui->projectView->setAddWaypointMode();
}

void MainWindow::on_actionWaypointFromContext_triggered()
{
    project->setContextMode(true);
    ui->projectView->setAddWaypointMode();
}

void MainWindow::on_actionTrackline_triggered()
{
    project->setContextMode(false);
    ui->projectView->setAddTracklineMode();
}

void MainWindow::on_actionTracklineFromContext_triggered()
{
    project->setContextMode(true);
    ui->projectView->setAddTracklineMode();
}


void MainWindow::on_treeView_customContextMenuRequested(const QPoint &pos)
{
    QModelIndex index = ui->treeView->indexAt(pos);
    MissionItem  *mi = nullptr;
    if(index.isValid())
        mi = project->itemFromIndex(index);

    QMenu menu(this);

    if(mi && mi->canBeSentToRobot())
    {
        QAction *sendToROSAction = menu.addAction("Send to ROS (Use Execute button)");
        sendToROSAction->setEnabled(false);
        //connect(sendToROSAction, &QAction::triggered, this, &MainWindow::sendToROS);
        
        QMenu *missionMenu = menu.addMenu("Mission");
        
        QAction *appendMissionAction = missionMenu->addAction("append");
        connect(appendMissionAction, &QAction::triggered, this, &MainWindow::appendMission);

        QAction *prependMissionAction = missionMenu->addAction("prepend");
        connect(prependMissionAction, &QAction::triggered, this, &MainWindow::prependMission);

        QAction *updateMissionAction = missionMenu->addAction("update");
        connect(updateMissionAction, &QAction::triggered, this, &MainWindow::updateMission);
        
        QMenu *exportMenu = menu.addMenu("Export");

        QAction *exportHypackAction = exportMenu->addAction("Export Hypack");
        connect(exportHypackAction, &QAction::triggered, this, &MainWindow::exportHypack);

        QAction *exportMPAction = exportMenu->addAction("Export Mission Plan");
        connect(exportMPAction, &QAction::triggered, this, &MainWindow::exportMissionPlan);
    }

    
    QAction *openBackgroundAction = menu.addAction("Open Background");
    connect(openBackgroundAction, &QAction::triggered, this, &MainWindow::on_actionOpenBackground_triggered);
    
    QMenu *addMenu = menu.addMenu("Add");

    if(!index.isValid())
    {
        QAction *addWaypointAction = addMenu->addAction("Add Waypoint");
        connect(addWaypointAction, &QAction::triggered, this, &MainWindow::on_actionWaypoint_triggered);

        QAction *addTrackLineAction = addMenu->addAction("Add Track Line");
        connect(addTrackLineAction, &QAction::triggered, this, &MainWindow::on_actionTrackline_triggered);

        QAction *addSurveyPatternAction = addMenu->addAction("Add Survey Pattern");
        connect(addSurveyPatternAction, &QAction::triggered, this, &MainWindow::on_actionSurveyPattern_triggered);

        QAction *addGroupAction = addMenu->addAction("Add Group");
        connect(addGroupAction, &QAction::triggered, this, &MainWindow::on_actionGroup_triggered);
        
        QAction *addPlatformAction = addMenu->addAction("Add Platform");
        connect(addPlatformAction, &QAction::triggered, this, &MainWindow::on_actionPlatform_triggered);
    }
    else
    {
        if(mi && mi->canAcceptChildType("Waypoint"))
        {
            QAction *addWaypointAction = addMenu->addAction("Add Waypoint");
            connect(addWaypointAction, &QAction::triggered, this, &MainWindow::on_actionWaypointFromContext_triggered);
        }

        if(mi && mi->canAcceptChildType("TrackLine"))
        {
            QAction *addTrackLineAction = addMenu->addAction("Add Track Line");
            connect(addTrackLineAction, &QAction::triggered, this, &MainWindow::on_actionTracklineFromContext_triggered);
        }

        if(mi && mi->canAcceptChildType("SurveyPattern"))
        {
            QAction *addSurveyPatternAction = addMenu->addAction("Add Survey Pattern");
            connect(addSurveyPatternAction, &QAction::triggered, this, &MainWindow::on_actionSurveyPatternFromContext_triggered);
        }

        if(mi && mi->canAcceptChildType("SurveyArea"))
        {
            QAction *addSurveyAreaAction = addMenu->addAction("Add Survey Area");
            connect(addSurveyAreaAction, &QAction::triggered, this, &MainWindow::on_actionSurveyAreaFromContext_triggered);
        }

        if(mi && mi->canAcceptChildType("Group"))
        {
            QAction *addGroupAction = addMenu->addAction("Add Group");
            connect(addGroupAction, &QAction::triggered, this, &MainWindow::on_actionGroupFromContext_triggered);
        }
        
        if(mi && mi->canAcceptChildType("Platform"))
        {
            QAction *addPlatformAction = addMenu->addAction("Add Platform");
            connect(addPlatformAction, &QAction::triggered, this, &MainWindow::on_actionPlatformFromContext_triggered);
        }

        QAction *deleteItemAction = menu.addAction("Delete");
        connect(deleteItemAction, &QAction::triggered, [=](){this->project->deleteItems(ui->treeView->selectionModel()->selectedRows());});
        
        
        TrackLine *tl = qobject_cast<TrackLine*>(mi);
        if(tl)
        {
            QAction *reverseDirectionAction = menu.addAction("Reverse Direction");
            connect(reverseDirectionAction, &QAction::triggered, tl, &TrackLine::reverseDirection);
            if(project->getBackgroundRaster() && project->getDepthRaster())
            {
                QAction *planPathAction = menu.addAction("Plan path");
                connect(planPathAction, &QAction::triggered, tl, &TrackLine::planPath);
            }

        }

        SurveyPattern *sp = qobject_cast<SurveyPattern*>(mi);
        if(sp)
        {
            QAction *reverseDirectionAction = menu.addAction("Reverse Direction");
            connect(reverseDirectionAction, &QAction::triggered, sp, &SurveyPattern::reverseDirection);
        }
        
        GeoGraphicsMissionItem *gmi = qobject_cast<GeoGraphicsMissionItem*>(mi);
        if(gmi)
        {
            if(gmi->locked())
            {
                QAction *unlockItemAction = menu.addAction("Unlock");
                connect(unlockItemAction, &QAction::triggered, gmi, &GeoGraphicsMissionItem::unlock);
            }
            else
            {
                QAction *lockItemAction = menu.addAction("Lock");
                connect(lockItemAction, &QAction::triggered, gmi, &GeoGraphicsMissionItem::lock);
            }
        }
        
        SurveyArea *sa = qobject_cast<SurveyArea*>(mi);
        if(sa)
        {
            if(project->getBackgroundRaster() && project->getDepthRaster())
            {
                QAction *generateAdaptiveTrackLinesAction = menu.addAction("Generate Adaptive Track Lines");
                connect(generateAdaptiveTrackLinesAction, &QAction::triggered, sa, &SurveyArea::generateAdaptiveTrackLines);
            }
        }
    }

    menu.exec(ui->treeView->mapToGlobal(pos));
}

void MainWindow::exportHypack() const
{
    project->exportHypack(ui->treeView->selectionModel()->currentIndex());
}

void MainWindow::exportMissionPlan() const
{
    project->exportMissionPlan(ui->treeView->selectionModel()->currentIndex());
}

void MainWindow::sendToROS() const
{
    project->sendToROS(ui->treeView->selectionModel()->currentIndex());
}

void MainWindow::appendMission() const
{
    project->appendMission(ui->treeView->selectionModel()->currentIndex());
}

void MainWindow::prependMission() const
{
    project->prependMission(ui->treeView->selectionModel()->currentIndex());
}

void MainWindow::updateMission() const
{
    project->updateMission(ui->treeView->selectionModel()->currentIndex());
}


void MainWindow::on_actionSave_triggered()
{
    on_actionSaveAs_triggered();
}

void MainWindow::on_actionSaveAs_triggered()
{
    QString fname = QFileDialog::getSaveFileName(this);
    project->save(fname);
}

void MainWindow::on_actionOpenBackground_triggered()
{
    QString fname = QFileDialog::getOpenFileName(this,tr("Open"),m_workspace_path);

    if(!fname.isEmpty())
    {
        setCursor(Qt::WaitCursor);
        project->openBackground(fname);
        unsetCursor();
    }

}

void MainWindow::on_actionSurveyPattern_triggered()
{
    project->setContextMode(false);
    ui->projectView->setAddSurveyPatternMode();
}

void MainWindow::on_actionSurveyPatternFromContext_triggered()
{
    project->setContextMode(true);
    ui->projectView->setAddSurveyPatternMode();
}

void MainWindow::on_actionSurveyArea_triggered()
{
    project->setContextMode(false);
    ui->projectView->setAddSurveyAreaMode();
}

void MainWindow::on_actionSurveyAreaFromContext_triggered()
{
    project->setContextMode(true);
    ui->projectView->setAddSurveyAreaMode();
}

void MainWindow::on_actionPlatform_triggered()
{
    project->setContextMode(false);
    project->createPlatform();
}

void MainWindow::on_actionPlatformFromContext_triggered()
{
    project->setContextMode(true);
    project->createPlatform();
}

void MainWindow::on_actionBehavior_triggered()
{
    project->setContextMode(false);
    project->createBehavior();
}

void MainWindow::on_actionOpenGeometry_triggered()
{
    project->setContextMode(false);
    QString fname = QFileDialog::getOpenFileName(this,tr("Open"),m_workspace_path);

    if(!fname.isEmpty())
        project->openGeometry(fname);
}

void MainWindow::on_actionGroup_triggered()
{
    project->setContextMode(false);
    project->addGroup();
}

void MainWindow::on_actionGroupFromContext_triggered()
{
    project->setContextMode(true);
    project->addGroup();
}

void MainWindow::on_actionRadar_triggered()
{
    qDebug() << "radar: " << ui->actionRadar->isChecked();
    emit project->showRadar(ui->actionRadar->isChecked());
}

void MainWindow::on_actionFollow_triggered()
{
    emit project->followRobot(ui->actionFollow->isChecked());
}

void MainWindow::on_actionRadarColor_triggered()
{
    emit project->selectRadarColor();
}

void MainWindow::on_actionShowTail_triggered()
{
    emit project->showTail(ui->actionShowTail->isChecked());
}

void MainWindow::onROSConnected(bool connected)
{
    ui->rosDetails->setEnabled(connected);
}

void MainWindow::on_actionAISManager_triggered()
{
    m_ais_manager->show();
}

void MainWindow::on_actionSay_something_triggered()
{
    m_sound_play->show();
}