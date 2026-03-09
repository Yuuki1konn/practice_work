#pragma once
#include <QDialog>

class QLineEdit;
class QLabel;
class QPushButton;
struct AppSession;

class LoginDialog : public QDialog {
    Q_OBJECT
public:
    explicit LoginDialog(AppSession* session, QWidget* parent = nullptr);

private slots:
    void onConnectClicked();

private:
    AppSession* session_;
    QLineEdit* userEdit_;
    QLineEdit* passEdit_;
    QLabel* msgLabel_;
    QPushButton* connectBtn_;
};

