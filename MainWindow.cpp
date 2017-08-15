#include "MainWindow.hpp"
#include "ui_MainWindow.h"
#include "dialogs/aboutdialog.h"
#include "dialogs/plotdialog.h"
#include "views/RawTreeView.hpp"
#include "nix.hpp"
#include "common/Common.hpp"
#include "model/nixtreemodel.h"
#include "model/nixtreemodelitem.h"
#include "dialogs/tabledialog.hpp"
#include "dialogs/optionsdialog.h"
#include "dialogs/filepropertiesdialog.hpp"
#include <QSettings>
#include "utils/utils.hpp"
#include "db/projectmanager.hpp"
#include <QInputDialog>
#include <QDebug>


MainWindow::MainWindow(QWidget *parent, QApplication *app) : QMainWindow(parent),
    ui(new Ui::MainWindow), currentFile(""), currentProject("") {

    QCoreApplication::setOrganizationName("g-node");
    QCoreApplication::setApplicationName("nixview");
    QCoreApplication::setApplicationVersion("0.9");

    ui->setupUi(this);
    file_label = new QLabel(this);
    file_progress = new QProgressBar(this);
    ui->statusBar->addPermanentWidget(new QLabel("File: ", this));
    ui->statusBar->addPermanentWidget(file_label, 1);
    ui->statusBar->addPermanentWidget(file_progress, 10);
    file_progress->setVisible(false);
    ui->actionFile_properties->setEnabled(false);
    QObject::connect(app, SIGNAL(invalid_file_error()), this, SLOT(invalid_file_error()));
    ui->recent_file_list->setAttribute(Qt::WA_MacShowFocusRect, false);
    ui->recent_projects_list->setAttribute(Qt::WA_MacShowFocusRect, false);
    ui->menuFind->addAction(ui->actionFind);
    connect_widgets();
    get_recent_files();
    get_recent_projects();
    //populate_recent_projects();
}


void MainWindow::connect_widgets() {
    QObject::connect(ui->actionCloseFile, SIGNAL(triggered()), this, SLOT(close_file()));
    QObject::connect(this, SIGNAL(emit_view_change(int)), ui->main_view, SLOT(set_view(int)));
    QObject::connect(ui->main_view, SIGNAL(emit_current_qml(QModelIndex)), this, SLOT(item_selected(QModelIndex)));
    QObject::connect(ui->main_view, SIGNAL(emit_current_qml(QModelIndex)), ui->info_view, SLOT(update_info_widget(QModelIndex)));
    QObject::connect(ui->main_view, SIGNAL(scan_progress_update()), this, SLOT(file_scan_progress()));
    QObject::connect(ui->main_view, SIGNAL(close_file()), this, SLOT(close_file()));
    //QObject::connect(ui->main_view, SIGNAL(update_file()), this, SLOT(new_file_update()));
    QObject::connect(ui->menu_open_recent, SIGNAL(triggered(QAction*)), this, SLOT(open_recent_file(QAction*)));
    QObject::connect(ui->searchForm, SIGNAL(newResults(std::vector<QVariant>)), this, SLOT(newSearchResults(std::vector<QVariant>)));
}


void MainWindow::get_recent_files() {
    QSettings *settings = new QSettings();
    settings->beginGroup(RECENT_FILES_GROUP);
    settings->beginGroup(RECENT_FILES_LIST);
    QStringList keys = settings->childKeys();
    recent_files.clear();
    for (QString k: keys) {
        recent_files.push_back(settings->value(k).toString());
    }
    nixview::util::remove_duplicates(recent_files);
    populate_recent_file_menu();
    settings->endGroup();
    settings->endGroup();
    delete settings;
}

void MainWindow::get_recent_projects() {
    QSettings *settings = new QSettings();
    settings->beginGroup(RECENT_PROJECTS_GROUP);
    settings->beginGroup(RECENT_PROJECTS_LIST);
    QStringList keys = settings->childKeys();
    recent_projects.clear();
    for (QString k: keys) {
        recent_projects.push_back(settings->value(k).toString());
    }
    nixview::util::remove_duplicates(recent_projects);
    populate_recent_projects_menu();
    settings->endGroup();
    settings->endGroup();
    delete settings;
}

MainWindow::~MainWindow() {
    delete ui;
}

// slots
void MainWindow::on_actionTree_triggered() {
    emit emit_view_change(VIEW_TREE);
}

void MainWindow::on_actionColumn_triggered() {
    emit emit_view_change(VIEW_COLUMN);
}


