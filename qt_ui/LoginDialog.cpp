#include "LoginDialog.h"
#include "AppSession.h"

#include <QVBoxLayout>
#include <QFormLayout>
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>
//构造函数接收一个 AppSession 指针（用于保存登录信息）和一个父窗口指针。
//初始化父类 QDialog，并将传入的 session 保存到成员变量 session_ 中。
LoginDialog::LoginDialog(AppSession* session, QWidget* parent)
    : QDialog(parent), session_(session) {
    setWindowTitle("数据库登录");

    userEdit_ = new QLineEdit(this);
    passEdit_ = new QLineEdit(this);
    passEdit_->setEchoMode(QLineEdit::Password);
    msgLabel_ = new QLabel(this);
    connectBtn_ = new QPushButton("连接", this);
    //将“用户”和“密码”标签与对应的输入框配对，形成表单。
    auto* form = new QFormLayout;
    form->addRow("MySQL 用户:", userEdit_);
    form->addRow("MySQL 密码:", passEdit_);
    //将表单、按钮和消息标签垂直排列。
    auto* layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addWidget(connectBtn_);
    layout->addWidget(msgLabel_);
    //信号槽连接：将按钮的 clicked 信号连接到 onConnectClicked 槽函数。
    connect(connectBtn_, &QPushButton::clicked, this, &LoginDialog::onConnectClicked);
}

void LoginDialog::onConnectClicked(){
    //从输入框中获取用户名和密码，并赋值给 session_ 对象的 uid 和 pwd。
    session_->uid = userEdit_->text().toStdString();
    session_->pwd = passEdit_->text().toStdString();
    //调用 session_->connect(err) 尝试连接数据库，连接结果存入 connected，错误信息通过 err 返回
    std::string err;
    if(session_->connect(err)){
        accept();
    }else{
        msgLabel_->setText(QString("连接失败: %1").arg(QString::fromStdString(err)));
    }
}