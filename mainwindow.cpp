﻿#include "mainwindow.h"
#include "ui_mainwindow.h"

#include "io/config.h"

#include "project/sequence.h"

#include "ui/sourcetable.h"
#include "ui/viewerwidget.h"

#include "panels/panels.h"
#include "panels/project.h"
#include "panels/effectcontrols.h"
#include "panels/viewer.h"
#include "panels/timeline.h"

#include "project/undo.h"

#include "dialogs/aboutdialog.h"
#include "dialogs/newsequencedialog.h"
#include "dialogs/exportdialog.h"
#include "dialogs/preferencesdialog.h"
#include "dialogs/demonotice.h"
#include "dialogs/speeddialog.h"

#include "playback/audio.h"
#include "playback/playback.h"

#include "ui_timeline.h"

#include <QDebug>
#include <QStyleFactory>
#include <QMessageBox>
#include <QFileDialog>
#include <QStandardPaths>
#include <QTimer>
#include <QCloseEvent>
#include <QMovie>
#include <QInputDialog>
#include <QRegExp>

QMainWindow* mainWindow;

#define OLIVE_FILE_FILTER "Olive Project (*.ove)"

QTimer autorecovery_timer;
QString config_dir;
QString appName = "Olive (September 2018 | Alpha)";
bool demoNoticeShown = false;

void MainWindow::setup_layout() {
    panel_project->show();
    panel_effect_controls->show();
    panel_viewer->show();
    panel_timeline->show();

    addDockWidget(Qt::TopDockWidgetArea, panel_project);
    addDockWidget(Qt::TopDockWidgetArea, panel_effect_controls);
    addDockWidget(Qt::TopDockWidgetArea, panel_viewer);
	addDockWidget(Qt::BottomDockWidgetArea, panel_timeline);

	resizeDocks({panel_project}, {40}, Qt::Horizontal);
}

MainWindow::MainWindow(QWidget *parent) :
	QMainWindow(parent),
	ui(new Ui::MainWindow)
{
	mainWindow = this;

	// set up style?

    qApp->setStyle(QStyleFactory::create("Fusion"));

	QPalette darkPalette;
    darkPalette.setColor(QPalette::Window, QColor(53,53,53));
	darkPalette.setColor(QPalette::WindowText, Qt::white);
	darkPalette.setColor(QPalette::Base, QColor(25,25,25));
	darkPalette.setColor(QPalette::AlternateBase, QColor(53,53,53));
    darkPalette.setColor(QPalette::ToolTipBase, QColor(25,25,25));
	darkPalette.setColor(QPalette::ToolTipText, Qt::white);
	darkPalette.setColor(QPalette::Text, Qt::white);
	darkPalette.setColor(QPalette::Button, QColor(53,53,53));
	darkPalette.setColor(QPalette::ButtonText, Qt::white);	darkPalette.setColor(QPalette::BrightText, Qt::red);
	darkPalette.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(128, 128, 128));
	darkPalette.setColor(QPalette::Link, QColor(42, 130, 218));
	darkPalette.setColor(QPalette::Highlight, QColor(42, 130, 218));
	darkPalette.setColor(QPalette::HighlightedText, Qt::black);

    qApp->setPalette(darkPalette);

	// end style
	ui->setupUi(this);

	updateTitle("");
	setWindowState(Qt::WindowMaximized);

	statusBar()->showMessage("Welcome to " + appName);

    setDockNestingEnabled(true);

    // TODO maybe replace these with non-pointers later on?
    panel_project = new Project(this);
    panel_effect_controls = new EffectControls(this);
    panel_viewer = new Viewer(this);
    panel_timeline = new Timeline(this);

	setup_layout();

    connect(ui->menuWindow, SIGNAL(aboutToShow()), this, SLOT(windowMenu_About_To_Be_Shown()));
    connect(ui->menu_View, SIGNAL(aboutToShow()), this, SLOT(viewMenu_About_To_Be_Shown()));
    connect(ui->menu_Tools, SIGNAL(aboutToShow()), this, SLOT(toolMenu_About_To_Be_Shown()));
    connect(ui->menuEdit, SIGNAL(aboutToShow()), this, SLOT(editMenu_About_To_Be_Shown()));
    connect(ui->menu_File, SIGNAL(aboutToShow()), this, SLOT(fileMenu_About_To_Be_Shown()));

    QString data_dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (!data_dir.isEmpty()) {
        QDir dir(data_dir);
        dir.mkpath(".");
        if (dir.exists()) {
            // detect auto-recovery file
            autorecovery_filename = data_dir + "/autorecovery.ove";
            if (QFile::exists(autorecovery_filename)) {
                if (QMessageBox::question(NULL, "Auto-recovery", "Olive didn't close properly and an autorecovery file was detected. Would you like to open it?", QMessageBox::Yes, QMessageBox::No) == QMessageBox::Yes) {
					updateTitle(autorecovery_filename);
                    panel_project->load_project();
                }
            }
            autorecovery_timer.setInterval(60000);
            QObject::connect(&autorecovery_timer, SIGNAL(timeout()), this, SLOT(autorecover_interval()));
            autorecovery_timer.start();

            // search for open recents list
            recent_proj_file = data_dir + "/recents";
            QFile f(recent_proj_file);
            if (f.exists() && f.open(QFile::ReadOnly | QFile::Text)) {
                QTextStream text_stream(&f);
                while (true) {
                    QString line = text_stream.readLine();
                    if (line.isNull()) {
                        break;
                    } else {
                        recent_projects.append(line);
                    }
                }
                f.close();
            }

            config_dir = data_dir + "/config.xml";
            config.load(config_dir);
        }
	}

	connect(ui->action_Undo, SIGNAL(triggered(bool)), this, SLOT(undo()));
	connect(ui->action_Redo, SIGNAL(triggered(bool)), this, SLOT(redo()));
}

