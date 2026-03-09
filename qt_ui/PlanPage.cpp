#include "PlanPage.h"
#include "AppSession.h"
#include "UiMessages.h"

#include "../include/course_io.h"
#include "../include/scheduler.h"

#include <QComboBox>
#include <QAbstractItemView>
#include <QCheckBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QTableWidget>
#include <QHeaderView>
#include <QVBoxLayout>
#include <QDir>

#include <algorithm>
#include <unordered_map>
#include <unordered_set>

namespace {

QString strategyToText(ScheduleStrategy strategy) {
    return strategy == ScheduleStrategy::FRONT_LOADED ? "前置集中" : "均匀负担";
}

ScheduleStrategy comboToStrategy(int index) {
    return index == 1 ? ScheduleStrategy::FRONT_LOADED : ScheduleStrategy::BALANCED;
}

int strategyToComboIndex(ScheduleStrategy strategy) {
    return strategy == ScheduleStrategy::FRONT_LOADED ? 1 : 0;
}

QString findCourseName(const std::vector<Course>& courses, const std::string& course_id) {
    for (const auto& c : courses) {
        if (c.id == course_id) {
            return QString::fromStdString(c.name);
        }
    }
    return QString();
}

int findCourseCredit(const std::vector<Course>& courses, const std::string& course_id) {
    for (const auto& c : courses) {
        if (c.id == course_id) {
            return c.credit;
        }
    }
    return 0;
}

std::unordered_map<std::string, Course> buildCourseMap(const std::vector<Course>& courses) {
    std::unordered_map<std::string, Course> course_map;
    for (const auto& c : courses) {
        course_map[c.id] = c;
    }
    return course_map;
}

std::vector<StudentCoursePlanRow> buildPlanRowsFromPlan(const std::vector<SemesterPlan>& plan) {
    std::vector<StudentCoursePlanRow> rows;
    for (size_t i = 0; i < plan.size(); ++i) {
        const int semester = static_cast<int>(i + 1);
        for (const auto& cid : plan[i].course_ids) {
            StudentCoursePlanRow row;
            row.course_id = cid;
            row.semester = semester;
            row.status = "PLANNED";
            rows.push_back(row);
        }
    }
    return rows;
}

std::vector<Course> buildPendingCoursesByCompleted(
    const std::vector<Course>& all_courses,
    const std::unordered_set<std::string>& completed_ids
) {
    std::vector<Course> pending;
    for (const auto& c : all_courses) {
        if (completed_ids.count(c.id) > 0) {
            continue;
        }

        Course copy = c;
        std::vector<std::string> kept_prereq;
        for (const auto& pre : c.prereq_ids) {
            if (completed_ids.count(pre) == 0) {
                kept_prereq.push_back(pre);
            }
        }
        copy.prereq_ids = kept_prereq;
        pending.push_back(copy);
    }
    return pending;
}

}  // namespace

