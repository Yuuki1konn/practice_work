#pragma once
#include <QDialog>

class QTableWidget;
class QLabel;
struct AppSession;

class StudentPage : public QDialog{
    Q_OBJECT
public:
    explicit StudentPage(AppSession* session, QWidget* parent = nullptr);

private slots:
    void reloadStudents();
    void onAddStudent();
    void onEditStudent();
    void onDeleteStudent();
    void onViewCourses();

private:
    AppSession* session_;
    QTableWidget* table_;
    QLabel* statusLabel_;
};