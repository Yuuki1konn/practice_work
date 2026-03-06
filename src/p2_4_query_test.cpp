#include<iostream>
#include<string>
#include<limits>
#include "../include/db_odbc.h"

static int finish(int code){
    std::cout << "按回车退出...";
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    std::string _;
    std::getline(std::cin, _);
    return code;
}

int main(){
    std::string uid,pwd,sid;
    std::cout <<"MySQL user: ";
    std::getline(std::cin,uid);
    std::cout <<"MySQL password: ";
    std::getline(std::cin,pwd);
    std::cout <<"Student ID: ";
    std::getline(std::cin, sid);
    
    std::string conn = 
        "DRIVER={MySQL ODBC 9.6 Unicode Driver};"
        "SERVER=127.0.0.1;"
        "PORT=3306;"
        "DATABASE=university_scheduler;"
        "UID=" + uid + ";"
        "PWD=" + pwd + ";"
        "OPTION=3";

    OdbcDb db;
    std::string err;

    if(!db.connect(conn,err)){
        std::cout << "connect failed: " << err << std::endl;
        std::cout << "提示: 如果错误含 IM002，说明系统未安装 MySQL ODBC 驱动，"
                  << "请安装 MySQL Connector/ODBC 8.x (64-bit)。\n";
        return finish(1);
    }

    if(!db.ping(err)){
        std::cout << "ping failed: " << err << std::endl;
        return finish(1);
    }
    std::vector<LearnedCourse> rows;
    if(!db.listStudentCourses(sid,rows,err)){
        std::cout << "Query failed: " << err << std::endl;
        return finish(1);
    }

    if(rows.empty()){
        std::cout << "No courses found for student: " << sid << std::endl;
        return finish(0);
    }

    std::cout << "semester\tcourse_id\tcourse_name\tstatus\tscore\n";
    for(const auto& r : rows){
        std::cout << r.semester << "\t"
                  << r.course_id << "\t"
                  << r.course_name << "\t"
                  << r.status << "\t";
        if(r.has_score){
            std::cout << r.score;
        }
        else std::cout << "NULL";
        std::cout << "\n";
    }
    return finish(0);
}