PlanPage::PlanPage(AppSession* session, QWidget* parent)
    : QDialog(parent), session_(session) {
    setWindowTitle("排课参数配置");
    resize(720, 420);

    maxSemesterSpin_ = new QSpinBox(this);
    maxSemesterSpin_->setRange(1, 20);

    maxCreditSpin_ = new QSpinBox(this);
    maxCreditSpin_->setRange(1, 40);

    strategyCombo_ = new QComboBox(this);
    strategyCombo_->addItem("均匀负担");
    strategyCombo_->addItem("前置集中");

    studentCombo_ = new QComboBox(this);
    removeCompletedCheck_ = new QCheckBox("生成前剔除该学生已修课程", this);
    removeCompletedCheck_->setChecked(true);

    cacheSummaryLabel_ = new QLabel(this);
    cacheSummaryLabel_->setWordWrap(true);

    configSummaryLabel_ = new QLabel(this);
    configSummaryLabel_->setWordWrap(true);

    studentInfoLabel_ = new QLabel(this);
    studentInfoLabel_->setWordWrap(true);

    historyTable_ = new QTableWidget(this);
    historyTable_->setColumnCount(5);
    historyTable_->setHorizontalHeaderLabels({"学期", "课程号", "课程名", "状态", "成绩"});
    historyTable_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    historyTable_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    historyTable_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    historyTable_->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    historyTable_->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    historyTable_->verticalHeader()->setVisible(false);
    historyTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    historyTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    historyTable_->setSelectionMode(QAbstractItemView::SingleSelection);

    planSummaryLabel_ = new QLabel(this);
    planSummaryLabel_->setWordWrap(true);

    planTable_ = new QTableWidget(this);
    planTable_->setColumnCount(5);
    planTable_->setHorizontalHeaderLabels({"学期", "课程号", "课程名", "学分", "该学期总学分"});
    planTable_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    planTable_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    planTable_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    planTable_->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    planTable_->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    planTable_->verticalHeader()->setVisible(false);
    planTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    planTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    planTable_->setSelectionMode(QAbstractItemView::SingleSelection);

    statusLabel_ = new QLabel(this);
    statusLabel_->setWordWrap(true);

    auto* form = new QFormLayout;
    form->addRow("最大学期数:", maxSemesterSpin_);
    form->addRow("单学期学分上限:", maxCreditSpin_);
    form->addRow("排课策略:", strategyCombo_);
    form->addRow("学生:", studentCombo_);
    form->addRow("", removeCompletedCheck_);

    auto* configBox = new QGroupBox("排课参数", this);
    configBox->setLayout(form);

    auto* cacheBoxLayout = new QVBoxLayout;
    cacheBoxLayout->addWidget(cacheSummaryLabel_);
    auto* cacheBox = new QGroupBox("当前课程缓存概况", this);
    cacheBox->setLayout(cacheBoxLayout);

    auto* currentConfigLayout = new QVBoxLayout;
    currentConfigLayout->addWidget(configSummaryLabel_);
    auto* currentConfigBox = new QGroupBox("当前已保存配置", this);
    currentConfigBox->setLayout(currentConfigLayout);

    auto* studentBoxLayout = new QVBoxLayout;
    studentBoxLayout->addWidget(studentInfoLabel_);
    studentBoxLayout->addWidget(historyTable_);
    auto* studentBox = new QGroupBox("学生信息与历史课程", this);
    studentBox->setLayout(studentBoxLayout);

    auto* planBoxLayout = new QVBoxLayout;
    planBoxLayout->addWidget(planSummaryLabel_);
    planBoxLayout->addWidget(planTable_);
    auto* planBox = new QGroupBox("生成的课表", this);
    planBox->setLayout(planBoxLayout);

    auto* btnSave = new QPushButton("保存配置", this);
    auto* btnGenerate = new QPushButton("生成课表", this);
    auto* btnExport = new QPushButton("导出课表", this);
    auto* btnRefresh = new QPushButton("刷新概况", this);
    auto* btnRefreshStudents = new QPushButton("刷新学生列表", this);
    auto* btnClose = new QPushButton("关闭", this);

    auto* btnRow = new QHBoxLayout;
    btnRow->addWidget(btnSave);
    btnRow->addWidget(btnGenerate);
    btnRow->addWidget(btnExport);
    btnRow->addWidget(btnRefresh);
    btnRow->addWidget(btnRefreshStudents);
    btnRow->addStretch();
    btnRow->addWidget(btnClose);

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(configBox);
    layout->addWidget(cacheBox);
    layout->addWidget(currentConfigBox);
    layout->addWidget(studentBox);
    layout->addWidget(planBox);
    layout->addLayout(btnRow);
    layout->addWidget(statusLabel_);

    if (session_) {
        maxSemesterSpin_->setValue(session_->planConfig.max_semesters);
        maxCreditSpin_->setValue(session_->planConfig.max_credits_per_semester);
        strategyCombo_->setCurrentIndex(strategyToComboIndex(session_->planConfig.strategy));
    }

    connect(btnSave, &QPushButton::clicked, this, &PlanPage::onSaveConfig);
    connect(btnGenerate, &QPushButton::clicked, this, &PlanPage::onGeneratePlan);
    connect(btnExport, &QPushButton::clicked, this, &PlanPage::onExportPlan);
    connect(btnRefresh, &QPushButton::clicked, this, &PlanPage::reloadSummary);
    connect(btnRefreshStudents, &QPushButton::clicked, this, &PlanPage::reloadStudents);
    connect(studentCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &PlanPage::onStudentChanged);
    connect(btnClose, &QPushButton::clicked, this, &PlanPage::accept);

    reloadSummary();
    reloadStudents();
    reloadGeneratedPlanView();
}

void PlanPage::reloadSummary() {
    if (!session_) {
        cacheSummaryLabel_->setText("会话无效");
        configSummaryLabel_->setText("会话无效");
        statusLabel_->setText("会话无效");
        return;
    }

    int totalCredits = 0;
    int withPrereqCount = 0;
    for (const auto& c : session_->courseCache) {
        totalCredits += c.credit;
        if (!c.prereq_ids.empty()) {
            ++withPrereqCount;
        }
    }

    cacheSummaryLabel_->setText(
        QString("缓存课程数: %1\n总学分: %2\n含先修课程数: %3\n最近生成计划学期数: %4")
            .arg(session_->courseCache.size())
            .arg(totalCredits)
            .arg(withPrereqCount)
            .arg(session_->generatedPlan.size())
    );

    configSummaryLabel_->setText(
        QString("最大学期数: %1\n单学期学分上限: %2\n排课策略: %3")
            .arg(session_->planConfig.max_semesters)
            .arg(session_->planConfig.max_credits_per_semester)
            .arg(strategyToText(session_->planConfig.strategy))
    );
}

