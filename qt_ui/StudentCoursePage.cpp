#include "StudentCoursePage.h"
#include "AppSession.h"

#include <QVBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QTableWidget>
#include <QHeaderView>
#include <QAbstractItemView>

StudentCoursePage::StudentCoursePage(AppSession* session,
                                     const StudentInfo& student,
                                     QWidget* parent)
    : QDialog(parent), session_(session), student_(student) {
    setWindowTitle("学生课程情况");
    resize(860, 480);

    infoLabel_ = new QLabel(this);
    infoLabel_->setText(QString("学生：%1 | %2 | %3 | %4")
        .arg(QString::fromStdString(student_.student_id))
        .arg(QString::fromStdString(student_.name))
        .arg(QString::fromStdString(student_.major))
        .arg(student_.grade));

    table_ = new QTableWidget(this);
    table_->setColumnCount(5);
    table_->setHorizontalHeaderLabels({"学期", "课程号", "课程名", "状态", "成绩"});
    table_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setSelectionMode(QAbstractItemView::SingleSelection);

    statusLabel_ = new QLabel(this);
    auto* btnClose = new QPushButton("关闭", this);
    
    auto* layout = new QVBoxLayout(this);
    layout->addWidget(infoLabel_);
    layout->addWidget(table_);
    layout->addWidget(statusLabel_);
    layout->addWidget(btnClose);

    connect(btnClose, &QPushButton::clicked, this, &StudentCoursePage::accept);

    loadCourses();
}

void StudentCoursePage::loadCourses() {
    table_->setRowCount(0);

    if(!session_ || !session_->connected) {
        statusLabel_->setText("数据库未连接");
        return;
    }
    
    std::vector<LearnedCourse> history;
    std::string err;
    if(!session_->db.listStudentCourses(student_.student_id, history, err)){
        statusLabel_->setText(QString("读取课程记录失败：%1").arg(QString::fromStdString(err)));
        return;
    }
    
    table_->setRowCount(static_cast<int>(history.size()));
    for(int i = 0; i< static_cast<int>(history.size()); ++i) {
        const auto& r = history[i];
        table_->setItem(i, 0, new QTableWidgetItem(QString::number(r.semester)));
        table_->setItem(i, 1, new QTableWidgetItem(QString::fromStdString(r.course_id)));
        table_->setItem(i, 2, new QTableWidgetItem(QString::fromStdString(r.course_name)));
        table_->setItem(i, 3, new QTableWidgetItem(QString::fromStdString(r.status)));
        if(r.has_score){
            table_->setItem(i, 4, new QTableWidgetItem(QString::number(r.score)));
        }else{
            table_->setItem(i, 4, new QTableWidgetItem("NULL"));
        }
    }

    statusLabel_->setText(QString("共 %1 条课程记录").arg(history.size()));
}