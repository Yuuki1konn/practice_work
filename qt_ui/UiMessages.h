#pragma once

#include <QMessageBox>
#include <QString>
#include <QWidget>

namespace UiMessages {

inline void info(QWidget* parent, const QString& title, const QString& text) {
    QMessageBox::information(parent, title, text);
}

inline void warning(QWidget* parent, const QString& title, const QString& text) {
    QMessageBox::warning(parent, title, text);
}

inline void error(QWidget* parent, const QString& title, const QString& text) {
    QMessageBox::critical(parent, title, text);
}

inline bool confirm(QWidget* parent,
                    const QString& title,
                    const QString& text,
                    QMessageBox::StandardButton default_button = QMessageBox::No) {
    const auto reply = QMessageBox::question(
        parent,
        title,
        text,
        QMessageBox::Yes | QMessageBox::No,
        default_button
    );
    return reply == QMessageBox::Yes;
}

inline bool confirmUpload(QWidget* parent, const QString& target_name, int row_count) {
    return confirm(
        parent,
        "确认上传",
        QString("是否上传到数据库？\n将同步 %1 条%2记录。").arg(row_count).arg(target_name),
        QMessageBox::No
    );
}

}  // namespace UiMessages
