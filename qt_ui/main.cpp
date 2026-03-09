#include <QApplication>
#include "AppSession.h"
#include "LoginDialog.h"
#include "MainWindow.h"

int main(int argc, char *argv[]){
    //创建应用程序对象，并传递命令行参数。这一步初始化 Qt 内部资源，是任何 Qt GUI 程序必须做的第一件事。
    QApplication app(argc, argv);
    //实例化一个 AppSession 对象，用于保存登录状态（用户名、密码、数据库连接等）。
    AppSession session;
    LoginDialog login(&session);
    if(login.exec() != QDialog::Accepted){
        return 0;
    }
    //创建并显示主窗口
    MainWindow w(&session);//创建 MainWindow 对象，同样传入 session 指针，使主窗口能够访问数据库连接和用户信息。
    w.show();
    // 进入事件循环
    return app.exec();
}