void MainWindow::searchResultSelected() {
    QList<QListWidgetItem*> items = ui->searchResults->selectedItems();
    if (items.size() > 0) {
        item_selected(items[0]->data(Qt::UserRole));
    }
}


void MainWindow::item_selected(QModelIndex qml) {
    NixTreeModelItem *item = static_cast<NixTreeModelItem*>(qml.internalPointer());
    item_selected(item->itemData());
}


void MainWindow::item_selected(QVariant v) {
    selected_item = v;
    ui->actionPlot->setEnabled(false);
    ui->actionTable->setEnabled(false);
    if(v.canConvert<nix::DataArray>() | v.canConvert<nix::Feature>()) {
        ui->actionTable->setEnabled(true);
        ui->actionPlot->setEnabled(true);
    } else if (v.canConvert<nix::Tag>() | v.canConvert<nix::MultiTag>()) {
        ui->actionPlot->setEnabled(true);
    }
}


void MainWindow::show_about() {
    AboutDialog d(this);
    d.exec();
}


void MainWindow::show_plot() {
    PlotDialog d(this);
    d.set_entity(selected_item);
    d.exec();
}


void MainWindow::show_table() {
    TableDialog d(this);
    d.set_entity(selected_item);
    d.exec();
}


void MainWindow::show_options() {
    OptionsDialog d(this);
    QObject::connect(&d, SIGNAL(recent_file_changed(QStringList)), this, SLOT(recent_file_update(QStringList)));
    QObject::connect(&d, SIGNAL(column_visibility_changed(QString, QString,bool)), this, SLOT(visible_columns_update(QString, QString,bool)));
    d.exec();
}


void MainWindow::find() {
    previous_page = ui->stackedWidget->currentIndex();
    ui->searchForm->setNixFile(ui->main_view->get_nix_file());
    ui->stackedWidget->setCurrentIndex(1);
    ui->searchForm->receiveFocus();
}


void MainWindow::closeSearch() {
    ui->stackedWidget->setCurrentIndex(previous_page);
}


void MainWindow::clearSearch() {
    ui->searchForm->clear();
    ui->searchResults->clear();
}


void MainWindow::newSearchResults(std::vector<QVariant> results) {
    ui->searchResults->clear();
    for (auto r : results) {
        std::string str;
        if (r.canConvert<nix::DataArray>()) {
            nix::DataArray a = r.value<nix::DataArray>();
            str = a.name() + " [" + a.type() + "]";
        } else if (r.canConvert<nix::Block>()) {
            nix::Block b = r.value<nix::Block>();
            str = b.name() + " [" + b.type() + "]";
        } else if (r.canConvert<nix::Tag>()) {
            nix::Tag t = r.value<nix::Tag>();
            str = t.name() + " [" + t.type() + "]";
        } else if (r.canConvert<nix::MultiTag>()) {
            nix::MultiTag t = r.value<nix::MultiTag>();
            str = t.name() + " [" + t.type() + "]";
        } else if (r.canConvert<nix::Group>()) {
            nix::Group g = r.value<nix::Group>();
            str = g.name() + " [" + g.type() + "]";
        } else if (r.canConvert<nix::Source>()) {
            nix::Source s = r.value<nix::Source>();
            str = s.name() + " [" + s.type() + "]";
        }
        QListWidgetItem *itm = new QListWidgetItem(QString::fromStdString(str));
        itm->setData(Qt::UserRole, r);
        ui->searchResults->addItem(itm);
    }
}


void MainWindow::checkToolTip(QListWidgetItem *item) {
    if (item->toolTip().size() == 0) {
        item->setToolTip(QString::fromStdString(EntityDescriptor::describe(item->data(Qt::UserRole))));
    }
}


void MainWindow::show_file_properties() {
    FilePropertiesDialog d(this);
    boost::filesystem::path p(file_label->text().toStdString());
    d.set_file(ui->main_view->get_nix_file(), p);
    d.exec();
}


void MainWindow::file_scan_progress() {
    file_progress->setValue(ui->main_view->get_scan_progress());
    QCoreApplication::processEvents();
}


void MainWindow::invalid_file_error() {
    file_label->setText("");
    file_progress->setVisible(false);
    QMessageBox::information(this, "Invalid file", "Nix library reports an error. The selected file is not a valid nix file.");
}


