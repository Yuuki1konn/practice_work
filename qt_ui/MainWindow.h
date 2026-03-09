#pragma once
#include <QMainWindow>

class QLabel;
struct AppSession;

class MainWindow : public QMainWindow{
    Q_OBJECT//Q_OBJECT 宏：必须出现在使用 Qt 信号与槽机制的类中，启用元对象系统特性（如信号、槽、属性等）。
public:
    explicit MainWindow(AppSession* session, QWidget* parent = nullptr);

private:
    AppSession* session_;
    QLabel* infoLabel_;
};