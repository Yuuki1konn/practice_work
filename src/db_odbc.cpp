#include "../include/db_odbc.h"

#include <cstring>
#include <sstream>
#include <unordered_set>

static std::string getOdbcError(SQLSMALLINT handle_type, SQLHANDLE handle) {
    std::ostringstream oss;
    SQLCHAR state[6] = {0};
    SQLCHAR msg[512] = {0};
    SQLINTEGER native_error = 0;
    SQLSMALLINT msg_len = 0;

    SQLSMALLINT i = 1;
    while (true) {
        SQLRETURN ret = SQLGetDiagRecA(
            handle_type, handle, i, state, &native_error, msg, sizeof(msg), &msg_len
        );
        if (ret == SQL_NO_DATA || !SQL_SUCCEEDED(ret)) {
            break;
        }
        oss << "[" << state << "]" << msg << " ";
        ++i;
    }
    return oss.str();
}

OdbcDb::~OdbcDb() {
    disconnect();
}

bool OdbcDb::connect(const std::string& conn_str, std::string& err) {
    disconnect();

    if (!SQL_SUCCEEDED(SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &env_))) {
        err = "SQLAllocHandle(SQL_HANDLE_ENV) failed.";
        return false;
    }
    if (!SQL_SUCCEEDED(SQLSetEnvAttr(env_, SQL_ATTR_ODBC_VERSION, (void*)SQL_OV_ODBC3, 0))) {
        err = "SQLSetEnvAttr ODBC3 failed: " + getOdbcError(SQL_HANDLE_ENV, env_);
        disconnect();
        return false;
    }
    if (!SQL_SUCCEEDED(SQLAllocHandle(SQL_HANDLE_DBC, env_, &dbc_))) {
        err = "SQLAllocHandle(SQL_HANDLE_DBC) failed: " + getOdbcError(SQL_HANDLE_ENV, env_);
        disconnect();
        return false;
    }

    SQLCHAR out_conn[1024] = {0};
    SQLSMALLINT out_len = 0;
    SQLRETURN ret = SQLDriverConnectA(
        dbc_,
        nullptr,
        (SQLCHAR*)conn_str.c_str(),
        SQL_NTS,
        out_conn,
        sizeof(out_conn),
        &out_len,
        SQL_DRIVER_NOPROMPT
    );
    if (!SQL_SUCCEEDED(ret)) {
        err = "SQLDriverConnect failed: " + getOdbcError(SQL_HANDLE_DBC, dbc_);
        disconnect();
        return false;
    }
    return true;
}

void OdbcDb::disconnect() {
    if (dbc_ != SQL_NULL_HDBC) {
        SQLDisconnect(dbc_);
        SQLFreeHandle(SQL_HANDLE_DBC, dbc_);
        dbc_ = SQL_NULL_HDBC;
    }
    if (env_ != SQL_NULL_HENV) {
        SQLFreeHandle(SQL_HANDLE_ENV, env_);
        env_ = SQL_NULL_HENV;
    }
}

bool OdbcDb::ping(std::string& err) {
    if (dbc_ == SQL_NULL_HDBC) {
        err = "Not connected.";
        return false;
    }
    SQLHSTMT stmt = SQL_NULL_HSTMT;
    if (!SQL_SUCCEEDED(SQLAllocHandle(SQL_HANDLE_STMT, dbc_, &stmt))) {
        err = "SQLAllocHandle(SQL_HANDLE_STMT) failed.";
        return false;
    }
    SQLRETURN ret = SQLExecDirectA(stmt, (SQLCHAR*)"SELECT 1", SQL_NTS);
    if (!SQL_SUCCEEDED(ret)) {
        err = "SELECT 1 failed: " + getOdbcError(SQL_HANDLE_STMT, stmt);
        SQLFreeHandle(SQL_HANDLE_STMT, stmt);
        return false;
    }
    SQLFreeHandle(SQL_HANDLE_STMT, stmt);
    return true;
}

