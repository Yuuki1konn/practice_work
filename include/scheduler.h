#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <string>
#include <unordered_map>// 引入 std::unordered_map，用于构建图和存储入度
#include <vector>// 引入 std::vector，用于存储课程列表和拓扑排序结果

#include "models.h"

//勾图 先修课-> 后继课
bool buildPrereqGraph(
    const std::vector<Course>& courses,
    //邻接表，表示从先修课程到后继课程的映射（即先修课 -> 依赖它的课程列表）。
    std::unordered_map<std::string, std::vector<std::string>>& adj,
    //入度表，表示每个课程的先修课程数量（即依赖它的先修课程数量）。
    std::unordered_map<std::string, int>& indegree,
    std::string& err
);

//拓扑排序(Kahn)
bool topologicalSort(
    const std::vector<Course>& courses,
    //[输出参数] 存储拓扑排序后的课程 ID 列表。
    std::vector<std::string>& order,
    std::string& err
);

// 生成按学期的排课计划
bool generateSemesterPlan(
    const std::vector<Course>& courses,
    const PlanConfig& config, //排课配置，包括最大允许学期数、每学期最大学分、排课策略。
    std::vector<SemesterPlan>& plan, //[输出参数] 存储生成的按学期的排课计划。
    std::string& err
);


#endif