void PlanPage::reloadStudents() {
    studentCombo_->clear();
    historyTable_->setRowCount(0);

    if (!session_) {
        studentInfoLabel_->setText("会话无效");
        statusLabel_->setText("会话无效");
        return;
    }
    if (!session_->connected) {
        studentInfoLabel_->setText("数据库未连接");
        statusLabel_->setText("数据库未连接");
        return;
    }

    std::vector<StudentInfo> students;
    std::string err;
    if (!session_->db.listStudents(students, err)) {
        studentInfoLabel_->setText("学生列表读取失败");
        statusLabel_->setText(QString("读取学生失败：%1").arg(QString::fromStdString(err)));
        return;
    }

    studentCombo_->addItem("请选择学生", "");
    int targetIndex = 0;
    for (const auto& s : students) {
        const QString text = QString("%1 | %2 | %3 | %4")
            .arg(QString::fromStdString(s.student_id))
            .arg(QString::fromStdString(s.name))
            .arg(QString::fromStdString(s.major))
            .arg(s.grade);
        studentCombo_->addItem(text, QString::fromStdString(s.student_id));
        if (!session_->selectedStudentId.empty() && s.student_id == session_->selectedStudentId) {
            targetIndex = studentCombo_->count() - 1;
        }
    }

    if (students.empty()) {
        studentInfoLabel_->setText("当前数据库中没有学生记录");
        statusLabel_->setText("学生列表为空");
        return;
    }

    studentCombo_->setCurrentIndex(targetIndex == 0 ? 1 : targetIndex);
    statusLabel_->setText(QString("已加载 %1 名学生").arg(students.size()));
}

void PlanPage::onStudentChanged(int index) {
    if (!session_) {
        return;
    }

    const std::string sid = studentCombo_->itemData(index).toString().toStdString();
    session_->selectedStudentId = sid;

    if (sid.empty()) {
        studentInfoLabel_->setText("未选择学生");
        historyTable_->setRowCount(0);
        return;
    }

    StudentInfo student;
    bool found = false;
    std::string err;
    if (!session_->db.getStudentById(sid, student, found, err)) {
        studentInfoLabel_->setText("读取学生失败");
        historyTable_->setRowCount(0);
        statusLabel_->setText(QString("读取学生失败：%1").arg(QString::fromStdString(err)));
        return;
    }
    if (!found) {
        studentInfoLabel_->setText("学生不存在，可能已被删除");
        historyTable_->setRowCount(0);
        statusLabel_->setText("学生不存在");
        return;
    }

    studentInfoLabel_->setText(
        QString("学号: %1\n姓名: %2\n主修: %3\n年级: %4")
            .arg(QString::fromStdString(student.student_id))
            .arg(QString::fromStdString(student.name))
            .arg(QString::fromStdString(student.major))
            .arg(student.grade)
    );

    loadStudentHistory(sid);
}

void PlanPage::loadStudentHistory(const std::string& student_id) {
    historyTable_->setRowCount(0);

    if (!session_ || student_id.empty()) {
        return;
    }

    std::vector<LearnedCourse> history;
    std::string err;
    if (!session_->db.listStudentCourses(student_id, history, err)) {
        statusLabel_->setText(QString("读取历史课程失败：%1").arg(QString::fromStdString(err)));
        return;
    }

    historyTable_->setRowCount(static_cast<int>(history.size()));
    for (int i = 0; i < static_cast<int>(history.size()); ++i) {
        const auto& r = history[i];
        historyTable_->setItem(i, 0, new QTableWidgetItem(QString::number(r.semester)));
        historyTable_->setItem(i, 1, new QTableWidgetItem(QString::fromStdString(r.course_id)));
        historyTable_->setItem(i, 2, new QTableWidgetItem(QString::fromStdString(r.course_name)));
        historyTable_->setItem(i, 3, new QTableWidgetItem(QString::fromStdString(r.status)));
        historyTable_->setItem(i, 4, new QTableWidgetItem(r.has_score ? QString::number(r.score) : "NULL"));
    }

    statusLabel_->setText(QString("已载入该学生 %1 条课程记录").arg(history.size()));
}