bool OdbcDb::getStudentCount(int& count, std::string& err) {
    count = 0;
    if (dbc_ == SQL_NULL_HDBC) {
        err = "Not connected.";
        return false;
    }

    SQLHSTMT stmt = SQL_NULL_HSTMT;
    if (!SQL_SUCCEEDED(SQLAllocHandle(SQL_HANDLE_STMT, dbc_, &stmt))) {
        err = "SQLAllocHandle(SQL_HANDLE_STMT) failed.";
        return false;
    }

    SQLRETURN ret = SQLExecDirectA(stmt, (SQLCHAR*)"SELECT COUNT(*) FROM student", SQL_NTS);
    if (!SQL_SUCCEEDED(ret)) {
        err = "COUNT query failed: " + getOdbcError(SQL_HANDLE_STMT, stmt);
        SQLFreeHandle(SQL_HANDLE_STMT, stmt);
        return false;
    }

    ret = SQLFetch(stmt);
    if (!SQL_SUCCEEDED(ret)) {
        err = "SQLFetch failed: " + getOdbcError(SQL_HANDLE_STMT, stmt);
        SQLFreeHandle(SQL_HANDLE_STMT, stmt);
        return false;
    }

    SQLLEN ind = 0;
    SQLINTEGER v = 0;
    ret = SQLGetData(stmt, 1, SQL_C_SLONG, &v, sizeof(v), &ind);
    if (!SQL_SUCCEEDED(ret) || ind == SQL_NULL_DATA) {
        err = "SQLGetData failed: " + getOdbcError(SQL_HANDLE_STMT, stmt);
        SQLFreeHandle(SQL_HANDLE_STMT, stmt);
        return false;
    }

    count = (int)v;
    SQLFreeHandle(SQL_HANDLE_STMT, stmt);
    return true;
}

bool OdbcDb::listStudentCourses(const std::string& student_id, std::vector<LearnedCourse>& out, std::string& err) {
    out.clear();
    if (dbc_ == SQL_NULL_HDBC) {
        err = "Not connected.";
        return false;
    }

    SQLHSTMT stmt = SQL_NULL_HSTMT;
    if (!SQL_SUCCEEDED(SQLAllocHandle(SQL_HANDLE_STMT, dbc_, &stmt))) {
        err = "SQLAllocHandle(SQL_HANDLE_STMT) failed.";
        return false;
    }

    const char* sql =
        "SELECT sc.course_id, c.course_name, sc.semester, sc.status, sc.score "
        "FROM student_course sc "
        "LEFT JOIN course c ON c.course_id = sc.course_id "
        "WHERE sc.student_id = ? "
        "ORDER BY sc.semester, sc.course_id";

    if (!SQL_SUCCEEDED(SQLPrepareA(stmt, (SQLCHAR*)sql, SQL_NTS))) {
        err = "SQLPrepare failed: " + getOdbcError(SQL_HANDLE_STMT, stmt);
        SQLFreeHandle(SQL_HANDLE_STMT, stmt);
        return false;
    }

    SQLLEN sid_ind = SQL_NTS;
    if (!SQL_SUCCEEDED(SQLBindParameter(stmt, 1, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR,
                                        (SQLULEN)student_id.size(), 0,
                                        (SQLPOINTER)student_id.c_str(), 0, &sid_ind))) {
        err = "SQLBindParameter failed: " + getOdbcError(SQL_HANDLE_STMT, stmt);
        SQLFreeHandle(SQL_HANDLE_STMT, stmt);
        return false;
    }

    if (!SQL_SUCCEEDED(SQLExecute(stmt))) {
        err = "SQLExecute failed: " + getOdbcError(SQL_HANDLE_STMT, stmt);
        SQLFreeHandle(SQL_HANDLE_STMT, stmt);
        return false;
    }

    while (true) {
        SQLRETURN ret = SQLFetch(stmt);
        if (ret == SQL_NO_DATA) {
            break;
        }
        if (!SQL_SUCCEEDED(ret)) {
            err = "SQLFetch failed: " + getOdbcError(SQL_HANDLE_STMT, stmt);
            SQLFreeHandle(SQL_HANDLE_STMT, stmt);
            return false;
        }

        LearnedCourse row;
        char course_id[64] = {0};
        char course_name[128] = {0};
        SQLINTEGER semester = 0;
        char status[32] = {0};
        double score = 0.0;
        SQLLEN ind1 = 0, ind2 = 0, ind3 = 0, ind4 = 0, ind5 = 0;

        if (!SQL_SUCCEEDED(SQLGetData(stmt, 1, SQL_C_CHAR, course_id, sizeof(course_id), &ind1)) ||
            !SQL_SUCCEEDED(SQLGetData(stmt, 2, SQL_C_CHAR, course_name, sizeof(course_name), &ind2)) ||
            !SQL_SUCCEEDED(SQLGetData(stmt, 3, SQL_C_SLONG, &semester, sizeof(semester), &ind3)) ||
            !SQL_SUCCEEDED(SQLGetData(stmt, 4, SQL_C_CHAR, status, sizeof(status), &ind4)) ||
            !SQL_SUCCEEDED(SQLGetData(stmt, 5, SQL_C_DOUBLE, &score, sizeof(score), &ind5))) {
            err = "SQLGetData failed: " + getOdbcError(SQL_HANDLE_STMT, stmt);
            SQLFreeHandle(SQL_HANDLE_STMT, stmt);
            return false;
        }

        row.course_id = (ind1 == SQL_NULL_DATA) ? "" : course_id;
        row.course_name = (ind2 == SQL_NULL_DATA) ? "" : course_name;
        row.semester = (ind3 == SQL_NULL_DATA) ? 0 : (int)semester;
        row.status = (ind4 == SQL_NULL_DATA) ? "" : status;
        row.has_score = (ind5 != SQL_NULL_DATA);
        if (row.has_score) {
            row.score = score;
        }
        out.push_back(row);
    }

    SQLFreeHandle(SQL_HANDLE_STMT, stmt);
    return true;
}

