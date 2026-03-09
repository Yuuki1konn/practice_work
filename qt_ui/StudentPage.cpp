#include "StudentPage.h"
#include "AppSession.h"
#include "StudentEditDialog.h"
#include "StudentCoursePage.h"
#include "UiMessages.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QTableWidget>
#include <QHeaderView>
#include <QAbstractItemView>
#include <QString>
// 构造函数，接收 AppSession 指针和父窗口指针
StudentPage::StudentPage(AppSession* session, QWidget* parent)
    : QDialog(parent), session_(session){// 初始化基类和成员变量
        // 设置窗口标题和初始大小
    setWindowTitle("学生管理");
    resize(760,460);
        // 创建表格控件，设置4列
    table_ = new QTableWidget(this);
    table_->setColumnCount(4);
     // 设置水平表头标签
    table_->setHorizontalHeaderLabels({"学号", "姓名", "主修", "年级"});
     // 设置列宽模式：均匀拉伸填充整个表格
    table_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    // 禁止用户直接编辑表格内容
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    // 设置选择行为：选中整行
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    // 设置选择模式：只能单选
    table_->setSelectionMode(QAbstractItemView::SingleSelection);
    // 创建“刷新”和“关闭”按钮
    auto* btnAdd = new QPushButton("新增", this);
    auto* btnEdit = new QPushButton("修改", this);
    auto* btnDelete = new QPushButton("删除", this);
    auto* btnRefresh = new QPushButton("刷新", this);
    auto* btnViewCourses = new QPushButton("查看课程", this);
    auto* btnClose = new QPushButton("关闭", this);
    statusLabel_ = new QLabel(this);// 创建状态标签，用于显示当前信息（如连接状态、记录数）
    // 创建一个水平布局用于放置按钮
    auto* btnRow = new QHBoxLayout;
    btnRow->addWidget(btnAdd);
    btnRow->addWidget(btnEdit);
    btnRow->addWidget(btnDelete);
    btnRow->addWidget(btnViewCourses);
    btnRow->addWidget(btnRefresh);
    btnRow->addStretch();
    btnRow->addWidget(btnClose);
    // 创建主垂直布局，将表格、按钮行、状态标签依次加入
    auto* layout = new QVBoxLayout(this);
    layout->addWidget(table_);
    layout->addLayout(btnRow);
    layout->addWidget(statusLabel_);
    // 连接信号槽 ：点击“刷新”按钮时，调用 reloadStudents 槽函数
    connect(btnAdd, &QPushButton::clicked, this, &StudentPage::onAddStudent);
    connect(btnEdit, &QPushButton::clicked, this, &StudentPage::onEditStudent);
    connect(btnDelete, &QPushButton::clicked, this, &StudentPage::onDeleteStudent);
    connect(btnViewCourses, &QPushButton::clicked, this, &StudentPage::onViewCourses);
    connect(btnRefresh, &QPushButton::clicked, this, &StudentPage::reloadStudents);
    // 点击“关闭”按钮时，关闭当前窗口
    connect(btnClose, &QPushButton::clicked, this, &StudentPage::close);
    // 初始化加载学生数据
    reloadStudents();
}
// reloadStudents 槽函数：从数据库重新加载学生数据并更新表格
void StudentPage::reloadStudents(){
    // 清空表格所有行
    table_->setRowCount(0);
    // 检查会话是否有效以及数据库是否已连接
    if(!session_ || !session_->connected){
        statusLabel_->setText("数据库未连接");
        return;
    }
    
    std::vector<StudentInfo> students;
    std::string err;
    // 调用会话的 listStudents 方法获取学生列表，失败时 err 包含错误信息
    if(!session_->db.listStudents(students, err)){
        statusLabel_->setText(QString("读取失败：%1").arg(QString::fromStdString(err)));
        return;
    }
    // 设置表格行数为获取到的学生数量
    table_->setRowCount(static_cast<int>(students.size()));
    // 遍历学生信息，填充表格每一行
    for(int i = 0; i < static_cast<int>(students.size()); ++i){
        const auto& s = students[i];
        // 为每一列创建新的 QTableWidgetItem，并将学生数据转换为 QString 后设置
        table_->setItem(i, 0, new QTableWidgetItem(QString::fromStdString(s.student_id)));
        table_->setItem(i, 1, new QTableWidgetItem(QString::fromStdString(s.name)));
        table_->setItem(i, 2, new QTableWidgetItem(QString::fromStdString(s.major)));
        // 年级是整数，用 number 转换
        table_->setItem(i, 3, new QTableWidgetItem(QString::number(s.grade)));
    }
    // 更新状态标签，显示记录总数
    statusLabel_->setText(QString("共 %1 条记录").arg(students.size()));
}

