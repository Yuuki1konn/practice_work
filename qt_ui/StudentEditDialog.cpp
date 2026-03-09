#include "StudentEditDialog.h"

#include <QFormLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>

StudentEditDialog::StudentEditDialog(QWidget* parent)
    : QDialog(parent) {
    setWindowTitle("新增学生");
    resize(420, 240);

    idEdit_ = new QLineEdit(this);
    nameEdit_ = new QLineEdit(this);
    majorEdit_ = new QLineEdit(this);
    gradeEdit_ = new QLineEdit(this);

    msgLabel_ = new QLabel(this);
    saveBtn_ = new QPushButton("保存", this);
    auto* cancelBtn_ = new QPushButton("取消", this);

    auto* form = new QFormLayout;
    form->addRow("学号:", idEdit_);
    form->addRow("姓名:", nameEdit_);
    form->addRow("主修:", majorEdit_);
    form->addRow("年级:", gradeEdit_);

    auto* btnRow = new QHBoxLayout;
    btnRow->addStretch();
    btnRow->addWidget(saveBtn_);
    btnRow->addWidget(cancelBtn_);

    auto* layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addWidget(msgLabel_);
    layout->addLayout(btnRow);
    
    connect(saveBtn_, &QPushButton::clicked, this, &StudentEditDialog::onSaveClicked);
    connect(cancelBtn_, &QPushButton::clicked, this, &StudentEditDialog::reject);
}

void StudentEditDialog::setStudent(const StudentInfo& s) {
    idEdit_->setText(QString::fromStdString(s.student_id));
    idEdit_->setReadOnly(true);
    nameEdit_->setText(QString::fromStdString(s.name));
    majorEdit_->setText(QString::fromStdString(s.major));
    gradeEdit_->setText(QString::number(s.grade));
    setWindowTitle("修改学生");
}

StudentInfo StudentEditDialog::student() const {
    StudentInfo s;
    s.student_id = idEdit_->text().trimmed().toStdString();
    s.name = nameEdit_->text().trimmed().toStdString();
    s.major = majorEdit_->text().trimmed().toStdString();
    s.grade = gradeEdit_->text().trimmed().toInt();
    return s;
}

void StudentEditDialog::onSaveClicked() {
    bool ok = false;
    const int grade = gradeEdit_->text().trimmed().toInt(&ok);
    
    if(idEdit_->text().trimmed().isEmpty()){
        msgLabel_->setText("学号不能为空");
        return;
    }
    
    if(nameEdit_->text().trimmed().isEmpty()){
        msgLabel_->setText("姓名不能为空");
        return;
    }
    
    if(majorEdit_->text().trimmed().isEmpty()){
        msgLabel_->setText("主修不能为空");
        return;
    }
    
    if(!ok || grade <= 0){
        msgLabel_->setText("年级必须是正整数");
        return;
    }

    accept();
}