bool OdbcDb::listCoursesFromDb(std::vector<Course>& out, std::string& err) {
    out.clear();
    if (dbc_ == SQL_NULL_HDBC) {
        err = "Not connected.";
        return false;
    }

    SQLHSTMT stmt = SQL_NULL_HSTMT;
    if (!SQL_SUCCEEDED(SQLAllocHandle(SQL_HANDLE_STMT, dbc_, &stmt))) {
        err = "SQLAllocHandle(SQL_HANDLE_STMT) failed.";
        return false;
    }

    const char* sql = "SELECT course_id, course_name, credit FROM course ORDER BY course_id";
    if (!SQL_SUCCEEDED(SQLExecDirectA(stmt, (SQLCHAR*)sql, SQL_NTS))) {
        err = "listCoursesFromDb query failed: " + getOdbcError(SQL_HANDLE_STMT, stmt);
        SQLFreeHandle(SQL_HANDLE_STMT, stmt);
        return false;
    }

    while (true) {
        SQLRETURN ret = SQLFetch(stmt);
        if (ret == SQL_NO_DATA) {
            break;
        }
        if (!SQL_SUCCEEDED(ret)) {
            err = "SQLFetch failed: " + getOdbcError(SQL_HANDLE_STMT, stmt);
            SQLFreeHandle(SQL_HANDLE_STMT, stmt);
            return false;
        }

        char cid[64] = {0};
        char cname[128] = {0};
        SQLINTEGER credit = 0;
        SQLLEN ind1 = 0, ind2 = 0, ind3 = 0;

        if (!SQL_SUCCEEDED(SQLGetData(stmt, 1, SQL_C_CHAR, cid, sizeof(cid), &ind1)) ||
            !SQL_SUCCEEDED(SQLGetData(stmt, 2, SQL_C_CHAR, cname, sizeof(cname), &ind2)) ||
            !SQL_SUCCEEDED(SQLGetData(stmt, 3, SQL_C_SLONG, &credit, sizeof(credit), &ind3))) {
            err = "SQLGetData failed: " + getOdbcError(SQL_HANDLE_STMT, stmt);
            SQLFreeHandle(SQL_HANDLE_STMT, stmt);
            return false;
        }

        if (ind1 == SQL_NULL_DATA || ind2 == SQL_NULL_DATA || ind3 == SQL_NULL_DATA) {
            continue;
        }

        Course c;
        c.id = cid;
        c.name = cname;
        c.credit = (int)credit;
        c.prereq_ids.clear();
        out.push_back(c);
    }

    SQLFreeHandle(SQL_HANDLE_STMT, stmt);

    // Ensure prereq table exists, then load prereq relations.
    SQLHSTMT ensure_stmt = SQL_NULL_HSTMT;
    if (!SQL_SUCCEEDED(SQLAllocHandle(SQL_HANDLE_STMT, dbc_, &ensure_stmt))) {
        err = "SQLAllocHandle(SQL_HANDLE_STMT) failed.";
        return false;
    }
    const char* ensure_sql =
        "CREATE TABLE IF NOT EXISTS course_prereq ("
        "course_id VARCHAR(20) NOT NULL,"
        "prereq_id VARCHAR(20) NOT NULL,"
        "PRIMARY KEY (course_id, prereq_id),"
        "CONSTRAINT fk_cp_course FOREIGN KEY (course_id) REFERENCES course(course_id) "
        "ON UPDATE CASCADE ON DELETE CASCADE,"
        "CONSTRAINT fk_cp_prereq FOREIGN KEY (prereq_id) REFERENCES course(course_id) "
        "ON UPDATE CASCADE ON DELETE RESTRICT"
        ")";
    if (!SQL_SUCCEEDED(SQLExecDirectA(ensure_stmt, (SQLCHAR*)ensure_sql, SQL_NTS))) {
        err = "ensure course_prereq table failed: " + getOdbcError(SQL_HANDLE_STMT, ensure_stmt);
        SQLFreeHandle(SQL_HANDLE_STMT, ensure_stmt);
        return false;
    }
    SQLFreeHandle(SQL_HANDLE_STMT, ensure_stmt);

    if (out.empty()) {
        return true;
    }

    SQLHSTMT pre_stmt = SQL_NULL_HSTMT;
    if (!SQL_SUCCEEDED(SQLAllocHandle(SQL_HANDLE_STMT, dbc_, &pre_stmt))) {
        err = "SQLAllocHandle(SQL_HANDLE_STMT) failed.";
        return false;
    }

    const char* pre_sql = "SELECT course_id, prereq_id FROM course_prereq ORDER BY course_id, prereq_id";
    if (!SQL_SUCCEEDED(SQLExecDirectA(pre_stmt, (SQLCHAR*)pre_sql, SQL_NTS))) {
        err = "list course_prereq failed: " + getOdbcError(SQL_HANDLE_STMT, pre_stmt);
        SQLFreeHandle(SQL_HANDLE_STMT, pre_stmt);
        return false;
    }

    while (true) {
        SQLRETURN ret = SQLFetch(pre_stmt);
        if (ret == SQL_NO_DATA) {
            break;
        }
        if (!SQL_SUCCEEDED(ret)) {
            err = "SQLFetch prereq failed: " + getOdbcError(SQL_HANDLE_STMT, pre_stmt);
            SQLFreeHandle(SQL_HANDLE_STMT, pre_stmt);
            return false;
        }

        char cid[64] = {0};
        char pid[64] = {0};
        SQLLEN ind1 = 0, ind2 = 0;
        if (!SQL_SUCCEEDED(SQLGetData(pre_stmt, 1, SQL_C_CHAR, cid, sizeof(cid), &ind1)) ||
            !SQL_SUCCEEDED(SQLGetData(pre_stmt, 2, SQL_C_CHAR, pid, sizeof(pid), &ind2))) {
            err = "SQLGetData prereq failed: " + getOdbcError(SQL_HANDLE_STMT, pre_stmt);
            SQLFreeHandle(SQL_HANDLE_STMT, pre_stmt);
            return false;
        }
        if (ind1 == SQL_NULL_DATA || ind2 == SQL_NULL_DATA) {
            continue;
        }

        std::string c_id = cid;
        std::string p_id = pid;
        for (auto& c : out) {
            if (c.id == c_id) {
                c.prereq_ids.push_back(p_id);
                break;
            }
        }
    }

    SQLFreeHandle(SQL_HANDLE_STMT, pre_stmt);
    return true;
}

