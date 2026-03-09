#pragma once
#include <QDialog>
#include "../include/db_odbc.h"

class QTableWidget;
class QLabel;
struct AppSession;

class StudentCoursePage : public QDialog {
    Q_OBJECT
public:
    explicit StudentCoursePage(AppSession* session,
                               const StudentInfo& student,
                               QWidget* parent = nullptr);

private:
    void loadCourses();

    AppSession* session_;
    StudentInfo student_;
    QLabel* infoLabel_;
    QLabel* statusLabel_;
    QTableWidget* table_;
};