#include "../include/scheduler.h"

#include<queue>
#include<unordered_set>
#include<algorithm>

namespace {

/**
 *  从课程向量构建一个以课程 ID 为键的映射表。
 * 
 * 该映射便于通过课程 ID 快速访问 Course 对象（如获取学分、名称等），
 * 在排课算法中频繁使用，避免每次线性查找。
 */
std::unordered_map<std::string, Course> buildCourseMap(const std::vector<Course>& courses) {
    std::unordered_map<std::string, Course> m;//[输出参数] 课程映射表，键为课程 ID，值为 Course 对象。
    for (const auto& c : courses) {
        m[c.id] = c;
    }
    return m;
}


/**
 * 根据当前可选的课程列表、剩余学分和学期数，为当前学期选择合适的课程。
 * 
 * 该函数实现了排课策略的核心选择逻辑：
 *   - FRONT_LOADED（前置加载）：优先选择学分高的课程，尽可能把高学分课程往前排。
 *   - BALANCED（均衡）：根据剩余学分和学期数计算一个目标学分（剩余学分/剩余学期数，向上取整），
 *     优先选择学分较小的课程，使学期总学分尽量接近目标，同时不超过学期上限。
 */
std::vector<std::string> pickCoursesForSemester(
    const std::vector<std::string>& available,//[输入参数] 当前可选的课程列表（未被选过的课程）。
    const std::unordered_map<std::string, Course>& course_map,//[输入参数] 课程映射表，键为课程 ID，值为 Course 对象。
    const PlanConfig& config,//[输入参数] 排课配置，包括最大允许学期数、每学期最大学分、排课策略。
    int remaining_credits,//[输入参数] 当前学期剩余可学习的学分。
    int remaining_semesters//[输入参数] 当前学期剩余可学习的学期数。
) {
    std::vector<std::string> ids = available; 
    std::vector<std::string> selected;//存储选择的课程
    int current_credits = 0;// 当前学期已选总学分

    // 便捷 lambda：根据课程 ID 获取学分（假设 ID 一定存在于 course_map 中）
    auto getCredit = [&](const std::string& id) {
        return course_map.at(id).credit;
    };
    // 前置加载策略：优先选高学分课程
    if (config.strategy == ScheduleStrategy::FRONT_LOADED) {
        // 按学分降序排序，学分相同则按 ID 字典序升序（保证稳定输出）
        std::sort(ids.begin(), ids.end(), [&](const std::string& a, const std::string& b) {
            if (getCredit(a) != getCredit(b)) {
                return getCredit(a) > getCredit(b);// 学分高的在前
            }
            return a < b;// 学分相同则 ID 小的在前
        });

        // 贪心选择：只要当前课程加入后不超过学期上限，就选中
        for (const auto& id : ids) {
            int credit = getCredit(id);
            if (current_credits + credit <= config.max_credits_per_semester) {
                selected.push_back(id);
                current_credits += credit;
            }
        }
        return selected;
    }

    // 均衡策略：计算目标学分（剩余学分 / 剩余学期数，向上取整）
    int target = (remaining_credits + remaining_semesters - 1) / remaining_semesters;
    if (target > config.max_credits_per_semester) {
        target = config.max_credits_per_semester; // 目标不能超过学期上限
    }
    // 按学分升序排序（学分小的优先），便于凑近目标
    std::sort(ids.begin(), ids.end(), [&](const std::string& a, const std::string& b) {
        if (getCredit(a) != getCredit(b)) {
            return getCredit(a) < getCredit(b);// 学分小的在前
        }
        return a < b;// 学分相同则 ID 小的在前
    });
    // 尝试选择课程，使当前学期总学分尽量接近 target，但不超过 target 和上限
    for (const auto& id : ids) {
        int credit = getCredit(id);
        // 条件：加入后不超过目标，且不超过学期上限
        if (current_credits + credit <= target && current_credits + credit <= config.max_credits_per_semester) {
            selected.push_back(id);
            current_credits += credit;
        }
    }
    // 如果上述规则没有选中任何课程（例如所有课程学分都大于 target），
    // 则退而求其次，选择第一门不超过学期上限的课程（保证至少能选一门）
    if (selected.empty()) {
        for (const auto& id : ids) {
            int credit = getCredit(id);
            if (credit <= config.max_credits_per_semester) {
                selected.push_back(id);
                break;
            }
        }
    }

    return selected;
}
}//namespace