bool OdbcDb::upsertCourses(const std::vector<Course>& rows, CourseSyncStats& stats, std::string& err) {
    stats.inserted = 0;
    stats.updated_or_unchanged = 0;

    if (dbc_ == SQL_NULL_HDBC) {
        err = "Not connected.";
        return false;
    }
    if (rows.empty()) {
        return true;
    }

    SQLHSTMT stmt = SQL_NULL_HSTMT;
    if (!SQL_SUCCEEDED(SQLAllocHandle(SQL_HANDLE_STMT, dbc_, &stmt))) {
        err = "SQLAllocHandle(SQL_HANDLE_STMT) failed.";
        return false;
    }

    const char* sql =
        "INSERT INTO course(course_id, course_name, credit) "
        "VALUES(?, ?, ?) "
        "ON DUPLICATE KEY UPDATE "
        "course_name = VALUES(course_name), "
        "credit = VALUES(credit)";

    if (!SQL_SUCCEEDED(SQLPrepareA(stmt, (SQLCHAR*)sql, SQL_NTS))) {
        err = "SQLPrepare failed: " + getOdbcError(SQL_HANDLE_STMT, stmt);
        SQLFreeHandle(SQL_HANDLE_STMT, stmt);
        return false;
    }

    char cid[64] = {0};
    char cname[128] = {0};
    SQLINTEGER credit = 0;
    SQLLEN cid_ind = SQL_NTS;
    SQLLEN cname_ind = SQL_NTS;
    SQLLEN credit_ind = 0;

    if (!SQL_SUCCEEDED(SQLBindParameter(stmt, 1, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR,
                                        sizeof(cid) - 1, 0, cid, sizeof(cid), &cid_ind)) ||
        !SQL_SUCCEEDED(SQLBindParameter(stmt, 2, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR,
                                        sizeof(cname) - 1, 0, cname, sizeof(cname), &cname_ind)) ||
        !SQL_SUCCEEDED(SQLBindParameter(stmt, 3, SQL_PARAM_INPUT, SQL_C_SLONG, SQL_INTEGER,
                                        0, 0, &credit, sizeof(credit), &credit_ind))) {
        err = "SQLBindParameter failed: " + getOdbcError(SQL_HANDLE_STMT, stmt);
        SQLFreeHandle(SQL_HANDLE_STMT, stmt);
        return false;
    }

    for (const auto& c : rows) {
        if (c.id.empty() || c.name.empty() || c.credit <= 0) {
            err = "Invalid course row for upsert.";
            SQLFreeHandle(SQL_HANDLE_STMT, stmt);
            return false;
        }

        std::memset(cid, 0, sizeof(cid));
        std::memset(cname, 0, sizeof(cname));
        std::strncpy(cid, c.id.c_str(), sizeof(cid) - 1);
        std::strncpy(cname, c.name.c_str(), sizeof(cname) - 1);
        credit = c.credit;

        SQLRETURN ret = SQLExecute(stmt);
        if (!SQL_SUCCEEDED(ret)) {
            err = "upsertCourses SQLExecute failed at course_id=" + c.id + ": " +
                  getOdbcError(SQL_HANDLE_STMT, stmt);
            SQLFreeHandle(SQL_HANDLE_STMT, stmt);
            return false;
        }

        SQLLEN rc = 0;
        if (SQL_SUCCEEDED(SQLRowCount(stmt, &rc))) {
            if (rc == 1) {
                stats.inserted++;
            } else {
                stats.updated_or_unchanged++;
            }
        } else {
            stats.updated_or_unchanged++;
        }
        SQLCloseCursor(stmt);
    }

    SQLFreeHandle(SQL_HANDLE_STMT, stmt);

    // Ensure prereq table exists.
    SQLHSTMT ensure_stmt = SQL_NULL_HSTMT;
    if (!SQL_SUCCEEDED(SQLAllocHandle(SQL_HANDLE_STMT, dbc_, &ensure_stmt))) {
        err = "SQLAllocHandle(SQL_HANDLE_STMT) failed.";
        return false;
    }
    const char* ensure_sql =
        "CREATE TABLE IF NOT EXISTS course_prereq ("
        "course_id VARCHAR(20) NOT NULL,"
        "prereq_id VARCHAR(20) NOT NULL,"
        "PRIMARY KEY (course_id, prereq_id),"
        "CONSTRAINT fk_cp_course FOREIGN KEY (course_id) REFERENCES course(course_id) "
        "ON UPDATE CASCADE ON DELETE CASCADE,"
        "CONSTRAINT fk_cp_prereq FOREIGN KEY (prereq_id) REFERENCES course(course_id) "
        "ON UPDATE CASCADE ON DELETE RESTRICT"
        ")";
    if (!SQL_SUCCEEDED(SQLExecDirectA(ensure_stmt, (SQLCHAR*)ensure_sql, SQL_NTS))) {
        err = "ensure course_prereq table failed: " + getOdbcError(SQL_HANDLE_STMT, ensure_stmt);
        SQLFreeHandle(SQL_HANDLE_STMT, ensure_stmt);
        return false;
    }
    SQLFreeHandle(SQL_HANDLE_STMT, ensure_stmt);

    // Replace prereq mappings for each course in cache.
    SQLHSTMT del_stmt = SQL_NULL_HSTMT;
    SQLHSTMT ins_stmt = SQL_NULL_HSTMT;
    if (!SQL_SUCCEEDED(SQLAllocHandle(SQL_HANDLE_STMT, dbc_, &del_stmt)) ||
        !SQL_SUCCEEDED(SQLAllocHandle(SQL_HANDLE_STMT, dbc_, &ins_stmt))) {
        err = "SQLAllocHandle(SQL_HANDLE_STMT) failed.";
        if (del_stmt != SQL_NULL_HSTMT) SQLFreeHandle(SQL_HANDLE_STMT, del_stmt);
        if (ins_stmt != SQL_NULL_HSTMT) SQLFreeHandle(SQL_HANDLE_STMT, ins_stmt);
        return false;
    }

    const char* del_sql = "DELETE FROM course_prereq WHERE course_id = ?";
    const char* ins_sql = "INSERT INTO course_prereq(course_id, prereq_id) VALUES(?, ?)";

    if (!SQL_SUCCEEDED(SQLPrepareA(del_stmt, (SQLCHAR*)del_sql, SQL_NTS))) {
        err = "prepare prereq delete SQL failed: " + getOdbcError(SQL_HANDLE_STMT, del_stmt);
        SQLFreeHandle(SQL_HANDLE_STMT, del_stmt);
        SQLFreeHandle(SQL_HANDLE_STMT, ins_stmt);
        return false;
    }
    if (!SQL_SUCCEEDED(SQLPrepareA(ins_stmt, (SQLCHAR*)ins_sql, SQL_NTS))) {
        err = "prepare prereq insert SQL failed: " + getOdbcError(SQL_HANDLE_STMT, ins_stmt);
        SQLFreeHandle(SQL_HANDLE_STMT, del_stmt);
        SQLFreeHandle(SQL_HANDLE_STMT, ins_stmt);
        return false;
    }

    char del_cid[64] = {0};
    SQLLEN del_cid_ind = SQL_NTS;
    if (!SQL_SUCCEEDED(SQLBindParameter(del_stmt, 1, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR,
                                        sizeof(del_cid) - 1, 0, del_cid, sizeof(del_cid), &del_cid_ind))) {
        err = "bind prereq delete parameter failed: " + getOdbcError(SQL_HANDLE_STMT, del_stmt);
        SQLFreeHandle(SQL_HANDLE_STMT, del_stmt);
        SQLFreeHandle(SQL_HANDLE_STMT, ins_stmt);
        return false;
    }

    char ins_cid[64] = {0};
    char ins_pid[64] = {0};
    SQLLEN ins_cid_ind = SQL_NTS;
    SQLLEN ins_pid_ind = SQL_NTS;
    if (!SQL_SUCCEEDED(SQLBindParameter(ins_stmt, 1, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR,
                                        sizeof(ins_cid) - 1, 0, ins_cid, sizeof(ins_cid), &ins_cid_ind)) ||
        !SQL_SUCCEEDED(SQLBindParameter(ins_stmt, 2, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR,
                                        sizeof(ins_pid) - 1, 0, ins_pid, sizeof(ins_pid), &ins_pid_ind))) {
        err = "bind prereq insert parameter failed: " + getOdbcError(SQL_HANDLE_STMT, ins_stmt);
        SQLFreeHandle(SQL_HANDLE_STMT, del_stmt);
        SQLFreeHandle(SQL_HANDLE_STMT, ins_stmt);
        return false;
    }

    for (const auto& c : rows) {
        std::memset(del_cid, 0, sizeof(del_cid));
        std::strncpy(del_cid, c.id.c_str(), sizeof(del_cid) - 1);
        if (!SQL_SUCCEEDED(SQLExecute(del_stmt))) {
            err = "delete old prereq failed for " + c.id + ": " + getOdbcError(SQL_HANDLE_STMT, del_stmt);
            SQLFreeHandle(SQL_HANDLE_STMT, del_stmt);
            SQLFreeHandle(SQL_HANDLE_STMT, ins_stmt);
            return false;
        }
        SQLCloseCursor(del_stmt);

        std::unordered_set<std::string> seen;
        for (const auto& pre : c.prereq_ids) {
            if (pre.empty() || !seen.insert(pre).second) {
                continue;
            }
            std::memset(ins_cid, 0, sizeof(ins_cid));
            std::memset(ins_pid, 0, sizeof(ins_pid));
            std::strncpy(ins_cid, c.id.c_str(), sizeof(ins_cid) - 1);
            std::strncpy(ins_pid, pre.c_str(), sizeof(ins_pid) - 1);
            if (!SQL_SUCCEEDED(SQLExecute(ins_stmt))) {
                err = "insert prereq failed (" + c.id + " <- " + pre + "): " +
                      getOdbcError(SQL_HANDLE_STMT, ins_stmt);
                SQLFreeHandle(SQL_HANDLE_STMT, del_stmt);
                SQLFreeHandle(SQL_HANDLE_STMT, ins_stmt);
                return false;
            }
            SQLCloseCursor(ins_stmt);
        }
    }

    SQLFreeHandle(SQL_HANDLE_STMT, del_stmt);
    SQLFreeHandle(SQL_HANDLE_STMT, ins_stmt);
    return true;
}

