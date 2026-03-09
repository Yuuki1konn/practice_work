#include "CourseEditDialog.h"
#include "AppSession.h"

#include <QFormLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>
#include <QStringList>

#include <cctype>
#include <unordered_set>

static std::string trimStd(const std::string& s){
    size_t l = 0;
    size_t r = s.size();
    while(l < r && std::isspace(static_cast<unsigned char>(s[l]))) ++l;
    while(r > l && std::isspace(static_cast<unsigned char>(s[r-1]))) --r;
    return s.substr(l, r-l);
}
CourseEditDialog::CourseEditDialog(AppSession* session, QWidget* parent)
    : QDialog(parent), session_(session) {
    setWindowTitle("新增课程");
    resize(460, 260);

    idEdit_ = new QLineEdit(this);
    nameEdit_ = new QLineEdit(this);
    creditEdit_ = new QLineEdit(this);
    prereqEdit_ = new QLineEdit(this);

    msgLabel_ = new QLabel(this);
    saveBtn_ = new QPushButton("保存", this);
    auto* cancelBtn = new QPushButton("取消", this);

    auto* form = new QFormLayout;
    form->addRow("课程ID:", idEdit_);
    form->addRow("课程名:", nameEdit_);
    form->addRow("学分:", creditEdit_);
    form->addRow("先修课程:", prereqEdit_);

    auto* tips = new QLabel("先修课程用英文逗号分隔，例如: C01,C02", this);

    auto* btnRow = new QHBoxLayout;
    btnRow->addStretch();
    btnRow->addWidget(saveBtn_);
    btnRow->addWidget(cancelBtn);

    auto* layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addWidget(tips);
    layout->addWidget(msgLabel_);
    layout->addLayout(btnRow);

    connect(saveBtn_, &QPushButton::clicked, this, &CourseEditDialog::onSaveClicked);
    connect(cancelBtn, &QPushButton::clicked, this, &CourseEditDialog::reject);
}

void CourseEditDialog::setCourse(const Course& c) {
    editMode_ = true;
    setWindowTitle("修改课程");

    idEdit_->setText(QString::fromStdString(c.id));
    idEdit_->setReadOnly(true);
    nameEdit_->setText(QString::fromStdString(c.name));
    creditEdit_->setText(QString::number(c.credit));

    QStringList ids;
    for (const auto& pre : c.prereq_ids) {
        ids << QString::fromStdString(pre);
    }
    prereqEdit_->setText(ids.join(","));
}


Course CourseEditDialog::course() const {
    Course c;
    c.id = idEdit_->text().trimmed().toStdString();
    c.name = nameEdit_->text().trimmed().toStdString();
    c.credit = creditEdit_->text().trimmed().toInt();

    const QStringList parts = prereqEdit_->text().split(",", Qt::SkipEmptyParts);
    for (const auto& part : parts) {
        const std::string v = trimStd(part.toStdString());
        if (!v.empty()) {
            c.prereq_ids.push_back(v);
        }
    }
    return c;
}

void CourseEditDialog::onSaveClicked() {
    bool ok = false;
    const int credit = creditEdit_->text().trimmed().toInt(&ok);

    if (idEdit_->text().trimmed().isEmpty()) {
        msgLabel_->setText("课程ID不能为空");
        return;
    }
    if (nameEdit_->text().trimmed().isEmpty()) {
        msgLabel_->setText("课程名不能为空");
        return;
    }
    if (!ok || credit <= 0) {
        msgLabel_->setText("学分必须为正整数");
        return;
    }

    const QString id = idEdit_->text().trimmed();
    const QString prereqRaw = prereqEdit_->text().trimmed();
    const QStringList prereqParts = prereqRaw.split(",", Qt::SkipEmptyParts);

    for (const auto& part : prereqParts) {
        const QString pre = part.trimmed();
        if (pre.isEmpty()) {
            continue;
        }
        if (pre == id) {
            msgLabel_->setText("课程不能把自己设为先修课");
            return;
        }
    }

    if (session_) {
        std::unordered_set<std::string> existingIds;
        for (const auto& c : session_->courseCache) {
            existingIds.insert(c.id);
            if (!editMode_ && QString::fromStdString(c.id) == id) {
                msgLabel_->setText("课程ID已存在于缓存中");
                return;
            }
        }
        existingIds.insert(id.toStdString());

        for (const auto& part : prereqParts) {
            const std::string pre = trimStd(part.toStdString());
            if (pre.empty()) {
                continue;
            }
            if (existingIds.count(pre) == 0) {
                msgLabel_->setText(QString("先修课程 %1 不在当前缓存中").arg(QString::fromStdString(pre)));
                return;
            }
        }
    }

    accept();
}