MainWindow::~MainWindow() {
	set_sequence(NULL);

    QString data_dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (!data_dir.isEmpty() && !autorecovery_filename.isEmpty()) {
        if (QFile::exists(autorecovery_filename)) {
            QFile::rename(autorecovery_filename, autorecovery_filename + "." + QDateTime::currentDateTimeUtc().toString("yyyyMMddHHmmss"));
        }
    }
    if (!config_dir.isEmpty()) {
        config.save(config_dir);
    }

	stop_audio();

	delete ui;

    delete panel_project;
    delete panel_effect_controls;
    delete panel_viewer;
    delete panel_timeline;
}

void MainWindow::on_action_Import_triggered()
{
	panel_project->import_dialog();
}

void MainWindow::on_actionExit_triggered()
{
    close();
}

void MainWindow::on_actionAbout_triggered()
{
	AboutDialog a(this);
	a.exec();
}

void MainWindow::on_actionDelete_triggered()
{
	if (panel_timeline->focused()) {
		panel_timeline->delete_selection(false);
    } else if (panel_effect_controls->is_focused()) {
        panel_effect_controls->delete_effects();
    } else if (panel_project->is_focused()) {
        panel_project->delete_selected_media();
	} else if (panel_effect_controls->keyframe_focus()) {
		panel_effect_controls->delete_selected_keyframes();
	}
}

void MainWindow::on_actionSelect_All_triggered()
{
	if (panel_timeline->focused()) {
		panel_timeline->select_all();
	}
}

void MainWindow::on_actionSequence_triggered()
{
    NewSequenceDialog nsd(this);
	nsd.set_sequence_name(panel_project->get_next_sequence_name());
	nsd.exec();
}

void MainWindow::on_actionZoom_In_triggered()
{
	if (panel_timeline->focused()) {
        panel_timeline->set_zoom(true);
	} else if (panel_effect_controls->keyframe_focus()) {
		panel_effect_controls->set_zoom(true);
	}
}

void MainWindow::on_actionZoom_out_triggered()
{
	if (panel_timeline->focused()) {
        panel_timeline->set_zoom(false);
	} else if (panel_effect_controls->keyframe_focus()) {
		panel_effect_controls->set_zoom(false);
	}
}

void MainWindow::on_actionExport_triggered()
{
    if (sequence == NULL) {
        QMessageBox::information(this, "No active sequence", "Please open the sequence you wish to export.", QMessageBox::Ok);
    } else {
        ExportDialog e(this);
        e.exec();
    }
}

void MainWindow::on_actionProject_2_toggled(bool arg1)
{
	panel_project->setVisible(arg1);
}

void MainWindow::on_actionEffect_Controls_toggled(bool arg1)
{
	panel_effect_controls->setVisible(arg1);
}

void MainWindow::on_actionViewer_toggled(bool arg1)
{
	panel_viewer->setVisible(arg1);
}

void MainWindow::on_actionTimeline_toggled(bool arg1)
{
	panel_timeline->setVisible(arg1);
}

