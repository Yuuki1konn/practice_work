#ifndef DB_ODBC_H
#define DB_ODBC_H


#include <string>
#include <vector>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <sqlext.h> // ODBC 核心头文件，定义了 SQLHENV, SQLHDBC, SQL_NULL_HENV 等类型和常量

#include "models.h"

struct LearnedCourse{
    std::string course_id;
    std::string course_name;
    int semester = 0;
    std::string status;
    bool has_score = false;
    double score = 0.0;
};

struct StudentCoursePlanRow{
    std::string course_id;
    int semester = 0;
    std::string status = "PLANNED";
};

struct CourseSyncStats{
    int inserted = 0;
    int updated_or_unchanged = 0;
};

struct PlanWriteStats{
    int inserted = 0;
    int duplicated = 0;
};

class OdbcDb{
    public:
        OdbcDb() = default;
        ~OdbcDb();

        bool connect(const std::string& conn_str , std::string& err);
        void disconnect();

        bool ping(std::string& err); // SELECT 1
        bool getStudentCount(int& count, std::string& err);
        bool listStudentCourses(const std::string& student_id, std::vector<LearnedCourse>& out, std::string& err);

        bool insertStudentPlanRows(const std::string& student_id, const std::vector<StudentCoursePlanRow>& rows,int& affected, std::string& err);

        bool listCoursesFromDb(std::vector<Course>& out, std::string& err);
        bool upsertCourses(const std::vector<Course>& rows, CourseSyncStats& stats, std::string& err);
        bool deleteStudentPlannedRows(const std::string& student_id, int& deleted, std::string& err);
        bool insertStudentPlanRowsDedup(const std::string& student_id, const std::vector<StudentCoursePlanRow>& rows, PlanWriteStats& stats, std::string& err);


    private:
        SQLHENV env_ = SQL_NULL_HENV;///< ODBC 环境句柄，初始化为空
        SQLHDBC dbc_ = SQL_NULL_HDBC;///< ODBC 连接句柄，初始化为空
};

#endif// DB_ODBC_H