void MainWindow::open_file() {
    QFileDialog fd(this);
    fd.setAcceptMode(QFileDialog::AcceptOpen);
    fd.setFileMode(QFileDialog::ExistingFile);
    fd.setNameFilter(tr("NIX File (*.nix *.h5)"));
    fd.setViewMode(QFileDialog::Detail);
    QStringList fileNames;
    if (fd.exec())
        fileNames = fd.selectedFiles();
    if (fileNames.size() == 0)
        return;
    read_nix_file(fileNames.front());
}


void MainWindow::open_project() {   
    QFileDialog fd(this);
    fd.setAcceptMode(QFileDialog::AcceptOpen);
    fd.setFileMode(QFileDialog::ExistingFile);
    fd.setNameFilter(tr("nixview project index file (*.npi, *.db)"));
    fd.setViewMode(QFileDialog::Detail);
    QStringList fileNames;
    if (fd.exec())
        fileNames = fd.selectedFiles();
    if (fileNames.size() == 0)
        return;

    close_file();
    close_project();
    open_project(fileNames.front());
}


void MainWindow::open_project(const QString &project) {
    if (ui->main_view->set_project(project)) {
        set_current_project(project);
        toggle_project_controls(true);
        ui->main_view->show_project_navigator(true);
        ui->stackedWidget->setCurrentIndex(0);
    }
}


void MainWindow::new_project() {
    ui->main_view->new_project();
}


void MainWindow::close_project() {
    toggle_project_controls(false);
    set_current_project("");
    close_file();
}


void MainWindow::close_file() {
    ui->stackedWidget->setCurrentIndex(2);
    ui->main_view->clear();
    toggle_file_controls(false);
    set_current_file("");
    clearSearch();
}


void MainWindow::read_nix_file(QString filename) {
    std::string file_path = filename.toStdString();
    QFile f(filename);
    if (!f.exists()) {
        QMessageBox::information(this, "File not found!", "File " + filename + " does not exist!", QMessageBox::Ok);
        return;
    }
    file_label->setText(file_path.c_str());
    file_progress->setVisible(true);

    if (ui->main_view->set_nix_file(file_path)) {
        file_progress->setVisible(false);
        ui->stackedWidget->setCurrentIndex(0);
        toggle_file_controls(true);
        update_recent_file_list(filename);
        set_current_file(filename);
        qDebug() << "[Info] current project.count " << currentProject.count();
        ui->main_view->show_project_navigator(currentProject.count() > 0);
    }
}


void MainWindow::update_recent_file_list(QString filename) {
    QSettings *settings = new QSettings();
    settings->beginGroup(RECENT_FILES_GROUP);
    settings->beginGroup(RECENT_FILES_LIST);
    QStringList files;
    QStringList keys = settings->childKeys();
    for (QString k : keys) {
        files.push_back(settings->value(k).toString());
    }
    files.insert(0, filename);
    settings->endGroup();
    settings->endGroup();
    delete settings;
    recent_file_update(files);
}


void MainWindow::save_recent_files(QStringList &files) {
    nixview::util::remove_duplicates(files);
    QSettings *settings = new QSettings;
    settings->beginGroup(RECENT_FILES_GROUP);
    int max_count = settings->value(RECENT_FILES_COUNT, 5).toInt();
    settings->beginGroup(RECENT_FILES_LIST);
    settings->remove("");
    for (int i = 0; i < files.size(); i ++) {
        if (i >= max_count) {
            break;
        }
        QString key = QString::fromStdString(nix::util::numToStr(i));
        settings->setValue(key, files[i]);
    }
    while (files.size() > max_count) {
        files.removeLast();
    }
    settings->endGroup();
    settings->endGroup();
    delete settings;
}


void MainWindow::populate_recent_file_menu() {
    ui->recent_file_list->clear();
    QList<QAction*> actions = ui->menu_open_recent->actions();
    for (QAction* a : actions) {
        ui->menu_open_recent->removeAction(a);
        delete a;
    }
    for (QString s : recent_files) {
        ui->menu_open_recent->addAction(s);
        QListWidgetItem *item = new QListWidgetItem(s);
        item->setToolTip(s);
        ui->recent_file_list->addItem(item);
    }
    ui->menu_open_recent->setEnabled(ui->menu_open_recent->actions().count() != 0);    
}