void MainWindow::on_actionRipple_Delete_triggered()
{
	panel_timeline->delete_selection(true);
}

void MainWindow::editMenu_About_To_Be_Shown() {
    ui->action_Undo->setEnabled(undo_stack.canUndo());
    ui->action_Redo->setEnabled(undo_stack.canRedo());
}

void MainWindow::undo() {
	undo_stack.undo();
    panel_timeline->redraw_all_clips(true);
}

void MainWindow::redo() {
	undo_stack.redo();
	panel_timeline->redraw_all_clips(true);
}

void MainWindow::openSpeedDialog() {
	if (sequence != NULL) {
		SpeedDialog s(this);
		for (int i=0;i<sequence->clips.size();i++) {
			Clip* c = sequence->clips.at(i);
            if (c != NULL && panel_timeline->is_clip_selected(c, true)) {
				s.clips.append(c);
			}
		}
		if (s.clips.size() > 0) s.exec();
	}
}

void MainWindow::on_actionSplit_at_Playhead_triggered()
{
    if (panel_timeline->focused()) {
        panel_timeline->split_at_playhead();
    }
}

void MainWindow::on_actionCu_t_triggered()
{
    if (panel_timeline->focused()) {
        panel_timeline->copy(true);
    }
}

void MainWindow::on_actionCop_y_triggered()
{
    if (panel_timeline->focused()) {
        panel_timeline->copy(false);
    }
}

void MainWindow::on_action_Paste_triggered()
{
    if (panel_timeline->focused()) {
        panel_timeline->paste();
    }
}

void MainWindow::autorecover_interval() {
    if (project_changed) {
        panel_project->save_project(true);
        qDebug() << "[INFO] Auto-recovery project saved";
    }
}

bool MainWindow::save_project_as() {
    QString fn = QFileDialog::getSaveFileName(this, "Save Project As...", "", OLIVE_FILE_FILTER);
    if (!fn.isEmpty()) {
        if (!fn.endsWith(".ove", Qt::CaseInsensitive)) {
            fn += ".ove";
        }
		updateTitle(fn);
        panel_project->save_project(false);
        return true;
    }
    return false;
}

bool MainWindow::save_project() {
    if (project_url.isEmpty()) {
        return save_project_as();
    } else {
        panel_project->save_project(false);
        return true;
    }
}

bool MainWindow::can_close_project() {
    if (project_changed) {
        QMessageBox* m = new QMessageBox(
                    QMessageBox::Question,
                    "Unsaved Project",
                    "This project has changed since it was last saved. Would you like to save it before closing?",
                    QMessageBox::Yes|QMessageBox::No|QMessageBox::Cancel,
                    this
                );
        m->setWindowModality(Qt::WindowModal);
        int r = m->exec();
        if (r == QMessageBox::Yes) {
            return save_project();
        } else if (r == QMessageBox::Cancel) {
            return false;
        }
    }
	return true;
}

void MainWindow::updateTitle(const QString& url) {
	project_url = url;
	setWindowTitle(appName + " - " + ((project_url.isEmpty()) ? "<untitled>" : project_url));
}

void MainWindow::closeEvent(QCloseEvent *e) {
    if (can_close_project()) {
        e->accept();
    } else {
        e->ignore();
	}
}

void MainWindow::paintEvent(QPaintEvent *event) {
	QMainWindow::paintEvent(event);
#ifndef QT_DEBUG
	if (!demoNoticeShown) {
		DemoNotice* d = new DemoNotice(this);
		d->open();
		demoNoticeShown = true;
	}
#endif
}

void MainWindow::on_action_Save_Project_triggered()
{
    save_project();
}

void MainWindow::on_action_Open_Project_triggered()
{
    QString fn = QFileDialog::getOpenFileName(this, "Open Project...", "", OLIVE_FILE_FILTER);
	if (!fn.isEmpty() && can_close_project()) {
		updateTitle(fn);
        panel_project->load_project();
        undo_stack.clear();
    }
}

void MainWindow::on_actionProject_triggered()
{
    if (can_close_project()) {
		panel_effect_controls->clear_effects(true);
        undo_stack.clear();
        project_url.clear();
        panel_project->new_project();
		updateTitle("");
		panel_timeline->redraw_all_clips(false);
    }
}

void MainWindow::on_actionSave_Project_As_triggered()
{
    save_project_as();
}

