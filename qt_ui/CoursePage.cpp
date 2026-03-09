#include "CoursePage.h"
#include "AppSession.h"
#include "CourseEditDialog.h"
#include "UiMessages.h"
#include "../include/course_io.h"


#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QTableWidget>
#include <QHeaderView>
#include <QAbstractItemView>
#include <QFileDialog>
#include <QDir>

CoursePage::CoursePage(AppSession* session, QWidget* parent)
    : QDialog(parent), session_(session) {
    setWindowTitle("课程管理");
    resize(900, 500);

    table_ = new QTableWidget(this);
    table_->setColumnCount(4);
    table_->setHorizontalHeaderLabels({"课程ID", "课程名", "学分", "先修课程"});
    table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    table_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    table_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    table_->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
    table_->verticalHeader()->setVisible(false);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setSelectionMode(QAbstractItemView::SingleSelection);
    table_->setWordWrap(false);

    auto* btnLoadTxt = new QPushButton("从文本文件载入到缓存", this);
    auto* btnLoadDb = new QPushButton("从数据库载入到缓存", this);
    auto* btnAdd = new QPushButton("新增课程", this);
    auto* btnEdit = new QPushButton("修改课程", this);
    auto* btnDelete = new QPushButton("删除课程", this);
    auto* btnDeleteDb = new QPushButton("从数据库删除", this);
    auto* btnSync = new QPushButton("同步到数据库", this);
    auto* btnRefresh = new QPushButton("按缓存刷新", this);
    auto* btnClose = new QPushButton("关闭", this);
    statusLabel_ = new QLabel(this);

    auto* btnRow = new QHBoxLayout;
    btnRow->addWidget(btnLoadTxt);
    btnRow->addWidget(btnLoadDb);
    btnRow->addWidget(btnAdd);
    btnRow->addWidget(btnEdit);
    btnRow->addWidget(btnDelete);
    btnRow->addWidget(btnDeleteDb);
    btnRow->addWidget(btnSync);
    btnRow->addWidget(btnRefresh);
    btnRow->addStretch();
    btnRow->addWidget(btnClose);

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(table_);
    layout->addLayout(btnRow);
    layout->addWidget(statusLabel_);

    connect(btnLoadTxt, &QPushButton::clicked, this, &CoursePage::loadFromTxtToCache);
    connect(btnLoadDb, &QPushButton::clicked, this, &CoursePage::loadFromDbToCache);
    connect(btnAdd, &QPushButton::clicked, this, &CoursePage::onAddCourse);
    connect(btnEdit, &QPushButton::clicked, this, &CoursePage::onEditCourse);
    connect(btnDelete, &QPushButton::clicked, this, &CoursePage::onDeleteCourse);
    connect(btnDeleteDb, &QPushButton::clicked, this, &CoursePage::onDeleteCourseFromDb);
    connect(btnSync, &QPushButton::clicked, this, &CoursePage::onSyncToDb);
    connect(btnRefresh, &QPushButton::clicked, this, &CoursePage::reloadFromCache);
    connect(btnClose, &QPushButton::clicked, this, &CoursePage::accept);

    reloadFromCache();
}

bool CoursePage::confirmReplaceCache(const QString& sourceName){
    if(!session_){
        return false;
    }

    if(session_->courseCache.empty()){
        return true;
    }

    const auto reply = QMessageBox::question(
        this,
        "确认替换缓存",
        QString("当前缓存中已加载 %1 门课程，\n是否从 %2 替换缓存？").arg(session_->courseCache.size()).arg(sourceName),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No
    );
    return reply == QMessageBox::Yes;
}

bool CoursePage::confirmUploadToDb() {
    if (!session_) {
        return false;
    }

    if (session_->courseCache.empty()) {
        UiMessages::info(this, "提示", "当前缓存为空，无需同步。");
        return false;
    }

    return UiMessages::confirmUpload(this, "课程", static_cast<int>(session_->courseCache.size()));
}