bool OdbcDb::deleteStudentPlannedRows(const std::string& student_id, int& deleted, std::string& err) {
    deleted = 0;
    if (dbc_ == SQL_NULL_HDBC) {
        err = "Not connected.";
        return false;
    }
    if (student_id.empty()) {
        err = "student_id is empty.";
        return false;
    }

    SQLHSTMT stmt = SQL_NULL_HSTMT;
    if (!SQL_SUCCEEDED(SQLAllocHandle(SQL_HANDLE_STMT, dbc_, &stmt))) {
        err = "SQLAllocHandle(SQL_HANDLE_STMT) failed.";
        return false;
    }

    const char* sql =
        "DELETE FROM student_course "
        "WHERE student_id = ? "
        "AND status IN ('PLANNED','ENROLLED')";

    if (!SQL_SUCCEEDED(SQLPrepareA(stmt, (SQLCHAR*)sql, SQL_NTS))) {
        err = "SQLPrepare failed: " + getOdbcError(SQL_HANDLE_STMT, stmt);
        SQLFreeHandle(SQL_HANDLE_STMT, stmt);
        return false;
    }

    SQLLEN sid_ind = SQL_NTS;
    if (!SQL_SUCCEEDED(SQLBindParameter(stmt, 1, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR,
                                        (SQLULEN)student_id.size(), 0,
                                        (SQLPOINTER)student_id.c_str(), 0, &sid_ind))) {
        err = "SQLBindParameter failed: " + getOdbcError(SQL_HANDLE_STMT, stmt);
        SQLFreeHandle(SQL_HANDLE_STMT, stmt);
        return false;
    }

    if (!SQL_SUCCEEDED(SQLExecute(stmt))) {
        err = "deleteStudentPlannedRows SQLExecute failed: " + getOdbcError(SQL_HANDLE_STMT, stmt);
        SQLFreeHandle(SQL_HANDLE_STMT, stmt);
        return false;
    }

    SQLLEN rc = 0;
    if (SQL_SUCCEEDED(SQLRowCount(stmt, &rc))) {
        deleted = (int)rc;
    }

    SQLFreeHandle(SQL_HANDLE_STMT, stmt);
    return true;
}

