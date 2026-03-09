#pragma once
#include <QDialog>
#include "../include/models.h"

class QLineEdit;
class QLabel;
class QPushButton;
struct AppSession;

class CourseEditDialog : public QDialog {
    Q_OBJECT
public:
    explicit CourseEditDialog(AppSession* session, QWidget* parent = nullptr);

    void setCourse(const Course& c);
    Course course() const;

private slots:
    void onSaveClicked();

private:
    AppSession* session_;
    bool editMode_ = false;

    QLineEdit* idEdit_;
    QLineEdit* nameEdit_;
    QLineEdit* creditEdit_;
    QLineEdit* prereqEdit_;
    QLabel* msgLabel_;
    QPushButton* saveBtn_;
};