void MainWindow::on_actionDeselect_All_triggered()
{
    if (panel_timeline->focused()) {
        panel_timeline->deselect();
    }
}

void MainWindow::on_actionReset_to_default_layout_triggered()
{
    setup_layout();
}

void MainWindow::on_actionGo_to_start_triggered()
{
	if (sequence != NULL && (panel_timeline->focused() || panel_viewer->hasFocus() || panel_effect_controls->keyframe_focus())) {
		panel_timeline->go_to_start();
	}
}

void MainWindow::on_actionPrevious_Frame_triggered()
{
	if (sequence != NULL && (panel_timeline->focused() || panel_viewer->hasFocus() || panel_effect_controls->keyframe_focus())) {
        panel_timeline->previous_frame();
    }
}

void MainWindow::on_actionNext_Frame_triggered()
{
	if (sequence != NULL && (panel_timeline->focused() || panel_viewer->hasFocus() || panel_effect_controls->keyframe_focus())) {
        panel_timeline->next_frame();
    }
}

void MainWindow::on_actionGo_to_End_triggered()
{
	if (sequence != NULL && (panel_timeline->focused() || panel_viewer->hasFocus() || panel_effect_controls->keyframe_focus())) {
        panel_timeline->go_to_end();
    }
}

void MainWindow::on_actionPlay_Pause_triggered()
{
	if (sequence != NULL && (panel_timeline->focused() || panel_viewer->hasFocus() || panel_effect_controls->keyframe_focus())) {
        panel_timeline->toggle_play();
    }
}

void MainWindow::on_actionEdit_Tool_triggered()
{
    if (panel_timeline->focused()) panel_timeline->ui->toolEditButton->click();
}

void MainWindow::on_actionToggle_Snapping_triggered()
{
    if (panel_timeline->focused()) panel_timeline->ui->snappingButton->click();
}

void MainWindow::on_actionPointer_Tool_triggered()
{
    if (panel_timeline->focused()) panel_timeline->ui->toolArrowButton->click();
}

void MainWindow::on_actionRazor_Tool_triggered()
{
    if (panel_timeline->focused()) panel_timeline->ui->toolRazorButton->click();
}

void MainWindow::on_actionRipple_Tool_triggered()
{
    if (panel_timeline->focused()) panel_timeline->ui->toolRippleButton->click();
}

void MainWindow::on_actionRolling_Tool_triggered()
{
    if (panel_timeline->focused()) panel_timeline->ui->toolRollingButton->click();
}

void MainWindow::on_actionSlip_Tool_triggered()
{
    if (panel_timeline->focused()) panel_timeline->ui->toolSlipButton->click();
}

void MainWindow::on_actionGo_to_Previous_Cut_triggered()
{
	if (sequence != NULL && (panel_timeline->focused() || panel_viewer->hasFocus())) {
        panel_timeline->previous_cut();
    }
}

void MainWindow::on_actionGo_to_Next_Cut_triggered()
{
	if (sequence != NULL && (panel_timeline->focused() || panel_viewer->hasFocus())) {
        panel_timeline->next_cut();
    }
}

void MainWindow::on_actionPreferences_triggered()
{
    PreferencesDialog pd(this);
    pd.setup_kbd_shortcuts(menuBar());
    pd.exec();
}

void MainWindow::on_actionIncrease_Track_Height_triggered()
{
    panel_timeline->increase_track_height();
}

void MainWindow::on_actionDecrease_Track_Height_triggered()
{
    panel_timeline->decrease_track_height();
}

void MainWindow::windowMenu_About_To_Be_Shown() {
    ui->actionProject_2->setChecked(panel_project->isVisible());
    ui->actionEffect_Controls->setChecked(panel_effect_controls->isVisible());
    ui->actionTimeline->setChecked(panel_timeline->isVisible());
    ui->actionViewer->setChecked(panel_viewer->isVisible());
}

void MainWindow::viewMenu_About_To_Be_Shown() {
    ui->actionTimeline_Track_Lines->setChecked(config.show_track_lines);
    ui->actionFrames->setChecked(config.timecode_view == TIMECODE_FRAMES);
    ui->actionDrop_Frame->setChecked(config.timecode_view == TIMECODE_DROP);
    ui->actionNon_Drop_Frame->setChecked(config.timecode_view == TIMECODE_NONDROP);

    ui->actionOff->setChecked(!config.show_title_safe_area);
    ui->actionDefault->setChecked(config.show_title_safe_area && !config.use_custom_title_safe_ratio);
    ui->action4_3->setChecked(config.show_title_safe_area && config.use_custom_title_safe_ratio && config.custom_title_safe_ratio == 4.0/3.0);
    ui->action16_9->setChecked(config.show_title_safe_area && config.use_custom_title_safe_ratio && config.custom_title_safe_ratio == 16.0/9.0);
    ui->actionCustom->setChecked(config.show_title_safe_area && config.use_custom_title_safe_ratio && !ui->action4_3->isChecked() && !ui->action16_9->isChecked());
}

