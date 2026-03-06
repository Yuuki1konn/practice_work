#include "../include/db_odbc.h"

#include <sstream>
#include <cstring>
//辅助函数：获取 ODBC 诊断信息（错误详情）。
static std::string getOdbcError(SQLSMALLINT handle_type, SQLHANDLE handle){
    std::ostringstream oss;
    SQLCHAR state[6] = {0};// SQLSTATE 长度为 5，加上结尾 '\0'
    SQLCHAR msg[512] = {0};// 错误消息缓冲区
    SQLINTEGER native_error = 0;// 数据库原生错误码
    SQLSMALLINT msg_len = 0;// 实际消息长度

    SQLSMALLINT i = 1;// 诊断记录号，从 1 开始
    while(true){
        SQLRETURN ret = SQLGetDiagRecA(
            handle_type, handle, i , state, &native_error, msg , sizeof(msg), &msg_len
        );
        if(ret == SQL_NO_DATA) break;// 无更多诊断记录
        if(!SQL_SUCCEEDED(ret)) break; // 获取失败，退出循环
        oss << "[" << state << "]" << msg <<" ";// 拼接状态码和消息
        ++i;
    }
    return oss.str();
}

OdbcDb::~OdbcDb(){
    disconnect();
}
// * 建立数据库连接。
bool OdbcDb::connect(const std::string& conn_str, std::string& err){
    disconnect(); // 确保从干净状态开始
    // 1. 分配环境句柄
    if(!SQL_SUCCEEDED(SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &env_)))
    {
        err = "SQLAllocHandle(SQL_HANDLE_ENV) ENV failed.";
        return false;
    }
    // 2. 设置环境属性为 ODBC 3.x 版本（必须）
    if(!SQL_SUCCEEDED(SQLSetEnvAttr(env_, SQL_ATTR_ODBC_VERSION,(void*)SQL_OV_ODBC3, 0))){
        err = "SQLSetEnvAttr ODBC3 failed:" + getOdbcError(SQL_HANDLE_ENV, env_);
        disconnect();
        return false;
    }
    // 3. 分配连接句柄
    if(!SQL_SUCCEEDED(SQLAllocHandle(SQL_HANDLE_DBC,env_,&dbc_))){
        err = "SQLAllocHandle DBC failed:" + getOdbcError(SQL_HANDLE_ENV, env_);
        disconnect();
        return false;
    }
    // 4. 使用连接字符串建立连接
    SQLCHAR out_conn[1024] = {0}; // 输出的完整连接字符串（可能包含扩充信息）
    SQLSMALLINT out_len = 0;// 输出连接字符串长度

    SQLRETURN ret = SQLDriverConnectA(
        dbc_, nullptr, // 连接句柄// 窗口句柄（无窗口）
        (SQLCHAR*)conn_str.c_str(), SQL_NTS,// 输入连接字符串// 字符串以 null 结尾
        out_conn,sizeof(out_conn),&out_len,// 输出缓冲区及其大小// 输出实际长度
        SQL_DRIVER_NOPROMPT// 无提示，直接连接
    );
    
    if(!SQL_SUCCEEDED(ret)){
        err = "SQLDriverConnect failed:" + getOdbcError(SQL_HANDLE_DBC, dbc_);
        disconnect();
        return false;
    }

    return true;
}
//断开连接并释放所有 ODBC 句柄。
void OdbcDb::disconnect(){
    if(dbc_ != SQL_NULL_HDBC){
        SQLDisconnect(dbc_);// 断开连接
        SQLFreeHandle(SQL_HANDLE_DBC, dbc_);// 释放连接句柄
        dbc_ = SQL_NULL_HDBC;
    }
    if(env_ != SQL_NULL_HENV){
        SQLFreeHandle(SQL_HANDLE_ENV, env_);// 释放环境句柄
        env_ = SQL_NULL_HENV;
    }
}
//发送 "SELECT 1" 测试连接是否存活。
bool OdbcDb::ping(std::string& err){
    if(dbc_ == SQL_NULL_HDBC){
        err = "Not connected.";
        return false;
    }
    
    SQLHSTMT stmt = SQL_NULL_HSTMT;
    // 分配语句句柄
    if(!SQL_SUCCEEDED(SQLAllocHandle(SQL_HANDLE_STMT, dbc_, &stmt))){
        err = "SQLAllocHandle STMT failed.";
        return false;
    }
    // 直接执行 "SELECT 1"
    SQLRETURN ret = SQLExecDirectA(
        stmt, (SQLCHAR*)"SELECT 1", SQL_NTS
    );
    if(!SQL_SUCCEEDED(ret)){
        err = "SELECT 1 failed:" + getOdbcError(SQL_HANDLE_STMT, stmt);
        SQLFreeHandle(SQL_HANDLE_STMT, stmt);
        return false;
    }
    // 释放语句句柄
    SQLFreeHandle(SQL_HANDLE_STMT, stmt);
    return true;
}
//获取学生表中的记录总数（假设表名为 student）。
bool OdbcDb::getStudentCount(int& count, std::string& err){
    count = 0;
    if(dbc_ == SQL_NULL_HDBC){
        err = "Not connected.";
        return false;
    }

    SQLHSTMT stmt = SQL_NULL_HSTMT;
    if(!SQL_SUCCEEDED(SQLAllocHandle(SQL_HANDLE_STMT, dbc_, &stmt))){
        err = "SQLAllocHandle STMT failed.";
        return false;
    }
    // 执行查询
    SQLRETURN ret = SQLExecDirectA(stmt,(SQLCHAR*)"SELECT COUNT(*) FROM student", SQL_NTS);
    if(!SQL_SUCCEEDED(ret)){
        err = "COUNT query failed:" + getOdbcError(SQL_HANDLE_STMT, stmt);
        SQLFreeHandle(SQL_HANDLE_STMT, stmt);
        return false;
    }
    // 获取结果集第一行
    ret = SQLFetch(stmt);
    if(!SQL_SUCCEEDED(ret)){
        err = "SQLFetch failed:" + getOdbcError(SQL_HANDLE_STMT, stmt);
        SQLFreeHandle(SQL_HANDLE_STMT, stmt);
        return false;
    }
    // 获取第一列的值（COUNT(*) 的结果）
    SQLLEN ind = 0;// 指示器，可用于检测 NULL
    SQLINTEGER v = 0;
    ret = SQLGetData(stmt, 1 , SQL_C_SLONG,&v,sizeof(v),&ind);
    if(!SQL_SUCCEEDED(ret)|| ind == SQL_NULL_DATA){// 如果出错或值为 NULL（理论上 COUNT 不会 NULL）
        err = "SQLGetData failed:" + getOdbcError(SQL_HANDLE_STMT, stmt);
        SQLFreeHandle(SQL_HANDLE_STMT, stmt);
        return false;
    } 

    count = (int)v;// 转换为 int 输出
    SQLFreeHandle(SQL_HANDLE_STMT, stmt);
    return true;
}
bool OdbcDb::listStudentCourses(const std::string& student_id, std::vector<LearnedCourse>& out, std::string& err){
    //清理输出向量并检查连接
    out.clear();
    if(dbc_ == SQL_NULL_HDBC){
        err = "Not connected.";
        return false;
    }
    //分配语句句柄
    SQLHSTMT stmt = SQL_NULL_HSTMT;
    if(!SQL_SUCCEEDED(SQLAllocHandle(SQL_HANDLE_STMT, dbc_, &stmt))){
        err = "SQLAllocHandle STMT failed.";
        return false;
    }
    //准备 SQL 语句（使用预处理）
    const char* sql=
        "SELECT sc.course_id, c.course_name, sc.semester, sc.status, sc.score "
        "FROM student_course sc "
        "LEFT JOIN course c ON c.course_id = sc.course_id "
        "WHERE sc.student_id = ? "
        "ORDER BY sc.semester, sc.course_id";
    //SQLPrepareA 将 SQL 字符串发送到数据库进行解析和编译。
    //SQL_NTS 表示字符串以 null 结尾（Null-Terminated String）。
    if(!SQL_SUCCEEDED(SQLPrepareA(stmt,(SQLCHAR*)sql,SQL_NTS))){
        //如果准备失败，调用 getOdbcError 获取详细错误信息，释放语句句柄，返回 false。
        err = "SQLPrepare failed: " + getOdbcError(SQL_HANDLE_STMT, stmt);
        SQLFreeHandle(SQL_HANDLE_STMT, stmt);
        return false;
    }

    // 绑定参数
    //sid_ind 初始化为 SQL_NTS，表示输入字符串以 null 结尾，驱动会自动计算长度
    SQLLEN sid_ind = SQL_NTS;
    if(!SQL_SUCCEEDED(SQLBindParameter(stmt,1,SQL_PARAM_INPUT,SQL_C_CHAR,SQL_VARCHAR,
                                        (SQLULEN)student_id.size(),0,(SQLPOINTER)student_id.c_str(),0,&sid_ind))){
        err = "SQLBindParameter failed: " + getOdbcError(SQL_HANDLE_STMT,stmt);
        SQLFreeHandle(SQL_HANDLE_STMT, stmt);
        return false;
    }
    // 执行语句
    //SQLExecute 执行预处理语句，此时数据库将参数值代入并执行查询。
    if(!SQL_SUCCEEDED(SQLExecute(stmt))){
        err = "SQLExecute failed: " + getOdbcError(SQL_HANDLE_STMT, stmt);
        SQLFreeHandle(SQL_HANDLE_STMT, stmt);
        return false;
    }
    //循环获取结果行
    while(true){
        SQLRETURN ret = SQLFetch(stmt);
        if(ret == SQL_NO_DATA) break;
        if(!SQL_SUCCEEDED(ret)){
            err = "SQLFetch failed: " + getOdbcError(SQL_HANDLE_STMT, stmt);
            SQLFreeHandle(SQL_HANDLE_STMT, stmt);
            return false;
        }

        LearnedCourse row;// 定义临时对象存储当前行数据

        // 定义缓冲区
        char course_id[64] = {0};
        char course_name[128] = {0};
        SQLINTEGER semester = 0;
        char status[32] = {0};
        double score = 0.0;
        // 指示器变量：用于检测 NULL 或实际长度
        SQLLEN ind1 = 0, ind2 = 0, ind3 = 0, ind4 = 0, ind5 = 0;
        // 依次获取各列数据
        if(!SQL_SUCCEEDED(SQLGetData(stmt, 1, SQL_C_CHAR, course_id, sizeof(course_id), &ind1)) ||
           !SQL_SUCCEEDED(SQLGetData(stmt, 2, SQL_C_CHAR, course_name, sizeof(course_name), &ind2)) ||
           !SQL_SUCCEEDED(SQLGetData(stmt, 3, SQL_C_SLONG, &semester, sizeof(semester), &ind3)) ||
           !SQL_SUCCEEDED(SQLGetData(stmt, 4, SQL_C_CHAR, status, sizeof(status), &ind4)) ||
           !SQL_SUCCEEDED(SQLGetData(stmt, 5, SQL_C_DOUBLE, &score, sizeof(score), &ind5)))
        {
            err = "SQLGetData failed: " + getOdbcError(SQL_HANDLE_STMT, stmt);
            SQLFreeHandle(SQL_HANDLE_STMT, stmt);
            return false;
        }
        // 根据指示器值判断是否为 NULL，并填充 row
        row.course_id = (ind1 == SQL_NULL_DATA) ? "" : course_id;
        row.course_name = (ind2 == SQL_NULL_DATA) ? "" : course_name;
        row.semester = (ind3 == SQL_NULL_DATA) ? 0 : (int)semester;
        row.status = (ind4 == SQL_NULL_DATA) ? "" : status;
        row.has_score = (ind5 != SQL_NULL_DATA);
        if(row.has_score){
            row.score = score;
        }
        out.push_back(row);// 将当前行加入结果集
    }
    SQLFreeHandle(SQL_HANDLE_STMT, stmt);
    return true;
}

