#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <cctype>
#include <limits>
#include "../include/db_odbc.h"
//去除空白字符
static std::string trim(const std::string& s){
    size_t l = 0, r = s.size();
    while(l < r && std::isspace((unsigned char)s[l])) ++l;
    while(r > l && std::isspace((unsigned char)s[r-1])) --r;
    return s.substr(l, r - l);
}
// 判断字符串 s 是否以字符串 p 开头（前缀匹配
static bool startsWith(const std::string& s, const std::string& p){
    return s.rfind(p, 0) == 0;
}
// 从指定的计划文件（文本文件）中加载学生课程计划行。
//文件格式预期为：
//- 以“学期 X”开头的行定义当前学期号
//- 随后是若干行课程信息，每行格式：课程ID|课程名称|学分（名称和学分会被忽略，只取ID）
//- 忽略空行、表头行（如“课程安排”、“总学期数”、“ID|课程名|学分”、“总学分”）
static bool loadPlanRows(const std::string& path, std::vector<StudentCoursePlanRow>& rows, std::string& err){
    rows.clear();// 清空输出容器，确保不残留之前的数据
    std::ifstream fin(path);// 打开文件
    if(!fin.is_open()){
        err = "无法打开文件：" + path;
        return false;// 文件打开失败，返回错误
    }

    std::string line;
    int current_semester = 0;
    int line_no = 0;

    while(std::getline(fin, line)){
        ++line_no;// 增加行号
        std::string t = trim(line);
        if(t.empty()) continue;// 跳过空行
        
        if(startsWith(t, "学期")){
            std::istringstream iss(t.substr(std::string("学期").size()));
            int s = 0;
            if(!(iss >> s) || s <= 0){
                err = "第" + std::to_string(line_no) + "行：学期格式错误" + t;
                return false;// 学期格式错误，返回错误
            }
            current_semester = s;// 更新当前学期
            continue;// 继续下一行
        }

        if(t =="课程安排" || startsWith(t, "总学期数") || t== "ID|课程名|学分" || startsWith(t, "总学分")){
            continue;// 跳过这一行
        }

        size_t p1 = t.find('|');
        size_t p2 = t.find('|', p1 == std::string::npos ? 0 : p1 + 1 );
        if(p1 == std::string::npos || p2 == std::string::npos) continue;// 跳过这一行

        std::string cid = trim(t.substr(0, p1));
        if(cid.empty()|| cid == "ID") continue;// 跳过这一行
        if(current_semester <= 0){
            err = "course row appear before semester header, line" + std::to_string(line_no);
            return false;
        }
        StudentCoursePlanRow r;
        r.course_id = cid;
        r.semester = current_semester;
        r.status = "PLANNED";
        rows.push_back(r);// 加入输出容器
    }
    if(rows.empty()){
        err = "no course row found in plan file";
        return false;
    }
    return true;

}
static int finish(int code){
    std::cout<<"按回车退出...";
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    std::string _;
    std::getline(std::cin, _);
    return code;
}

int main(){
    std::string uid,pwd,sid,plan_path;
    std::cout<<"MySQL user: ";
    std::getline(std::cin, uid);
    std::cout<<"MySQL password: ";
    std::getline(std::cin, pwd);
    std::cout<<"Student ID: ";
    std::getline(std::cin, sid);
    std::cout<<"Plan path(默认 output/plan.txt): ";
    std::getline(std::cin, plan_path);
    if(plan_path.empty()) plan_path = "output/plan.txt";

    std::vector<StudentCoursePlanRow> rows;
    std::string err;
    if(!loadPlanRows(plan_path, rows, err)){
        std::cout<<"加载计划文件失败："<<err<<std::endl;
        return finish(1);
    }
    std::string conn=
        "Driver={MySQL ODBC 9.6 Unicode Driver};"
        "Server=127.0.0.1;"
        "Port=3306;"
        "Database=university_scheduler;"
        "Uid=" + uid + ";"
        "Pwd=" + pwd + ";"
        "Option=3";
    
    OdbcDb db;
    if(!db.connect(conn, err)){
        std::cout<<"数据库连接失败："<<err<<std::endl;
        return finish(1);
    }

    int affected = 0;
    if(!db.insertStudentPlanRows(sid, rows, affected, err)){
        std::cout<<"插入计划失败："<<err<<std::endl;
        std::cout<<"提示：若外键错误，请先保证 course 表里已有这些 course_id.\n";
        return finish(1);
    }
    std::cout << "计划行: " << rows.size() << "，新插入: " << affected
          << "，重复跳过: " << (rows.size() - affected) << "\n";

    std::vector<LearnedCourse> out;
    if(!db.listStudentCourses(sid, out, err)){
        std::cout<<"查询计划失败："<<err<<std::endl;
        return finish(1);
    }

    std::cout<<"semester\tcourse_id\tcourse_name\tstatus\tscore\n";
    for(const auto& r : out){
        std::cout<<r.semester<<"\t"<<r.course_id<<"\t"<<r.course_name<<"\t"<<r.status<<"\t";
        if(r.has_score) std::cout<< r.score;
        else std::cout<<"NULL";
        std::cout<<"\n";
    }

    return finish(0);
}