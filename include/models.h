#ifndef MODELS_H  // 头文件保护宏，防止同一个头文件被多次包含导致重复定义错误
#define MODELS_H

#include <string>
#include <vector>

// 枚举类：排课策略
enum class ScheduleStrategy {
    BALANCED = 1,
    FRONT_LOADED = 2
};

//课程结构体，描述一门课程的基本信息
struct Course{
    std::string id; //课程唯一标识符，例如"C01" 为唯一标识符
    std::string name; //课程名称，例如"数据结构"
    int credit = 0; //课程学分，大于0
    std::vector<std::string> prereq_ids; //先修课程ID列表，数组来存储 学习本课前必须完成的先修课程
};

//排课计划配置结构体 ， 用于控制排课算法的参数
struct PlanConfig{
    int max_semesters = 8; //允许的最大学期数，默认最大为8
    int max_credits_per_semester = 20; //每学期允许的最大学分数 ，默认20
    ScheduleStrategy strategy = ScheduleStrategy::BALANCED; //默认排课策略为平衡策略
};

struct SemesterPlan {
    std::vector<std::string> course_ids; //该学期的课程ID列表
    int total_credits = 0; //该学期的总学分数
};

#endif // MODELS_H //头文件保护结束