//函数目的：批量插入或更新学生的课程计划记录。
bool OdbcDb::insertStudentPlanRows(const std::string& student_id,const std::vector<StudentCoursePlanRow>& rows,int& affected, std::string& err){
    //受影响行数
    affected = 0;
    //检查连接
    if(dbc_ == SQL_NULL_HDBC){
        err = "Not connected.";
        return false;
    }
    if(student_id.empty()){
        err = "student_id is empty.";
        return false;
    }
    if(rows.empty()) return true;
    //分配语句句柄，用于执行 SQL。
    SQLHSTMT stmt = SQL_NULL_HSTMT;
    if (!SQL_SUCCEEDED(SQLAllocHandle(SQL_HANDLE_STMT, dbc_, &stmt))) {
        err = "SQLAllocHandle STMT failed.";
        return false;
    }
    //准备 SQL 语句
    const char* sql =
        "INSERT IGNORE INTO student_course(student_id,course_id,semester,status) "
        "VALUES(?, ?, ?, ?)";
    //SQLPrepareA 将 SQL 字符串发送到数据库进行解析和编译。
    //SQL_NTS 表示字符串以 null 结尾（Null-Terminated String）。
    if(!SQL_SUCCEEDED(SQLPrepareA(stmt,(SQLCHAR*)sql,SQL_NTS))){
        err = "SQLPrepare failed: " + getOdbcError(SQL_HANDLE_STMT, stmt);
        SQLFreeHandle(SQL_HANDLE_STMT, stmt);
        return false;
    }

    char sid[64] = {0};// 存储学生ID
    char cid[64] = {0};// 存储课程ID
    SQLINTEGER sem = 0;// 存储学期
    char status[32] = {0};// 存储状态

    SQLLEN sid_ind = SQL_NTS;
    SQLLEN cid_ind = SQL_NTS;
    SQLLEN sem_ind = 0;
    SQLLEN status_ind = SQL_NTS;

    if(!SQL_SUCCEEDED(SQLBindParameter(stmt, 1, SQL_PARAM_INPUT,SQL_C_CHAR,SQL_VARCHAR,sizeof(sid) - 1,0,sid,sizeof(sid),&sid_ind))||
       !SQL_SUCCEEDED(SQLBindParameter(stmt, 2, SQL_PARAM_INPUT,SQL_C_CHAR,SQL_VARCHAR,sizeof(cid) - 1,0,cid,sizeof(cid),&cid_ind))||
       !SQL_SUCCEEDED(SQLBindParameter(stmt, 3, SQL_PARAM_INPUT,SQL_C_SLONG,SQL_INTEGER,0,0,&sem,sizeof(sem),&sem_ind))||
       !SQL_SUCCEEDED(SQLBindParameter(stmt, 4, SQL_PARAM_INPUT,SQL_C_CHAR,SQL_VARCHAR,sizeof(status) - 1,0,status,sizeof(status),&status_ind)))
    {
        err = "SQLBindParameter failed: " + getOdbcError(SQL_HANDLE_STMT, stmt);
        SQLFreeHandle(SQL_HANDLE_STMT, stmt);
        return false;
    }

    //复制固定部分（学生ID）到缓冲区
    std::strncpy(sid, student_id.c_str(), sizeof(sid) - 1);

    //循环处理每一行
    for(const auto& r : rows){
         // 检查单行数据有效性
        if(r.course_id.empty() || r.semester <= 0) {
            err = "Invalid row: empty course_id or semester <= 0";
            SQLFreeHandle(SQL_HANDLE_STMT, stmt);
            return false;
        }
        // 清空并重新设置可变缓冲区
        std::memset(cid, 0 , sizeof(cid));
        std::memset(status, 0, sizeof(status));

        std::strncpy(cid, r.course_id.c_str(), sizeof(cid) - 1);
        sem = r.semester;

        std::string st = r.status.empty() ? "PLANNED" : r.status;
        std::strncpy(status, st.c_str(), sizeof(status) - 1);
        // 执行语句
        SQLRETURN ret = SQLExecute(stmt);
        if(!SQL_SUCCEEDED(ret)){
            err = "SQLExecute failed at (" + r.course_id + ", sem " + std::to_string(r.semester) + ")" +getOdbcError(SQL_HANDLE_STMT, stmt);
            SQLFreeHandle(SQL_HANDLE_STMT, stmt);
            return false;
        }
        // 获取受影响行数
        SQLLEN rc = 0;
        if(SQL_SUCCEEDED(SQLRowCount(stmt, &rc))) {
            affected += (int)rc;
        }
         // 关闭游标，以便下次执行
        SQLCloseCursor(stmt);
    }

    SQLFreeHandle(SQL_HANDLE_STMT, stmt);
    return true;
}