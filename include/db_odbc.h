#ifndef DB_ODBC_H
#define DB_ODBC_H

#include <string>
#include <vector>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <sqlext.h>

#include "models.h"

struct LearnedCourse {
    std::string course_id;
    std::string course_name;
    int semester = 0;
    std::string status;
    bool has_score = false;
    double score = 0.0;
};

struct StudentInfo {
    std::string student_id;
    std::string name;
    std::string major;
    int grade = 0;
};

struct StudentCoursePlanRow {
    std::string course_id;
    int semester = 0;
    std::string status = "PLANNED";
};

struct CourseSyncStats {
    int inserted = 0;
    int updated_or_unchanged = 0;
};

struct PlanWriteStats {
    int inserted = 0;
    int duplicated = 0;
};

class OdbcDb {
public:
    OdbcDb() = default;
    ~OdbcDb();

    bool connect(const std::string& conn_str, std::string& err);
    void disconnect();

    bool ping(std::string& err);
    bool getStudentCount(int& count, std::string& err);
    bool listStudentCourses(const std::string& student_id, std::vector<LearnedCourse>& out, std::string& err);
    bool listStudents(std::vector<StudentInfo>& out, std::string& err);
    bool getStudentById(const std::string& student_id, StudentInfo& out, bool& found, std::string& err);
    bool addStudent(const StudentInfo& s, std::string& err);
    bool updateStudent(const StudentInfo& s, std::string& err);
    bool deleteStudent(const std::string& student_id, std::string& err);

    // Legacy wrapper kept for older test files.
    bool insertStudentPlanRows(const std::string& student_id,
                               const std::vector<StudentCoursePlanRow>& rows,
                               int& affected,
                               std::string& err);

    bool listCoursesFromDb(std::vector<Course>& out, std::string& err);
    bool upsertCourses(const std::vector<Course>& rows, CourseSyncStats& stats, std::string& err);
    bool deleteStudentPlannedRows(const std::string& student_id, int& deleted, std::string& err);
    bool insertStudentPlanRowsDedup(const std::string& student_id,
                                    const std::vector<StudentCoursePlanRow>& rows,
                                    PlanWriteStats& stats,
                                    std::string& err);

private:
    SQLHENV env_ = SQL_NULL_HENV;
    SQLHDBC dbc_ = SQL_NULL_HDBC;
};

#endif
