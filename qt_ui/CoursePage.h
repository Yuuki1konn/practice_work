#pragma once
#include <QDialog>

class QTableWidget;
class QLabel;
struct AppSession;

class CoursePage : public QDialog {
    Q_OBJECT
public:
    explicit CoursePage(AppSession* session, QWidget* parent = nullptr);

private slots:
    void reloadFromCache();
    void loadFromDbToCache();
    void loadFromTxtToCache();
    void onAddCourse();
    void onEditCourse();
    void onDeleteCourse();
    void onDeleteCourseFromDb();
    void onSyncToDb();

private:
    bool confirmReplaceCache(const QString& sourceName);
    bool confirmUploadToDb();

    AppSession* session_;
    QTableWidget* table_;
    QLabel* statusLabel_;
};