// onAddStudent 槽函数：响应添加按钮点击，打开新增学生对话框
void StudentPage::onAddStudent(){
    if(!session_ || !session_->connected){
        statusLabel_->setText("数据库未连接");
        return;
    }
    
    StudentEditDialog dlg(this);
    if(dlg.exec() != QDialog::Accepted){
        return;
    }

    std::string err;
    if(!session_->db.addStudent(dlg.student(),err)){
        statusLabel_->setText(QString("添加失败：%1").arg(QString::fromStdString(err)));
        return;
    }

    reloadStudents();
    statusLabel_->setText("新增学生成功");
}

void StudentPage::onEditStudent(){
    if(!session_ || !session_->connected){
        statusLabel_->setText("数据库未连接");
        return;
    }

    const int row = table_->currentRow();
    if(row < 0){
        statusLabel_->setText("请选择要修改的学生");
        return;
    }

    const auto * item = table_->item(row,0);
    if(!item){
        statusLabel_->setText("无法读取当前选中学生");
        return;
    }

    const std::string sid = item->text().trimmed().toStdString();
    StudentInfo s;
    bool found = false;
    std::string err;
    if(!session_->db.getStudentById(sid, s, found, err)){
        statusLabel_->setText(QString("读取学生失败：%1").arg(QString::fromStdString(err)));
        return;
    }
    if(!found){
        statusLabel_->setText("学生不存在,可能已被删除");
        reloadStudents();
        return;
    }

    StudentEditDialog dlg(this);
    dlg.setStudent(s);
    if(dlg.exec() != QDialog::Accepted){
        return;
    }

    if(!session_->db.updateStudent(dlg.student(), err)){
        statusLabel_->setText(QString("修改失败：%1").arg(QString::fromStdString(err)));
        return;
    }

    reloadStudents();
    statusLabel_->setText("修改学生成功");
}

void StudentPage::onDeleteStudent(){
    if(!session_ || !session_->connected){
        statusLabel_->setText("数据库未连接");
        return;
    }

    const int row = table_->currentRow();
    if(row < 0){
        statusLabel_->setText("请选择要删除的学生");
        return;
    }

    const auto* idItem = table_->item(row,0);
    const auto* nameItem = table_->item(row,1);
    if(!idItem){
        statusLabel_->setText("无法读取当前选中学生");
        return;
    }

    const QString sid = idItem->text().trimmed();
    const QString sname = nameItem ? nameItem->text().trimmed() : QString();
    
    if(!UiMessages::confirm(
            this,
            "确认删除",
            QString("确认删除学生 %1 (%2) 吗？\n如果该学生已有选课记录，数据库可能拒绝删除。").arg(sid, sname))){
        return;
    }
    
    std::string err;
    if(!session_->db.deleteStudent(sid.toStdString(), err)){
        statusLabel_->setText(QString("删除失败：%1").arg(QString::fromStdString(err)));
        return;
    }

    reloadStudents();
    statusLabel_->setText("删除学生成功");
}

void StudentPage::onViewCourses(){
    if(!session_ || !session_->connected){
        statusLabel_->setText("数据库未连接");
        return;
    }

    const int row = table_->currentRow();
    if(row < 0){
        statusLabel_->setText("请选择要查看课程的学生");
        return;
    }

    const auto* idItem = table_->item(row,0);
    if(!idItem){
        statusLabel_->setText("无法读取当前选中学生");
        return;
    }

    const std::string sid = idItem->text().trimmed().toStdString();
    StudentInfo s;
    bool found = false;
    std::string err;
    if(!session_->db.getStudentById(sid, s, found, err)){
        statusLabel_->setText(QString("读取学生失败：%1").arg(QString::fromStdString(err)));
        return;
    }
    if(!found){
        statusLabel_->setText("学生不存在,可能已被删除");
        reloadStudents();
        return;
    }

    StudentCoursePage dlg(session_, s, this);
    dlg.exec();
}
