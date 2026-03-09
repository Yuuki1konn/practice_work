#include "MainWindow.h"
#include "AppSession.h"
#include "StudentPage.h"
#include "CoursePage.h"
#include "PlanPage.h"

#include <QWidget>
#include <QVBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QMessageBox>

MainWindow::MainWindow(AppSession* session, QWidget* parent)
: QMainWindow(parent), session_(session) {
    //设置窗口基本属性
    setWindowTitle("大学课程安排系统 - Qt");
    resize(900,600);
    //创建中央部件与布局
    auto* central = new QWidget(this);
    auto* layout = new QVBoxLayout(central);
    //创建界面元素
    infoLabel_ = new QLabel(this);
    infoLabel_->setText(QString("当前数据库用户: %1").arg(QString::fromStdString(session_->uid)));
    //按钮
    auto* btnStudent = new QPushButton("学生管理",this);
    auto* btnCourse = new QPushButton("课程管理",this);
    auto* btnPlan = new QPushButton("计划管理",this);
    //将标签和按钮依次添加到垂直布局中，最后添加一个伸缩空间（addStretch()）使按钮组靠上对齐
    layout->addWidget(infoLabel_);
    layout->addWidget(btnStudent);
    layout->addWidget(btnCourse);
    layout->addWidget(btnPlan);
    layout->addStretch();
    //设置中央部件
    setCentralWidget(central);
    //连接信号槽
    connect(btnStudent, &QPushButton::clicked, this, [this](){
       StudentPage dlg(session_, this);
       dlg.exec();
    });
    connect(btnCourse, &QPushButton::clicked, this, [this](){
    CoursePage dlg(session_, this);
    dlg.exec();
    });
    connect(btnPlan, &QPushButton::clicked, this, [this](){
        PlanPage dlg(session_, this);
        dlg.exec();
    });
}
