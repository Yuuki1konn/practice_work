#pragma once

#include <QDialog>

class QLabel;
class QSpinBox;
class QComboBox;
class QTableWidget;
class QCheckBox;
struct AppSession;

class PlanPage : public QDialog {
    Q_OBJECT
public:
    explicit PlanPage(AppSession* session, QWidget* parent = nullptr);

private slots:
    void reloadSummary();
    void reloadStudents();
    void onStudentChanged(int index);
    void onSaveConfig();
    void onGeneratePlan();
    void onExportPlan();

private:
    void loadStudentHistory(const std::string& student_id);
    void reloadGeneratedPlanView();

    AppSession* session_;
    QSpinBox* maxSemesterSpin_;
    QSpinBox* maxCreditSpin_;
    QComboBox* strategyCombo_;
    QComboBox* studentCombo_;
    QCheckBox* removeCompletedCheck_;
    QLabel* cacheSummaryLabel_;
    QLabel* configSummaryLabel_;
    QLabel* studentInfoLabel_;
    QTableWidget* historyTable_;
    QTableWidget* planTable_;
    QLabel* planSummaryLabel_;
    QLabel* statusLabel_;
};