bool buildPrereqGraph(
    const std::vector<Course>& courses,
    std::unordered_map<std::string, std::vector<std::string>>& adj, //[输出参数] 邻接表，存储课程的先修关系。
    std::unordered_map<std::string, int>& indegree, //[输出参数] 入度表，记录每个课程的先修课程数量。
    std::string& err
){
    // 1. 清空输出参数，确保不残留之前的数据
    adj.clear();
    indegree.clear();

    // 2. 用集合存储所有课程 ID，方便快速查找先修课程是否存在
    std::unordered_set<std::string> id_set;
    for(const auto& c : courses){
        id_set.insert(c.id);// 插入课程 ID
        adj[c.id] = {}; // 初始化邻接表条目为空向量
        indegree[c.id] = 0;// 初始化入度为 0
    }
    // 3. 遍历所有课程，构建依赖关系
    for(const auto& c : courses){// 遍历当前课程的所有先修课程
        for(const auto& pre : c.prereq_ids){
            // 3.1 检查先修课程是否存在于课程集合中
            if(id_set.count(pre) == 0){
                err = "先修课程未定义：" + pre + "(课程" + c.id + ")";
                return false;
            }
            // 3.2 添加边：先修课程 pre → 当前课程 c.id
            adj[pre].push_back(c.id);
            // 3.3 增加当前课程 c 的入度
            indegree[c.id]++;
        }
    }

    return true;
}

bool topologicalSort(
    const std::vector<Course>& courses,// 输入：所有课程列表
    std::vector<std::string>& order,// 输出：排序后的课程 ID 列表
    std::string& err// 输出：错误信息
){
    order.clear();// 清空输出容器，确保不残留之前的数据

    // 1. 构建依赖图
    std::unordered_map<std::string,std::vector<std::string>> adj;// 邻接表
    std::unordered_map<std::string,int>indegree;// 入度表
    if(!buildPrereqGraph(courses,adj,indegree,err)){
        return false;// 图构建失败（如先修课程未定义），直接返回
    }
    // 2. 初始化队列，将所有入度为 0 的课程入队
    std::queue<std::string> q;// 使用队列进行广度优先遍历
    for(const auto& c: courses){
        if(indegree[c.id] == 0){
            q.push(c.id);
        }
    }
    // 3. Kahn 算法：不断取出入度为 0 的节点
    while(!q.empty()){
        std::string u = q.front();// 取出队首课程
        q.pop();
        order.push_back(u);// 将其加入拓扑序列

        // 遍历 u 的所有后继课程
        for(const auto& v : adj[u]){
            indegree[v]--;// 减少后继课程的入度
            if(indegree[v] == 0){// 如果入度变为 0，则入队
                q.push(v);
            }
        }
    }
    // 4. 检查是否所有课程都被处理了
    if(order.size() != courses.size()){
        err = "存在先修环，无法进行拓扑排序";
        return false;
    }
    return true;
}
/**
 * 生成按学期的排课计划（核心排课算法）。
 * 该函数基于课程的先修依赖关系、学分以及用户配置（最大学期数、每学期学分上限、排课策略），
 * 使用贪心拓扑排序的方式，逐学期选择可学课程，生成一个满足所有约束的学期计划列表。
 * @param courses  所有课程的列表（已通过合法性检查，如先修课程存在、学分有效等）。
 * @param config   排课配置（最大学期数、每学期最大学分、策略）。
 * @param plan     [输出参数] 生成的学期计划列表，每个 SemesterPlan 包含该学期课程 ID 列表和总学分。
 * @param err      [输出参数] 如果排课失败，返回详细的错误信息。
 * @return         成功生成计划返回 true，失败返回 false。
 */