void MainWindow::on_actionFrames_triggered()
{
    config.timecode_view = TIMECODE_FRAMES;
    if (sequence != NULL) {
		panel_viewer->update_playhead_timecode(sequence->playhead);
        panel_viewer->update_end_timecode();
    }
}

void MainWindow::on_actionDrop_Frame_triggered()
{
    config.timecode_view = TIMECODE_DROP;
    if (sequence != NULL) {
		panel_viewer->update_playhead_timecode(sequence->playhead);
        panel_viewer->update_end_timecode();
    }
}

void MainWindow::on_actionNon_Drop_Frame_triggered()
{
    config.timecode_view = TIMECODE_NONDROP;
    if (sequence != NULL) {
		panel_viewer->update_playhead_timecode(sequence->playhead);
        panel_viewer->update_end_timecode();
    }
}

void MainWindow::toolMenu_About_To_Be_Shown() {
    ui->actionEdit_Tool_Also_Seeks->setChecked(config.edit_tool_also_seeks);
    ui->actionEdit_Tool_Selects_Links->setChecked(config.edit_tool_selects_links);
    ui->actionSelecting_Also_Seeks->setChecked(config.select_also_seeks);
    ui->actionSeek_to_the_End_of_Pastes->setChecked(config.paste_seeks);
    ui->actionToggle_Snapping->setChecked(panel_timeline->snapping);
    ui->actionScroll_Wheel_Zooms->setChecked(config.scroll_zooms);
	ui->actionRectified_Waveforms->setChecked(config.rectified_waveforms);
	ui->actionEnable_Drag_Files_to_Timeline->setChecked(config.enable_drag_files_to_timeline);
    ui->actionAuto_scale_by_Default->setChecked(config.autoscale_by_default);
}

void MainWindow::on_actionEdit_Tool_Selects_Links_triggered() {
    config.edit_tool_selects_links = !config.edit_tool_selects_links;
}

void MainWindow::on_actionEdit_Tool_Also_Seeks_triggered() {
    config.edit_tool_also_seeks = !config.edit_tool_also_seeks;
}

void MainWindow::on_actionDuplicate_triggered() {
    if (panel_project->is_focused()) {
        panel_project->duplicate_selected();
    }
}

void MainWindow::on_actionSelecting_Also_Seeks_triggered() {
    config.select_also_seeks = !config.select_also_seeks;
}

void MainWindow::on_actionSeek_to_the_End_of_Pastes_triggered()
{
    config.paste_seeks = !config.paste_seeks;
}

void MainWindow::on_actionAdd_Default_Transition_triggered() {
    if (panel_timeline->focused()) panel_timeline->add_transition();
}

void MainWindow::on_actionSlide_Tool_triggered()
{
    if (panel_timeline->focused()) panel_timeline->ui->toolSlideButton->click();
}

void MainWindow::on_actionFolder_triggered() {
    undo_stack.push(new AddMediaCommand(panel_project->new_folder(0), panel_project->get_selected_folder()));
}

void MainWindow::fileMenu_About_To_Be_Shown() {
    if (recent_projects.size() > 0) {
        ui->menuOpen_Recent->clear();
        ui->menuOpen_Recent->setEnabled(true);
        for (int i=0;i<recent_projects.size();i++) {
            QAction* action = ui->menuOpen_Recent->addAction(recent_projects.at(i));
            action->setData(i);
            connect(action, SIGNAL(triggered()), this, SLOT(load_recent_project()));
        }
        ui->menuOpen_Recent->addSeparator();
        QAction* clear_action = ui->menuOpen_Recent->addAction("Clear Recent List");
        connect(clear_action, SIGNAL(triggered()), panel_project, SLOT(clear_recent_projects()));
    } else {
        ui->menuOpen_Recent->setEnabled(false);
    }
}