void MainWindow::save_recent_projects(QStringList &projects) {
    nixview::util::remove_duplicates(projects);
    QSettings *settings = new QSettings;
    settings->beginGroup(RECENT_PROJECTS_GROUP);
    int max_count = settings->value(RECENT_PROJECTS_COUNT, 5).toInt();
    settings->beginGroup(RECENT_PROJECTS_LIST);
    settings->remove("");
    for (int i = 0; i < projects.size(); i ++) {
        if (i >= max_count) {
            break;
        }
        QString key = QString::fromStdString(nix::util::numToStr(i));
        settings->setValue(key, projects[i]);
    }
    while (projects.size() > max_count) {
        projects.removeLast();
    }
    settings->endGroup();
    settings->endGroup();
    delete settings;
}


void MainWindow::update_recent_projects_list(QString project) {
    QSettings *settings = new QSettings();
    settings->beginGroup(RECENT_PROJECTS_GROUP);
    settings->beginGroup(RECENT_PROJECTS_LIST);
    QStringList projects;
    QStringList keys = settings->childKeys();
    for (QString k : keys) {
        projects.push_back(settings->value(k).toString());
    }
    projects.insert(0, project);
    settings->endGroup();
    settings->endGroup();
    delete settings;
    recent_project_update(projects);
}

void MainWindow::populate_recent_projects_menu() {
    ui->recent_projects_list->clear();
    QList<QAction*> actions = ui->menu_open_recent_project->actions();
    for (QAction* a : actions) {
        ui->menu_open_recent_project->removeAction(a);
        delete a;
    }
    for (QString s : recent_projects) {
        ui->menu_open_recent_project->addAction(s);
        QListWidgetItem *item = new QListWidgetItem(s);
        item->setToolTip(s);
        ui->recent_projects_list->addItem(item);
    }
    ui->menu_open_recent_project->setEnabled(ui->menu_open_recent_project->actions().count() != 0);
}


void MainWindow::recent_file_update(QStringList files) {
    save_recent_files(files);
    this->recent_files = files;
    populate_recent_file_menu();
}

void MainWindow::recent_project_update(QStringList projects) {
    save_recent_projects(projects);
    this->recent_projects = projects;
    populate_recent_projects_menu();
}

void MainWindow::visible_columns_update(QString who, QString column, bool state) {
    if (who == MAIN_TREE_VIEW) {
        ui->main_view->getTreeView()->setColumnState(column, state);
    } else if (who == METADATA_TREE_VIEW) {
        ui->info_view->metadata_column_state_change(column, state);
    }
}


void MainWindow::open_recent_file(QAction *a) {
    read_nix_file(a->text());
}


void MainWindow::recent_file_selected(QListWidgetItem *item) {
    read_nix_file(item->text());
}


void MainWindow::recent_project_selected(QListWidgetItem *item) {
    open_project(item->text());
}


void MainWindow::new_file_update(QString filename) {
    set_current_file(filename);
    toggle_file_controls(true);
}


void MainWindow::set_current_file(const QString &filename) {
    qDebug() << ("[DEBUG] Set filename from " + this->currentFile + " to ") << filename;
    this->currentFile = filename;
}


void MainWindow::set_current_project(const QString &project) {
    qDebug() << ("[DEBUG] Set project from " + this->currentProject + " to ") << project;
    this->currentProject = project;
}


void MainWindow::toggle_file_controls(bool enabled) {
    //qDebug() << ("[DEBUG] Toggle file controls from " + ui->actionCloseFile->isEnabled() + " to ") << enabled;
    std::cerr << "[DEBUG] Toggle file controls from " << ui->actionCloseFile->isEnabled() << " to " <<  enabled << std::endl;
    ui->actionCloseFile->setEnabled(enabled);
    ui->actionFile_properties->setEnabled(enabled);
    ui->actionFind->setEnabled(enabled);
    ui->actionPlot->setEnabled(enabled);
    ui->actionTable->setEnabled(enabled);
    ui->actionAddCurrentFileToProject->setEnabled(enabled && !currentProject.isEmpty());
}

void MainWindow::toggle_project_controls(bool enabled) {
    //qDebug() << ("[Info] Toggle project controls from " + ui->actionCloseFile->isEnabled() + " to ") << enabled;
    std::cerr << "[DEBUG] Toggle project controls from " << ui->actionClose_project->isEnabled() << " to " <<  enabled << std::endl;

    ui->actionClose_project->setEnabled(enabled);
    ui->actionAddCurrentFileToProject->setEnabled(enabled && !currentFile.isEmpty());
    ui->actionProjectAdd_file->setEnabled(enabled);
    ui->actionProjectRemove_file->setEnabled(enabled);
}