void PlanPage::onSaveConfig() {
    if (!session_) {
        statusLabel_->setText("会话无效");
        return;
    }

    session_->planConfig.max_semesters = maxSemesterSpin_->value();
    session_->planConfig.max_credits_per_semester = maxCreditSpin_->value();
    session_->planConfig.strategy = comboToStrategy(strategyCombo_->currentIndex());

    reloadSummary();
    statusLabel_->setText("排课参数已保存到当前会话");
    UiMessages::info(this, "保存成功", "排课参数已保存。");
}

void PlanPage::onGeneratePlan() {
    if (!session_) {
        statusLabel_->setText("会话无效");
        return;
    }
    if (session_->courseCache.empty()) {
        statusLabel_->setText("当前课程缓存为空，无法生成课表");
        UiMessages::warning(this, "无法生成", "当前课程缓存为空，请先导入课程。");
        return;
    }

    session_->planConfig.max_semesters = maxSemesterSpin_->value();
    session_->planConfig.max_credits_per_semester = maxCreditSpin_->value();
    session_->planConfig.strategy = comboToStrategy(strategyCombo_->currentIndex());

    std::vector<Course> schedulingCourses = session_->courseCache;
    int completedCount = 0;

    if (removeCompletedCheck_->isChecked()) {
        if (!session_->connected) {
            statusLabel_->setText("数据库未连接");
            return;
        }
        if (session_->selectedStudentId.empty()) {
            statusLabel_->setText("请选择学生后再生成课表");
            UiMessages::warning(this, "缺少学生", "已启用“剔除已修课程”，请先选择学生。");
            return;
        }

        std::vector<LearnedCourse> history;
        std::string err;
        if (!session_->db.listStudentCourses(session_->selectedStudentId, history, err)) {
            statusLabel_->setText(QString("读取学生历史失败：%1").arg(QString::fromStdString(err)));
            UiMessages::error(this, "读取失败", QString::fromStdString(err));
            return;
        }

        std::unordered_set<std::string> completedIds;
        for (const auto& row : history) {
            if (row.status == "COMPLETED") {
                completedIds.insert(row.course_id);
            }
        }
        completedCount = static_cast<int>(completedIds.size());
        schedulingCourses = buildPendingCoursesByCompleted(session_->courseCache, completedIds);
    }

    if (schedulingCourses.empty()) {
        session_->generatedPlan.clear();
        reloadSummary();
        reloadGeneratedPlanView();
        statusLabel_->setText("当前无待排课程");
        UiMessages::info(this, "生成完成", "当前无待排课程。");
        return;
    }

    std::string err;
    if (!generateSemesterPlan(schedulingCourses, session_->planConfig, session_->generatedPlan, err)) {
        statusLabel_->setText(QString("生成课表失败：%1").arg(QString::fromStdString(err)));
        UiMessages::error(this, "生成失败", QString::fromStdString(err));
        return;
    }

    int plannedCourseCount = 0;
    int plannedCreditCount = 0;
    for (const auto& semester : session_->generatedPlan) {
        plannedCourseCount += static_cast<int>(semester.course_ids.size());
        plannedCreditCount += semester.total_credits;
    }

    reloadSummary();
    reloadGeneratedPlanView();
    const QString msg = QString(
        "排课成功。\n生成学期数: %1\n待排课程数: %2\n待排总学分: %3\n剔除已修课程数: %4")
        .arg(session_->generatedPlan.size())
        .arg(plannedCourseCount)
        .arg(plannedCreditCount)
        .arg(completedCount);
    statusLabel_->setText(msg);
    UiMessages::info(this, "生成完成", msg);
}