void CoursePage::loadFromTxtToCache(){
    if(!session_){
        statusLabel_->setText("会话无效");
        return;
    }

    if(!confirmReplaceCache("TXT 文件中的课程")){
        statusLabel_->setText("已取消 TXT 导入");
        return;
    }

    const QString defaultDir = QDir::currentPath() + "/data";
    const QString path = QFileDialog::getOpenFileName(
        this,
        "选择课程 TXT 文件",
        defaultDir,
        "Text Files (*.txt);;All Files (*)"
    );
    if(path.isEmpty()){
        statusLabel_->setText("未选择文件");
        return;
    }

    std::vector<Course> courses;
    std::string err;
    if(!importCoursesFromTxt(path.toStdString(), courses, err)){
        statusLabel_->setText(QString("导入 TXT 文件失败：%1").arg(QString::fromStdString(err)));
        return;
    }

    session_->courseCache = courses;
    session_->courseCacheLoaded = true;
    reloadFromCache();
    statusLabel_->setText(QString("已从 TXT 文件载入 %1 门课程到缓存").arg(courses.size()));
}

void CoursePage::reloadFromCache(){
    table_->setRowCount(0);

    if(!session_){
        statusLabel_->setText("会话无效");
        return;
    }

    const auto& courses = session_->courseCache;
    table_->setRowCount(static_cast<int>(courses.size()));
    for(int i = 0; i < static_cast<int>(courses.size()); ++i){
        const auto& c = courses[i];
        
        QString prereqText;
        for(int j = 0; j < static_cast<int>(c.prereq_ids.size()); ++j){
            prereqText += QString::fromStdString(c.prereq_ids[j]);
            if(j + 1 < static_cast<int>(c.prereq_ids.size())){
                prereqText += ", ";
            }
        }

        table_->setItem(i, 0, new QTableWidgetItem(QString::fromStdString(c.id)));
        table_->setItem(i, 1, new QTableWidgetItem(QString::fromStdString(c.name)));
        table_->setItem(i, 2, new QTableWidgetItem(QString::number(c.credit)));
        table_->setItem(i, 3, new QTableWidgetItem(prereqText));
    }

    statusLabel_->setText(QString("缓存中共 %1 门课程").arg(courses.size()));
}

void CoursePage::loadFromDbToCache() {
    if (!session_) {
        statusLabel_->setText("会话无效");
        return;
    }
    if (!session_->connected) {
        statusLabel_->setText("数据库未连接");
        return;
    }
    if (!confirmReplaceCache("数据库中的课程")) {
        statusLabel_->setText("已取消数据库导入");
        return;
    }

    std::vector<Course> courses;
    std::string err;
    if (!session_->db.listCoursesFromDb(courses, err)) {
        statusLabel_->setText(QString("读取数据库课程失败：%1").arg(QString::fromStdString(err)));
        return;
    }

    session_->courseCache = courses;
    session_->courseCacheLoaded = true;
    reloadFromCache();
    statusLabel_->setText(QString("已从数据库载入 %1 门课程到缓存").arg(courses.size()));
}

void CoursePage::onAddCourse() {
    if (!session_) {
        statusLabel_->setText("会话无效");
        return;
    }

    CourseEditDialog dlg(session_, this);
    if (dlg.exec() != QDialog::Accepted) {
        return;
    }

    session_->courseCache.push_back(dlg.course());
    session_->courseCacheLoaded = true;
    reloadFromCache();
    statusLabel_->setText("已新增课程到缓存");
}

void CoursePage::onEditCourse() {
    if (!session_) {
        statusLabel_->setText("会话无效");
        return;
    }

    const int row = table_->currentRow();
    if (row < 0 || row >= static_cast<int>(session_->courseCache.size())) {
        statusLabel_->setText("请先选中一门课程");
        return;
    }

    CourseEditDialog dlg(session_, this);
    dlg.setCourse(session_->courseCache[row]);
    if (dlg.exec() != QDialog::Accepted) {
        return;
    }

    session_->courseCache[row] = dlg.course();
    reloadFromCache();
    statusLabel_->setText("已修改缓存中的课程");
}

void CoursePage::onDeleteCourse() {
    if (!session_) {
        statusLabel_->setText("会话无效");
        return;
    }

    const int row = table_->currentRow();
    if (row < 0 || row >= static_cast<int>(session_->courseCache.size())) {
        statusLabel_->setText("请先选中一门课程");
        return;
    }

    const auto& c = session_->courseCache[row];
    if (!UiMessages::confirm(
            this,
            "确认删除",
            QString("确定从缓存中删除课程 %1 (%2) 吗？")
                .arg(QString::fromStdString(c.id))
                .arg(QString::fromStdString(c.name)))) {
        return;
    }

    session_->courseCache.erase(session_->courseCache.begin() + row);

    for (auto& item : session_->courseCache) {
        std::vector<std::string> kept;
        for (const auto& pre : item.prereq_ids) {
            if (pre != c.id) {
                kept.push_back(pre);
            }
        }
        item.prereq_ids = kept;
    }

    reloadFromCache();
    statusLabel_->setText("已从缓存中删除课程，并清理相关先修引用");
}

