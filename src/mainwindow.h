#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QModelIndex>

namespace Ui {
class MainWindow;
}

class AISManager;
class SoundPlay;
class SpeechAlerts;

class AutonomousVehicleProject;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

    void open(QString const &fname);
    void openBackground(QString const &fname);
    void setWorkspace(QString const &dir);
public slots:

    void setCurrent(QModelIndex &index);
    void onROSConnected(bool connected);
    

private slots:
    void on_actionOpen_triggered();
    void on_actionSave_triggered();
    void on_actionSaveAs_triggered();
    void on_actionImport_triggered();

    void on_actionOpenBackground_triggered();

    void on_actionWaypoint_triggered();
    void on_actionWaypointFromContext_triggered();
    void on_actionTrackline_triggered();
    void on_actionTracklineFromContext_triggered();
    void on_actionSurveyPattern_triggered();
    void on_actionSurveyPatternFromContext_triggered();
    void on_actionSurveyArea_triggered();
    void on_actionSurveyAreaFromContext_triggered();
    void on_actionPlatform_triggered();
    void on_actionPlatformFromContext_triggered();
    void on_actionGroup_triggered();
    void on_actionGroupFromContext_triggered();
    void on_actionBehavior_triggered();
    void on_actionOpenGeometry_triggered();

    void on_treeView_customContextMenuRequested(const QPoint &pos);

    void on_actionRadar_triggered();
    void on_actionRadarColor_triggered();
    void on_actionShowTail_triggered();
    void on_actionAISManager_triggered();
    void on_actionSay_something_triggered();
    void on_actionFollow_triggered();

private:
    Ui::MainWindow *ui;
    AutonomousVehicleProject *project;
    QString m_workspace_path;
    AISManager* m_ais_manager;
    SoundPlay* m_sound_play;
    SpeechAlerts* m_speech_alerts;

    void exportHypack() const;
    void exportMissionPlan() const;
    void sendToROS() const;
    void appendMission() const;
    void prependMission() const;
    void updateMission() const;
};

#endif // MAINWINDOW_H