void PlanPage::reloadGeneratedPlanView() {
    planTable_->setRowCount(0);

    if (!session_) {
        planSummaryLabel_->setText("会话无效");
        return;
    }

    if (session_->generatedPlan.empty()) {
        planSummaryLabel_->setText("当前还没有生成课表。");
        return;
    }

    int rowCount = 0;
    int totalCredits = 0;
    for (const auto& semester : session_->generatedPlan) {
        rowCount += std::max<int>(1, static_cast<int>(semester.course_ids.size()));
        totalCredits += semester.total_credits;
    }

    planTable_->setRowCount(rowCount);

    int row = 0;
    for (size_t i = 0; i < session_->generatedPlan.size(); ++i) {
        const auto& semester = session_->generatedPlan[i];
        if (semester.course_ids.empty()) {
            planTable_->setItem(row, 0, new QTableWidgetItem(QString::number(static_cast<int>(i + 1))));
            planTable_->setItem(row, 1, new QTableWidgetItem("-"));
            planTable_->setItem(row, 2, new QTableWidgetItem("无课程"));
            planTable_->setItem(row, 3, new QTableWidgetItem("0"));
            planTable_->setItem(row, 4, new QTableWidgetItem(QString::number(semester.total_credits)));
            ++row;
            continue;
        }

        for (size_t j = 0; j < semester.course_ids.size(); ++j) {
            const std::string& cid = semester.course_ids[j];
            planTable_->setItem(row, 0, new QTableWidgetItem(QString::number(static_cast<int>(i + 1))));
            planTable_->setItem(row, 1, new QTableWidgetItem(QString::fromStdString(cid)));
            planTable_->setItem(row, 2, new QTableWidgetItem(findCourseName(session_->courseCache, cid)));
            planTable_->setItem(row, 3, new QTableWidgetItem(QString::number(findCourseCredit(session_->courseCache, cid))));
            planTable_->setItem(
                row,
                4,
                new QTableWidgetItem(j == 0 ? QString::number(semester.total_credits) : QString())
            );
            ++row;
        }
    }

    planSummaryLabel_->setText(
        QString("已生成 %1 个学期的课表，总学分 %2。")
            .arg(session_->generatedPlan.size())
            .arg(totalCredits)
    );
}

void PlanPage::onExportPlan() {
    if (!session_) {
        statusLabel_->setText("会话无效");
        return;
    }
    if (session_->generatedPlan.empty()) {
        statusLabel_->setText("当前没有可导出的课表");
        UiMessages::warning(this, "无法导出", "请先生成课表。");
        return;
    }

    const QString defaultPath = QDir::currentPath() + "/output/plan.txt";
    const QString path = QFileDialog::getSaveFileName(
        this,
        "导出课表",
        defaultPath,
        "Text Files (*.txt);;All Files (*)"
    );
    if (path.isEmpty()) {
        statusLabel_->setText("已取消导出");
        return;
    }

    std::string err;
    if (!exportPlanToTxt(path.toStdString(), session_->generatedPlan, buildCourseMap(session_->courseCache), err)) {
        statusLabel_->setText(QString("导出失败：%1").arg(QString::fromStdString(err)));
        UiMessages::error(this, "导出失败", QString::fromStdString(err));
        return;
    }

    statusLabel_->setText(QString("已导出课表到 %1").arg(path));

    if (!UiMessages::confirm(
            this,
            "导出成功",
            QString("已导出到：\n%1\n\n是否继续写入数据库 student_course？").arg(path))) {
        return;
    }

    if (!session_->connected) {
        statusLabel_->setText("数据库未连接");
        UiMessages::warning(this, "无法写库", "数据库未连接。");
        return;
    }
    if (session_->selectedStudentId.empty()) {
        statusLabel_->setText("未选择学生，无法写入 student_course");
        UiMessages::warning(this, "无法写库", "请先选择学生。");
        return;
    }

    QStringList modes;
    modes << "追加去重写入" << "覆盖当前学生计划后写入" << "取消";
    bool ok = false;
    const QString selectedMode = QInputDialog::getItem(
        this,
        "写入数据库",
        "请选择写入模式：",
        modes,
        0,
        false,
        &ok
    );
    if (!ok || selectedMode == "取消") {
        statusLabel_->setText("已取消写入数据库");
        return;
    }

    int deleted = 0;
    if (selectedMode == "覆盖当前学生计划后写入") {
        if (!session_->db.deleteStudentPlannedRows(session_->selectedStudentId, deleted, err)) {
            statusLabel_->setText(QString("清理旧计划失败：%1").arg(QString::fromStdString(err)));
            UiMessages::error(this, "写入失败", QString::fromStdString(err));
            return;
        }
    }

    PlanWriteStats stats;
    const auto rows = buildPlanRowsFromPlan(session_->generatedPlan);
    if (!session_->db.insertStudentPlanRowsDedup(session_->selectedStudentId, rows, stats, err)) {
        statusLabel_->setText(QString("写入 student_course 失败：%1").arg(QString::fromStdString(err)));
        UiMessages::error(this, "写入失败", QString::fromStdString(err));
        return;
    }

    const QString msg = QString("写入完成。\n新增: %1\n重复跳过: %2\n删除旧计划: %3")
        .arg(stats.inserted)
        .arg(stats.duplicated)
        .arg(deleted);
    statusLabel_->setText(msg);
    UiMessages::info(this, "写入完成", msg);

    loadStudentHistory(session_->selectedStudentId);
}