void CoursePage::onDeleteCourseFromDb() {
    if (!session_) {
        statusLabel_->setText("会话无效");
        return;
    }
    if (!session_->connected) {
        statusLabel_->setText("数据库未连接");
        return;
    }

    const int row = table_->currentRow();
    if (row < 0 || row >= static_cast<int>(session_->courseCache.size())) {
        statusLabel_->setText("请先选中一门课程");
        return;
    }

    const auto& c = session_->courseCache[row];
    CourseDeleteCheck check;
    std::string err;
    if (!session_->db.getCourseDeleteCheck(c.id, check, err)) {
        statusLabel_->setText(QString("读取课程引用失败：%1").arg(QString::fromStdString(err)));
        QMessageBox::critical(this, "读取失败", QString::fromStdString(err));
        return;
    }

    if (check.student_course_refs > 0) {
        const QString msg = QString(
            "课程 %1 (%2) 已被 student_course 引用 %3 次，不能从数据库删除。")
            .arg(QString::fromStdString(c.id))
            .arg(QString::fromStdString(c.name))
            .arg(check.student_course_refs);
        statusLabel_->setText(msg);
        UiMessages::warning(this, "禁止删除", msg);
        return;
    }

    bool remove_prereq_refs = false;
    QString msg = QString(
        "确定从数据库删除课程 %1 (%2) 吗？\n\n"
        "student_course 引用: %3\n"
        "该课程自己的先修映射: %4\n"
        "被其他课程作为先修引用: %5")
        .arg(QString::fromStdString(c.id))
        .arg(QString::fromStdString(c.name))
        .arg(check.student_course_refs)
        .arg(check.prereq_owner_refs)
        .arg(check.prereq_required_refs);

    if (check.prereq_required_refs > 0) {
        msg += "\n\n该课程正被其他课程作为先修课使用。\n若继续删除，将先清理这些先修引用。";
    }

    if (!UiMessages::confirm(this, "确认数据库删除", msg)) {
        statusLabel_->setText("已取消数据库删除");
        return;
    }
    remove_prereq_refs = (check.prereq_required_refs > 0);

    if (!session_->db.deleteCourseFromDb(c.id, remove_prereq_refs, nullptr, err)) {
        statusLabel_->setText(QString("数据库删除失败：%1").arg(QString::fromStdString(err)));
        UiMessages::error(this, "删除失败", QString::fromStdString(err));
        return;
    }

    session_->courseCache.erase(session_->courseCache.begin() + row);
    for (auto& item : session_->courseCache) {
        std::vector<std::string> kept;
        for (const auto& pre : item.prereq_ids) {
            if (pre != c.id) {
                kept.push_back(pre);
            }
        }
        item.prereq_ids = kept;
    }

    reloadFromCache();
    const QString ok_msg = QString("已从数据库删除课程 %1").arg(QString::fromStdString(c.id));
    statusLabel_->setText(ok_msg);
    UiMessages::info(this, "删除完成", ok_msg);
}

void CoursePage::onSyncToDb() {
    if (!session_) {
        statusLabel_->setText("会话无效");
        return;
    }

    if (!session_->connected) {
        statusLabel_->setText("数据库未连接");
        return;
    }

    if (!confirmUploadToDb()) {
        statusLabel_->setText("已取消同步到数据库");
        return;
    }

    CourseSyncStats stats;
    std::string err;
    if (!session_->db.upsertCourses(session_->courseCache, stats, err)) {
        statusLabel_->setText(QString("同步失败：%1").arg(QString::fromStdString(err)));
        UiMessages::error(this, "同步失败", QString::fromStdString(err));
        return;
    }

    statusLabel_->setText(
        QString("同步完成：新增 %1，更新/已存在 %2")
            .arg(stats.inserted)
            .arg(stats.updated_or_unchanged)
    );

    UiMessages::info(
        this,
        "同步完成",
        QString("已同步 %1 门课程到数据库。\n新增: %2\n更新/已存在: %3")
            .arg(session_->courseCache.size())
            .arg(stats.inserted)
            .arg(stats.updated_or_unchanged)
    );
}