bool OdbcDb::insertStudentPlanRowsDedup(const std::string& student_id,
                                        const std::vector<StudentCoursePlanRow>& rows,
                                        PlanWriteStats& stats,
                                        std::string& err) {
    stats.inserted = 0;
    stats.duplicated = 0;

    if (dbc_ == SQL_NULL_HDBC) {
        err = "Not connected.";
        return false;
    }
    if (student_id.empty()) {
        err = "student_id is empty.";
        return false;
    }
    if (rows.empty()) {
        return true;
    }

    SQLHSTMT stmt = SQL_NULL_HSTMT;
    if (!SQL_SUCCEEDED(SQLAllocHandle(SQL_HANDLE_STMT, dbc_, &stmt))) {
        err = "SQLAllocHandle(SQL_HANDLE_STMT) failed.";
        return false;
    }

    const char* sql =
        "INSERT INTO student_course(student_id, course_id, semester, status) "
        "SELECT ?, ?, ?, ? "
        "FROM DUAL "
        "WHERE NOT EXISTS ("
        "  SELECT 1 FROM student_course "
        "  WHERE student_id = ? AND course_id = ?"
        ")";

    if (!SQL_SUCCEEDED(SQLPrepareA(stmt, (SQLCHAR*)sql, SQL_NTS))) {
        err = "SQLPrepare failed: " + getOdbcError(SQL_HANDLE_STMT, stmt);
        SQLFreeHandle(SQL_HANDLE_STMT, stmt);
        return false;
    }

    char sid[64] = {0};
    char cid[64] = {0};
    SQLINTEGER sem = 0;
    char status[32] = {0};

    SQLLEN sid_ind = SQL_NTS;
    SQLLEN cid_ind = SQL_NTS;
    SQLLEN sem_ind = 0;
    SQLLEN status_ind = SQL_NTS;

    SQLLEN sid2_ind = SQL_NTS;
    SQLLEN cid2_ind = SQL_NTS;

    if (!SQL_SUCCEEDED(SQLBindParameter(stmt, 1, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR,
                                        sizeof(sid) - 1, 0, sid, sizeof(sid), &sid_ind)) ||
        !SQL_SUCCEEDED(SQLBindParameter(stmt, 2, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR,
                                        sizeof(cid) - 1, 0, cid, sizeof(cid), &cid_ind)) ||
        !SQL_SUCCEEDED(SQLBindParameter(stmt, 3, SQL_PARAM_INPUT, SQL_C_SLONG, SQL_INTEGER,
                                        0, 0, &sem, sizeof(sem), &sem_ind)) ||
        !SQL_SUCCEEDED(SQLBindParameter(stmt, 4, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR,
                                        sizeof(status) - 1, 0, status, sizeof(status), &status_ind)) ||
        !SQL_SUCCEEDED(SQLBindParameter(stmt, 5, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR,
                                        sizeof(sid) - 1, 0, sid, sizeof(sid), &sid2_ind)) ||
        !SQL_SUCCEEDED(SQLBindParameter(stmt, 6, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR,
                                        sizeof(cid) - 1, 0, cid, sizeof(cid), &cid2_ind))) {
        err = "SQLBindParameter failed: " + getOdbcError(SQL_HANDLE_STMT, stmt);
        SQLFreeHandle(SQL_HANDLE_STMT, stmt);
        return false;
    }

    std::strncpy(sid, student_id.c_str(), sizeof(sid) - 1);

    for (const auto& r : rows) {
        if (r.course_id.empty() || r.semester <= 0) {
            err = "Invalid row: empty course_id or semester <= 0";
            SQLFreeHandle(SQL_HANDLE_STMT, stmt);
            return false;
        }

        std::memset(cid, 0, sizeof(cid));
        std::memset(status, 0, sizeof(status));
        std::strncpy(cid, r.course_id.c_str(), sizeof(cid) - 1);
        sem = r.semester;
        std::string st = r.status.empty() ? "PLANNED" : r.status;
        std::strncpy(status, st.c_str(), sizeof(status) - 1);

        SQLRETURN ret = SQLExecute(stmt);
        if (!SQL_SUCCEEDED(ret)) {
            err = "insertStudentPlanRowsDedup SQLExecute failed at (" + r.course_id +
                  ", sem " + std::to_string(r.semester) + "): " +
                  getOdbcError(SQL_HANDLE_STMT, stmt);
            SQLFreeHandle(SQL_HANDLE_STMT, stmt);
            return false;
        }

        SQLLEN rc = 0;
        if (SQL_SUCCEEDED(SQLRowCount(stmt, &rc)) && rc > 0) {
            stats.inserted++;
        } else {
            stats.duplicated++;
        }
        SQLCloseCursor(stmt);
    }

    SQLFreeHandle(SQL_HANDLE_STMT, stmt);
    return true;
}

bool OdbcDb::insertStudentPlanRows(const std::string& student_id,
                                   const std::vector<StudentCoursePlanRow>& rows,
                                   int& affected,
                                   std::string& err) {
    PlanWriteStats stats;
    if (!insertStudentPlanRowsDedup(student_id, rows, stats, err)) {
        return false;
    }
    affected = stats.inserted;
    return true;
}
