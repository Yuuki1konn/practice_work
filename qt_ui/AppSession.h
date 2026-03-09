#pragma once
#include <string>
#include <vector>
#include "../include/models.h"
#include "../include/db_odbc.h"

struct AppSession{
    std::string uid;
    std::string pwd;
    OdbcDb db;
    bool connected = false;
    
    std::string buildConnStr() const{
        return "DRIVER={MySQL ODBC 9.6 Unicode Driver};"
               "SERVER=127.0.0.1;"
               "PORT=3306;"
               "DATABASE=university_scheduler;"
               "UID=" + uid + ";"
               "PWD=" + pwd + ";"
               "OPTION=3;";
    }

    bool connect(std::string& err){
        connected = db.connect(buildConnStr(), err);
        return connected;
    }

    std::vector<Course> courseCache;
    bool courseCacheLoaded = false;
    PlanConfig planConfig;
    std::vector<SemesterPlan> generatedPlan;
    std::string selectedStudentId;
};