bool generateSemesterPlan(
    const std::vector<Course>& courses,
    const PlanConfig& config,
    std::vector<SemesterPlan>& plan,
    std::string& err
) {
    // 1. 清空输出计划，并从基础合法性检查开始
    plan.clear();
    if (courses.empty()) {
        err = "课程目录为空";
        return false;
    }
    if (config.max_semesters <= 0 || config.max_credits_per_semester <= 0) {
        err = "学期上限和学分上限必须为正数";
        return false;
    }

    // 2. 检查每门课程学分是否超过单学期上限，并计算总学分
    int total_credits = 0;
    for (const auto& c : courses) {
        if (c.credit > config.max_credits_per_semester) {
            err = "课程 " + c.id + " 学分超过单学期上限";
            return false;
        }
        total_credits += c.credit;
    }
    // 总学分容量检查：总学分不能超过最大学期数 × 每学期学分上限
    if (total_credits > config.max_semesters * config.max_credits_per_semester) {
        err = "总学分超过给定学期上限与单学期学分上限的容量";
        return false;
    }

    // 3. 构建先修依赖图（邻接表 adj 和入度 indegree），并建立课程 ID 到 Course 对象的映射
    std::unordered_map<std::string, std::vector<std::string>> adj;
    std::unordered_map<std::string, int> indegree;
    if (!buildPrereqGraph(courses, adj, indegree, err)) {
        return false;// 图构建失败（如先修课程未定义），错误信息已由 buildPrereqGraph 设置
    }
    const auto course_map = buildCourseMap(courses);// 快速获取课程信息

    // 4. 初始化数据结构
    std::unordered_set<std::string> scheduled;// 已安排的课程 ID 集合
    std::vector<std::string> available;// 当前可选的课程（入度为 0 且尚未安排
    for (const auto& c : courses) {
        if (indegree[c.id] == 0) {
            available.push_back(c.id);
        }
    }

    int semester_used = 0;// 已使用的学期数
    int remaining_credits = total_credits;// 剩余总学分
    // 5. 主循环：当还有课程未安排时继续
    while (scheduled.size() < courses.size()) {
        // 5.1 学期数超限检查
        if (semester_used >= config.max_semesters) {
            err = "超过最大学期数，仍有课程无法安排";
            return false;
        }
        // 5.2 如果当前没有可用课程，说明存在环或依赖无法满足
        if (available.empty()) {
            err = "存在先修环或不可达依赖，无法继续排课";
            return false;
        }
        // 5.3 根据策略、剩余学分和剩余学期数，从 available 中挑选本学期的课程
        int remaining_semesters = config.max_semesters - semester_used;
        std::vector<std::string> picked = pickCoursesForSemester(
            available, course_map, config, remaining_credits, remaining_semesters
        );
        if (picked.empty()) {
            err = "当前学期无法选择任何课程，请检查学分上限设置";
            return false;
        }
        // 5.4 将选中的课程构建为集合，便于快速查找
        std::unordered_set<std::string> picked_set(picked.begin(), picked.end());
        // 5.5 创建本学期计划对象，记录课程和总学分
        SemesterPlan sem;
        for (const auto& id : picked) {
            sem.course_ids.push_back(id);
            sem.total_credits += course_map.at(id).credit;
            scheduled.insert(id);
            remaining_credits -= course_map.at(id).credit;
        }
        plan.push_back(sem);
        // 5.6 准备下一轮的候选课程列表
        // 首先将当前 available 中未被选中的课程保留
        std::vector<std::string> next_available;
        for (const auto& id : available) {
            if (picked_set.count(id) == 0) {
                next_available.push_back(id);
            }
        }
        // 遍历本学期选中的课程，处理它们的后继课程
        for (const auto& id : picked) {
            for (const auto& v : adj[id]) {
                indegree[v]--;// 减少后继课程的入度
                // 如果入度变为 0 且该后继课程尚未被安排，则加入下一轮候选
                if (indegree[v] == 0 && scheduled.count(v) == 0) {
                    next_available.push_back(v);
                }
            }
        }
        // 5.7 对 next_available 进行去重（防止同一课程被多次添加）
        std::unordered_set<std::string> dedup;
        std::vector<std::string> compact;
        for (const auto& id : next_available) {
            if (dedup.insert(id).second) {
                compact.push_back(id);
            }
        }
        available.swap(compact);// 用去重后的紧凑列表替换 available
        // 5.8 学期计数增加
        semester_used++;
    }
    // 6. 所有课程安排完毕，返回成功
    return true;
}