void MainWindow::load_recent_project() {
    int index = static_cast<QAction*>(sender())->data().toInt();
    QString recent_url = recent_projects.at(index);
    if (!QFile::exists(recent_url)) {
        if (QMessageBox::question(this, "Missing recent project", "The project '" + recent_url + "' no longer exists. Would you like to remove it from the recent projects list?", QMessageBox::Yes, QMessageBox::No) == QMessageBox::Yes) {
            recent_projects.removeAt(index);
            panel_project->save_recent_projects();
        }
	} else if (can_close_project()) {
		updateTitle(recent_url);
        panel_project->load_project();
    }
}

void MainWindow::on_actionScroll_Wheel_Zooms_triggered()
{
    config.scroll_zooms = !config.scroll_zooms;
}

void MainWindow::on_actionLink_Unlink_triggered()
{
    if (panel_timeline->focused()) panel_timeline->toggle_links();
}

void MainWindow::on_actionRipple_To_In_Point_triggered()
{
    if (panel_timeline->focused()) panel_timeline->ripple_to_in_point(true);
}

void MainWindow::on_actionRipple_to_Out_Point_triggered()
{
    if (panel_timeline->focused()) panel_timeline->ripple_to_in_point(false);
}

void MainWindow::on_actionSet_In_Point_triggered()
{
    if (panel_timeline->focused()) panel_timeline->set_in_point();
}

void MainWindow::on_actionSet_Out_Point_triggered()
{
    if (panel_timeline->focused()) panel_timeline->set_out_point();
}

void MainWindow::on_actionClear_In_Out_triggered() {
    if (panel_timeline->focused() && sequence->using_workarea) {
        undo_stack.push(new SetTimelineInOutCommand(sequence, false, 0, 0));
        panel_timeline->repaint_timeline();
    }
}

void MainWindow::on_actionDelete_In_Out_triggered()
{
    if (panel_timeline->focused()) {
        panel_timeline->delete_in_out(false);
    }
}

void MainWindow::on_actionRipple_Delete_In_Out_triggered()
{
    if (panel_timeline->focused()) {
        panel_timeline->delete_in_out(true);
    }
}

void MainWindow::on_actionTimeline_Track_Lines_triggered()
{
    config.show_track_lines = !config.show_track_lines;
    panel_timeline->redraw_all_clips(false);
}

void MainWindow::on_actionRectified_Waveforms_triggered()
{
	config.rectified_waveforms = !config.rectified_waveforms;
	panel_timeline->redraw_all_clips(false);
}

void MainWindow::on_actionDefault_triggered() {
    config.show_title_safe_area = true;
    config.use_custom_title_safe_ratio = false;
    panel_viewer->viewer_widget->update();
}

void MainWindow::on_actionOff_triggered() {
    config.show_title_safe_area = false;
    panel_viewer->viewer_widget->update();
}

void MainWindow::on_action4_3_triggered() {
    config.show_title_safe_area = true;
    config.use_custom_title_safe_ratio = true;
    config.custom_title_safe_ratio = 4.0/3.0;
    panel_viewer->viewer_widget->update();
}

void MainWindow::on_action16_9_triggered() {
    config.show_title_safe_area = true;
    config.use_custom_title_safe_ratio = true;
    config.custom_title_safe_ratio = 16.0/9.0;
    panel_viewer->viewer_widget->update();
}

void MainWindow::on_actionCustom_triggered() {
    QString input;
    bool invalid = false;
    QRegExp arTest("[0-9.]+:[0-9.]+");

    do {
        if (invalid) {
            QMessageBox::critical(this, "Invalid aspect ratio", "The aspect ratio '" + input + "' is invalid. Please try again.");
        }

        input = QInputDialog::getText(this, "Enter custom aspect ratio", "Enter the aspect ratio to use for the title/action safe area (e.g. 16:9):");
        invalid = !arTest.exactMatch(input) && !input.isEmpty();
    } while (invalid);

    if (!input.isEmpty()) {
        QStringList inputList = input.split(':');

        config.show_title_safe_area = true;
        config.use_custom_title_safe_ratio = true;
        config.custom_title_safe_ratio = inputList.at(0).toDouble()/inputList.at(1).toDouble();
        panel_viewer->viewer_widget->update();
    }
}

void MainWindow::on_actionEnable_Drag_Files_to_Timeline_triggered() {
	config.enable_drag_files_to_timeline = !config.enable_drag_files_to_timeline;
}

void MainWindow::on_actionAuto_scale_by_Default_triggered() {
    config.autoscale_by_default = !config.autoscale_by_default;
}
