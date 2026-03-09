#pragma once
#include <QDialog>
#include "../include/db_odbc.h"

class QLineEdit;
class QLabel;
class QPushButton;

class StudentEditDialog : public QDialog
{
    Q_OBJECT
public:
    explicit StudentEditDialog(QWidget* parent = nullptr);

    void setStudent(const StudentInfo& s);
    StudentInfo student() const;

private slots:
    void onSaveClicked();

private:
    QLineEdit* idEdit_;
    QLineEdit* nameEdit_;
    QLineEdit* majorEdit_;
    QLineEdit* gradeEdit_;
    QLabel* msgLabel_;
    QPushButton* saveBtn_;
};
