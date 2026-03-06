#include <iostream>
#include <limits>
#include <vector>
#include <string>
#include <unordered_map>
#include <sstream>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <unordered_set>

#include "../include/models.h" //引入自定义的数据模型头文件，包含 Course, PlanConfig, SemesterPlan 
#include "../include/course_io.h" //引入课程 IO 相关函数头文件，包含导入、导出课程目录的函数
#include "../include/course_edit.h" //引入课程编辑相关函数头文件，包含添加、删除、修改课程的函数
#include "../include/scheduler.h" //引入排课相关函数头文件，包含生成排课计划的函数
#include "../include/db_odbc.h" //引入数据库 ODBC 相关函数头文件，包含数据库连接、查询、更新等操作

std::vector<Course> courses; // 全局变量，存储导入的课程目录
std::unordered_map<std::string, Course> course_map; // 全局变量，存储课程 ID 到课程对象的映射
std::vector<SemesterPlan> plan; // 全局变量，存储生成的排课计划


/**
 * 打印主菜单包装
 */
void printMenu(){
    std::cout << "\n=== 排课系统主菜单(TUI) ===\n";
    std::cout << "1. 导入课程目录\n";
    std::cout << "2. 编辑课程目录\n";
    std::cout << "3. 生成排课计划\n";
    std::cout << "4. 导出排课计划\n";
    std::cout << "5. 管理学生\n";
    std::cout << "0. 退出程序\n";
    std::cout << "输入序号: ";
}

void rebuildCourseMap(const std::vector<Course>&courses, std::unordered_map<std::string, Course>& course_map){
    course_map.clear(); // 清空旧映射
    for (const auto& c : courses) {
        course_map[c.id] = c; // 用课程 ID 作为键，课程对象作为值
    }
}


std::string trim(const std::string& input) {
    size_t start = 0;
    // 从头扫描空白字符
    while (start < input.size() && std::isspace(static_cast<unsigned char>(input[start]))) {
        ++start;
    }

    size_t end = input.size();
    // 从尾扫描空白字符
    while (end > start && std::isspace(static_cast<unsigned char>(input[end - 1]))) {
        --end;
    }
    // 返回子串 [start, end)
    return input.substr(start, end - start);
}

std::string joinPrereq(const std::vector<std::string>& prereq_ids) {
    if (prereq_ids.empty()) {
        return "(无)";
    }
    std::ostringstream oss;
    for (size_t i = 0; i < prereq_ids.size(); ++i) {
        oss << prereq_ids[i];
        if (i + 1 < prereq_ids.size()) {
            oss << ",";
        }
    }
    return oss.str();
}

int findCourseIndexById(const std::vector<Course>& list, const std::string& id) {
    for (size_t i = 0; i < list.size(); ++i) {
        if (list[i].id == id) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

void waitForEnter() {
    std::cout << "按回车继续...";
    // 仅在缓冲区已有换行残留时吞掉，避免阻塞并导致需要按两次回车
    if (std::cin.rdbuf()->in_avail() > 0 && std::cin.peek() == '\n') {
        std::cin.get();
    }
    std::string temp;
    std::getline(std::cin, temp);
}

void clearScreen() {
#ifdef _WIN32
    std::system("cls");
#else
    std::system("clear");
#endif
}
 /** 列出 data 目录下所有扩展名为 .txt 或 .TXT 的文件的完整路径。
 * 
 * 该函数使用 C++17 的 std::filesystem 库遍历 "./data" 目录。
 * 如果 data 目录不存在或不是一个目录，则返回空列表。
 * 
 * @return 包含所有匹配文件完整路径的字符串向量。
 */
std::vector<std::string> listTxtFilesInDataDir() {
    std::vector<std::string> files;
    const std::filesystem::path data_dir("data");// data 目录的相对路径
    if (!std::filesystem::exists(data_dir) || !std::filesystem::is_directory(data_dir)) {
        return files;// 目录不存在或不是目录，返回空
    }
    // 遍历目录
    for (const auto& entry : std::filesystem::directory_iterator(data_dir)) {
        if (!entry.is_regular_file()) {
            continue;// 只处理普通文件，忽略子目录、链接等
        }
        const auto ext = entry.path().extension().string();// 获取扩展名，如 ".txt"
        if (ext == ".txt" || ext == ".TXT") {
            files.push_back(entry.path().string());// 保存完整路径
        }
    }
    return files;
}
/**
 * 检查字符串是否表示一个正整数（即只包含数字字符，且非空）。
 * 
 * 该函数遍历字符串的每个字符，确保它们都是数字（0-9）。
 * 注意：它不处理前导零或负号，空字符串返回 false。
 */
bool isPositiveInteger(const std::string& s) {
    if (s.empty()) {
        return false;
    }
    for (char ch : s) {
        if (!std::isdigit(static_cast<unsigned char>(ch))) {
            return false;
        }
    }
    return true;
}

/**
 * 解析一行以逗号分隔的先修课程 ID 字符串，返回 ID 列表。
 * 
 * 该函数用于处理从文件或用户输入中读取的、以逗号分隔的课程 ID 字符串，
 * 例如 "C01,C02,C03" 或 "C01, C02, C03"（注意：当前实现不会去除空格，
 * 所以如果输入包含空格，ID 中会包含空格，这可能不是期望的行为。
 * 通常应结合 trim 函数去除每个 token 的首尾空白，但此函数简化了处理，
 * 仅去除了空 token（即连续逗号产生的空串）。
 * 
 * @param line 原始字符串，例如 "C01,C02,C03"。
 * @return     包含解析出的课程 ID 的 vector。如果 line 为空，返回空 vector。
 * 
 * 算法逻辑：
 *   1. 创建一个 std::stringstream 对象 ss，用输入字符串 line 初始化。
 *   2. 使用 std::getline(ss, token, ',') 循环读取由逗号分隔的每个片段。
 *   3. 对每个读取到的 token，检查是否为空串（例如两个逗号相邻的情况）。
 *   4. 如果 token 非空，将其添加到 result 向量中。
 *   5. 循环结束后返回 result。
 * 
 * 注意：该函数对 token 进行 trim（去除首尾空白），如果原始字符串
 *      中包含空格（如 "C01, C02"），则 token 可能为 " C02"（含前导空格），
 *      可能导致后续的课程 ID 校验失败。则实际使用前对每个 token 进行 trim。
 */
std::vector<std::string> parsePrereqLine(const std::string& line){
    std::vector<std::string> result;
    std::stringstream ss(line);
    std::string token;
    while(std::getline(ss, token , ',')){
        token=trim(token);
        if(!token.empty()){
            result.push_back(token);
        }
    }
    return result;
}

/**
 * 从标准输入（键盘）读取课程信息，并填充到指定的 Course 对象中。
 * 
 * 该函数用于交互式输入，依次提示用户输入：
 *   - 课程号（可选，由 id_readonly 参数控制）
 *   - 课程名称
 *   - 课程学分
 *   - 先修课程 ID 列表（逗号分隔）
 * 
 * 输入处理：
 *   - 所有输入均使用 std::getline 读取整行，允许包含空格。
 *   - 学分输入后使用 std::stoi 转换为整数，若转换失败会抛出异常（本函数未捕获，需调用者处理）。
 *   - 先修课程行使用 parsePrereqLine 函数解析为字符串向量。
 * 
 * @param c            要填充的 Course 对象（引用，会被修改）
 * @param id_readonly  是否只读课程号。若为 true，则跳过课程号输入，保留 c.id 原值；
 *                     若为 false，则提示用户输入新课程号。默认值为 false。
 */
void inputCourseFields(Course& c, bool id_readonly = false){
    std::string line;// 临时存储用户输入的每一行
    // 如果课程号不是只读，则提示并读取课程号
    if(!id_readonly){
        std::cout << "课程号(3位字母数字)：";
        std::getline(std::cin, c.id);// 直接读入 c.id，注意可能包含空格
    }
    // 读取课程名称
    std::cout << "课程名称：";
    std::getline(std::cin, c.name);

    // 读取学分（字符串形式），然后转换为整数
    std::cout << "课程学分：";
    std::getline(std::cin, line);
    line = trim(line);
    if (line.empty()) {
        c.credit = 0; // 留给 addCourse 统一校验，避免异常崩溃
        std::cout << "学分为空，已按无效输入处理。\n";
    } else {
        try {
            c.credit = std::stoi(line);
        } catch (...) {
            c.credit = 0; // 留给 addCourse 统一报错
            std::cout << "学分输入无效，已按无效输入处理。\n";
        }
    }

     // 读取先修课程行，并解析为 ID 列表
    std::cout << "先修课程(逗号分隔)：";
    std::getline(std::cin, line);
    c.prereq_ids = parsePrereqLine(line);
}

void inputCourseFieldsForUpdate(Course& c) {
    std::string line;// 临时存储用户输入的每一行
    // 1. 课程名称输入
    std::cout << "课程名称(当前: " << c.name << ", 回车保持不变)：";
    std::getline(std::cin, line);
    line = trim(line);// 去除首尾空白
    if (!line.empty()) {// 非空表示用户输入了内容（可能是新的名称）
        c.name = line;// 更新课程名称
    }// 若输入为空，则保持原值

    // 2. 课程学分输入
    std::cout << "课程学分(当前: " << c.credit << ", 回车保持不变)：";
    std::getline(std::cin, line);
    line = trim(line);
    if (!line.empty()) {// 用户输入了内容，尝试转换为整数
        try {
            c.credit = std::stoi(line);// 转换学分，可能抛出异常
        } catch (...) {// 捕获所有异常（如输入非数字）
            std::cout << "学分输入无效，保持原值。\n";// 学分保持原值（不修改）
        }
    }// 若输入为空，则学分保持不变

    // 3. 先修课程列表输入
    std::cout << "先修课程(当前: " << joinPrereq(c.prereq_ids)
              << ", 输入逗号分隔；输入-清空；回车保持不变)：";
    std::getline(std::cin, line);
    line = trim(line);
    if (line == "-") {// 特殊处理：输入单个减号表示清空
        c.prereq_ids.clear();// 清空先修课程列表
    } else if (!line.empty()) {// 输入非空且不是减号，则视为新的先修列表字符串
        c.prereq_ids = parsePrereqLine(line);// 解析后替换原列表
    }// 若输入为空，则先修列表保持不变
}

void inputPlanConfig(PlanConfig& config) {
    std::string line;

    std::cout << "输入最大学期数(默认 " << config.max_semesters << "): ";
    std::getline(std::cin, line);
    line = trim(line);
    if (!line.empty()) {
        try {
            config.max_semesters = std::stoi(line);
        } catch (...) {
            std::cout << "输入无效，使用默认值。\n";
        }
    }

    std::cout << "输入单学期学分上限(默认 " << config.max_credits_per_semester << "): ";
    std::getline(std::cin, line);
    line = trim(line);
    if (!line.empty()) {
        try {
            config.max_credits_per_semester = std::stoi(line);
        } catch (...) {
            std::cout << "输入无效，使用默认值。\n";
        }
    }

    std::cout << "选择策略 1=均匀负担 2=前置集中 (默认1): ";
    std::getline(std::cin, line);
    line = trim(line);
    if (line == "2") {
        config.strategy = ScheduleStrategy::FRONT_LOADED;
    } else {
        config.strategy = ScheduleStrategy::BALANCED;
    }
}

void printPlanPreview(const std::vector<SemesterPlan>& current_plan,
                      const std::unordered_map<std::string, Course>& cmap) {
    if (current_plan.empty()) {
        std::cout << "当前尚未生成课表。\n";
        return;
    }

    std::cout << "\n当前排课结果：\n";
    for (size_t i = 0; i < current_plan.size(); ++i) {
        std::cout << "第 " << (i + 1) << " 学期  总学分: " << current_plan[i].total_credits << "\n";
        for (const auto& id : current_plan[i].course_ids) {
            auto it = cmap.find(id);
            if (it != cmap.end()) {
                std::cout << "  - " << it->second.id << " | "
                          << it->second.name << " | " << it->second.credit << "学分\n";
            } else {
                std::cout << "  - " << id << "\n";
            }
        }
        std::cout << "\n";
    }
}

void printStudentList(const std::vector<StudentInfo>& students) {
    if (students.empty()) {
        std::cout << "student 表为空。\n";
        return;
    }
    std::cout << "ID | 姓名 | 主修 | 年级\n";
    for (const auto& s : students) {
        std::cout << s.student_id << " | " << s.name << " | " << s.major << " | " << s.grade << "\n";
    }
}

void printStudentCourseHistory(const std::vector<LearnedCourse>& history) {
    if (history.empty()) {
        std::cout << "该学生暂无课程记录。\n";
        return;
    }
    std::cout << "semester | course_id | course_name | status | score\n";
    for (const auto& r : history) {
        std::cout << r.semester << " | " << r.course_id << " | " << r.course_name
                  << " | " << r.status << " | ";
        if (r.has_score) {
            std::cout << r.score;
        } else {
            std::cout << "NULL";
        }
        std::cout << "\n";
    }
}

bool parsePositiveIntStrict(const std::string& s, int& out) {
    std::string t = trim(s);
    if (t.empty()) {
        return false;
    }
    try {
        int v = std::stoi(t);
        if (v <= 0) {
            return false;
        }
        out = v;
        return true;
    } catch (...) {
        return false;
    }
}
//构建 ODBC 连接字符串，用于连接 MySQL 数据库。
std::string buildOdbcConnStr(const std::string& uid, const std::string& pwd){
    return "DRIVER={MySQL ODBC 9.6 Unicode Driver};"
           "SERVER=127.0.0.1;"
           "PORT=3306;"
           "DATABASE=university_scheduler;"
           "UID=" + uid + ";"
           "PWD=" + pwd + ";"
           "OPTION=3;";
}

// 根据学生已完成的课程集合，从所有课程中筛选出尚未完成的课程（待修课程），
// 并移除这些待修课程中已经完成的先修课程依赖。
std::vector<Course> buildPendingCoursesByCompleted(
    const std::vector<Course>& all_courses,
    const std::unordered_set<std::string>& completed_ids
){
    std::vector<Course> pending;
    for(const auto& c : all_courses){
        if(completed_ids.count(c.id) > 0){
            continue; //这门课已修完，直接剔除
        }

        // 复制课程，因为需要修改其先修列表
        Course copy = c;
        std::vector<std::string> kept_prereq;
        for(const auto& pre : c.prereq_ids){
            if(completed_ids.count(pre) == 0){
                // 如果先修课程尚未完成，则保留依赖
                kept_prereq.push_back(pre);
            }
        }
        copy.prereq_ids = kept_prereq;// 更新先修列表（已完成的先修被移除）
        pending.push_back(copy);
    }
    return pending;
}

bool connectDbByPrompt(OdbcDb& db, std::string& err) {
    std::string uid, pwd;
    std::cout << "MySQL user: ";
    std::getline(std::cin, uid);
    std::cout << "MySQL password: ";
    std::getline(std::cin, pwd);
    return db.connect(buildOdbcConnStr(uid, pwd), err);
}

bool syncCacheCoursesToDbInteractive(const std::vector<Course>& cache_courses) {
    if (cache_courses.empty()) {
        return true;
    }

    std::cout << "将缓存课程同步到数据库 course 表...\n";
    OdbcDb db;
    std::string err;
    if (!connectDbByPrompt(db, err)) {
        std::cout << "数据库连接失败: " << err << "\n";
        return false;
    }

    CourseSyncStats stats;
    if (!db.upsertCourses(cache_courses, stats, err)) {
        std::cout << "同步 course 表失败: " << err << "\n";
        return false;
    }

    std::cout << "course 同步完成: 新增 " << stats.inserted
              << "，更新/已存在 " << stats.updated_or_unchanged << "\n";
    return true;
}

bool buildSchedulingCoursesInteractive(const std::vector<Course>& all_courses,
                                       std::vector<Course>& scheduling_courses) {
    scheduling_courses = all_courses;

    std::string use_db;
    std::cout << "是否读取数据库已修课程并剔除? (y/N): ";
    std::getline(std::cin, use_db);
    use_db = trim(use_db);
    if (!(use_db == "y" || use_db == "Y")) {
        return true;
    }

    OdbcDb db;
    std::string err;
    if (!connectDbByPrompt(db, err)) {
        std::cout << "数据库连接失败: " << err << "\n";
        return false;
    }

    std::string sid;
    std::cout << "Student ID: ";
    std::getline(std::cin, sid);
    sid = trim(sid);
    if (sid.empty()) {
        std::cout << "学号不能为空。\n";
        return false;
    }

    StudentInfo stu;
    bool found = false;
    if (!db.getStudentById(sid, stu, found, err)) {
        std::cout << "读取学生信息失败: " << err << "\n";
        return false;
    }
    if (!found) {
        std::cout << "未找到学生: " << sid << "\n";
        return false;
    }

    std::cout << "学生信息: " << stu.student_id << " | " << stu.name
              << " | " << stu.major << " | " << stu.grade << "\n";

    std::vector<LearnedCourse> history;
    if (!db.listStudentCourses(sid, history, err)) {
        std::cout << "读取学生历史失败: " << err << "\n";
        return false;
    }

    std::cout << "学生历史课程情况：\n";
    printStudentCourseHistory(history);

    std::unordered_set<std::string> completed_ids;
    for (const auto& r : history) {
        if (r.status == "COMPLETED") {
            completed_ids.insert(r.course_id);
        }
    }

    scheduling_courses = buildPendingCoursesByCompleted(all_courses, completed_ids);
    std::cout << "已修完成 " << completed_ids.size() << " 门，待排 "
              << scheduling_courses.size() << " 门。\n";
    return true;
}

std::vector<StudentCoursePlanRow> buildPlanRowsFromPlan(const std::vector<SemesterPlan>& current_plan) {
    std::vector<StudentCoursePlanRow> rows;
    for (size_t i = 0; i < current_plan.size(); ++i) {
        int sem = static_cast<int>(i) + 1;
        for (const auto& cid : current_plan[i].course_ids) {
            StudentCoursePlanRow r;
            r.course_id = cid;
            r.semester = sem;
            r.status = "PLANNED";
            rows.push_back(r);
        }
    }
    return rows;
}

bool writePlanToDbInteractive(const std::vector<SemesterPlan>& current_plan) {
    if (current_plan.empty()) {
        return true;
    }

    std::string ans;
    std::cout << "是否将排课结果写入数据库 student_course 表? (y/N): ";
    std::getline(std::cin, ans);
    ans = trim(ans);
    if (!(ans == "y" || ans == "Y")) {
        return true;
    }

    OdbcDb db;
    std::string err;
    if (!connectDbByPrompt(db, err)) {
        std::cout << "数据库连接失败: " << err << "\n";
        return false;
    }

    std::string sid;
    std::cout << "Student ID: ";
    std::getline(std::cin, sid);
    sid = trim(sid);
    if (sid.empty()) {
        std::cout << "学号不能为空。\n";
        return false;
    }

    std::string mode;
    std::cout << "写入模式: 1=追加去重(推荐) 2=覆盖当前学生计划后写入 : ";
    std::getline(std::cin, mode);
    mode = trim(mode);

    if (mode == "2") {
        int deleted = 0;
        if (!db.deleteStudentPlannedRows(sid, deleted, err)) {
            std::cout << "删除旧计划失败: " << err << "\n";
            return false;
        }
        std::cout << "已删除该学生旧 PLANNED/ENROLLED 记录: " << deleted << " 条\n";
    }

    auto rows = buildPlanRowsFromPlan(current_plan);
    PlanWriteStats stats;
    if (!db.insertStudentPlanRowsDedup(sid, rows, stats, err)) {
        std::cout << "写入 student_course 失败: " << err << "\n";
        return false;
    }

    std::cout << "写入完成: 新增 " << stats.inserted
              << "，重复跳过 " << stats.duplicated << "\n";
    return true;
}

/**
 * 程序入口
 */
int main(){
    int choice = -1; //表示用户选择的菜单序号，默认-1表示未选择

    //循环菜单
    while(1){
        clearScreen();
        printMenu();//显示菜单

        //读取用户输入
        if (!(std::cin >> choice)) {
            std::cout << "输入错误，请输入序号: ";
            std::cin.clear(); // 清除错误标志
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n'); // 忽略输入缓冲区中的无效字符 前者代表streamsize中的最大值 后者表示到\n结束
            continue; // 继续下一次循环
        }

        //根据用户的选择执行不同操作
        switch (choice){

            case 1: // 选项 1：导入课程目录
            {
                std::string err;
                std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

                std::string source;
                std::cout << "导入来源: 1=TXT文件 2=数据库course表 (默认1): ";
                std::getline(std::cin, source);
                source = trim(source);

                if (source == "2") {
                    OdbcDb db;
                    if (!connectDbByPrompt(db, err)) {
                        std::cout << "数据库连接失败: " << err << "\n";
                        waitForEnter();
                        break;
                    }

                    if (!db.listCoursesFromDb(courses, err)) {
                        std::cout << "从数据库导入课程失败: " << err << "\n";
                        waitForEnter();
                        break;
                    }

                    rebuildCourseMap(courses, course_map);
                    plan.clear();
                    std::cout << "从数据库导入成功，共 " << courses.size() << " 门课程。\n";
                    std::cout << "已尝试同步导入先修关系（来自 course_prereq 表）。\n";
                    waitForEnter();
                    break;
                }

                std::string path;
                const auto txt_files = listTxtFilesInDataDir();
                if (!txt_files.empty()) {
                    std::cout << "检测到以下可导入课程文件：\n";
                    for (size_t i = 0; i < txt_files.size(); ++i) {
                        std::cout << "  " << (i + 1) << ". " << txt_files[i] << "\n";
                    }
                } else {
                    std::cout << "未在 data 目录检测到 .txt 文件。\n";
                }

                std::cout << "请输入文件编号或完整路径(回车默认 data/courses.txt): ";
                std::getline(std::cin, path);
                path = trim(path);

                if (path.empty()) {
                    path = "data/courses.txt";
                } else if (isPositiveInteger(path) && !txt_files.empty()) {
                    size_t idx = static_cast<size_t>(std::stoul(path));
                    if (idx >= 1 && idx <= txt_files.size()) {
                        path = txt_files[idx - 1];
                    } else {
                        std::cout << "编号超出范围，改用原始输入作为路径。\n";
                    }
                }
                std::cout << "本次导入文件: " << path << "\n";

                if (importCoursesFromTxt(path, courses, err)) {
                    rebuildCourseMap(courses, course_map);
                    plan.clear();
                    std::cout << "导入课程目录成功，共导入 " << courses.size() << " 门课程\n";
                } else {
                    std::cout << "导入课程目录失败: " << err << "\n";
                }
                waitForEnter();//界面停留，等待用户按键
                break;
            }

            case 2: // 选项 2：编辑课程目录
            {
                while(1){
                    clearScreen();
                    int sub = -1;
                    std::cout <<"\n--- 编辑课程目录---\n";
                    std::cout <<"1.查看课程\n";
                    std::cout <<"2.新增课程\n";
                    std::cout <<"3.修改课程\n";
                    std::cout <<"4.删除课程\n";
                    std::cout <<"0.返回主菜单\n";
                    std::cout <<"请输入序号: ";
                    if(!(std::cin >> sub)){
                        std::cout << "输入错误，请输入序号: ";
                        std::cin.clear(); // 清除错误标志
                        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n'); // 忽略输入缓冲区中的无效字符 前者代表streamsize中的最大值 后者表示到\n结束
                        continue; // 继续下一次循环
                    }
                    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n'); // 忽略输入缓冲区中的无效字符 前者代表streamsize中的最大值 后者表示到\n结束
                    if (sub == 0 ) break;
                    if (sub == 1 ) {
                        listCourses(courses);
                        waitForEnter();//界面停留，等待用户按键
                    }else if (sub == 2 ) {
                        if (courses.empty()) {
                            std::cout << "当前课程目录为空。你可以先录入基础课程；此时先修课程应留空。\n";
                        } else {
                            std::cout << "当前课程目录如下（可参考课程号与先修关系）：\n";
                            listCourses(courses);
                        }
                        Course c;
                        std::string err;
                        inputCourseFields(c);
                        if (addCourse(courses , c , err)){
                            rebuildCourseMap(courses, course_map); // 重建课程映射
                            plan.clear(); // 课程目录变更后，旧计划作废
                            std::cout<<"新增课程成功，课程号: "<<courses.back().id<<"\n";
                        }else{
                            std::cout<<"新增课程失败: "<<err<<"\n";
                        }
                        waitForEnter();
                    }else if (sub == 3 ) {
                        std::string id , err;
                        if (courses.empty()) {
                            std::cout << "当前课程目录为空，无法修改。\n";
                            waitForEnter();
                            continue;
                        }
                        std::cout << "当前课程目录如下：\n";
                        listCourses(courses);
                        std::cout << "输入要修改的课程号: ";
                        std::getline(std::cin, id);
                        id = trim(id);

                        int idx = findCourseIndexById(courses, id);
                        if (idx == -1) {
                            std::cout << "未找到课程: " << id << "\n";
                            waitForEnter();
                            continue;
                        }

                        Course c = courses[idx];
                        std::cout << "正在修改课程: " << c.id << " | " << c.name
                                  << " | 学分 " << c.credit << " | 先修 " << joinPrereq(c.prereq_ids) << "\n";
                        inputCourseFieldsForUpdate(c);

                        if(updateCourse(courses , c , err)){
                            rebuildCourseMap(courses, course_map); // 重建课程映射
                            plan.clear(); // 课程目录变更后，旧计划作废
                            std::cout<<"修改课程成功，课程号: "<<c.id<<"\n";
                        }else{
                            std::cout<<"修改课程失败: "<<err<<"\n";
                        }
                        waitForEnter();
                    }else if (sub == 4 ) {
                        std::string id, err;
                        if (courses.empty()) {
                            std::cout << "当前课程目录为空，无法删除。\n";
                            waitForEnter();
                            continue;
                        }
                        std::cout << "当前课程目录如下：\n";
                        listCourses(courses);
                        std::cout << "输入要删除的课程号：";
                        std::getline(std::cin, id);
                        if(removeCourse(courses,id,err)){
                            rebuildCourseMap(courses, course_map); // 重建课程映射
                            plan.clear(); // 课程目录变更后，旧计划作废
                            std::cout<<"删除课程成功，课程号: "<<id<<"\n";
                        }else{
                            std::cout<<"删除课程失败: "<<err<<"\n";
                        }
                        waitForEnter();
                    }else{
                        std::cout << "无效选项，请重新输入序号: ";
                    }
                }
                break;
            }
            case 3: // 选项 3：生成排课计划
            {
                std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

                if (courses.empty()) {
                    std::cout << "请先导入课程目录\n";
                    waitForEnter();
                    break;
                }

                if (!syncCacheCoursesToDbInteractive(courses)) {
                    waitForEnter();
                    break;
                }

                std::cout << "当前课程目录如下（生成排课计划参考）：\n";
                listCourses(courses);
                std::cout << "\n";

                std::vector<Course> scheduling_courses;
                if (!buildSchedulingCoursesInteractive(courses, scheduling_courses)) {
                    waitForEnter();
                    break;
                }

                if (scheduling_courses.empty()) {
                    std::cout << "当前学生无待排课程。\n";
                    plan.clear();
                    waitForEnter();
                    break;
                }

                PlanConfig config;
                std::string err;
                inputPlanConfig(config);

                if (!generateSemesterPlan(scheduling_courses, config, plan, err)) {
                    std::cout << "生成排课计划失败: " << err << "\n";
                    waitForEnter();
                    break;
                }

                rebuildCourseMap(courses, course_map); // 仅用于打印课程名和学分
                std::cout << "\n排课计划生成成功。\n";
                printPlanPreview(plan, course_map);
                waitForEnter();
                break;
            }
            case 4: // 选项 4：导出排课计划
            {
                std::string path;
                std::string err;

                std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
                if (courses.empty()) {
                    std::cout << "请先导入课程目录\n";
                    waitForEnter();
                    break;
                }

                if (!syncCacheCoursesToDbInteractive(courses)) {
                    waitForEnter();
                    break;
                }

                std::cout << "当前课程目录如下（导出前参考）：\n";
                listCourses(courses);
                std::cout << "\n";

                auto regenerateForExport = [&]() -> bool {
                    std::vector<Course> scheduling_courses;
                    if (!buildSchedulingCoursesInteractive(courses, scheduling_courses)) {
                        return false;
                    }
                    if (scheduling_courses.empty()) {
                        std::cout << "当前学生无待排课程。\n";
                        plan.clear();
                        return true;
                    }

                    PlanConfig config;
                    inputPlanConfig(config);
                    if (!generateSemesterPlan(scheduling_courses, config, plan, err)) {
                        std::cout << "生成排课计划失败: " << err << "\n";
                        return false;
                    }
                    rebuildCourseMap(courses, course_map);
                    return true;
                };

                if (plan.empty()) {
                    std::cout << "当前还没有已生成课表，需要先生成。\n";
                    if (!regenerateForExport()) {
                        waitForEnter();
                        break;
                    }
                } else {
                    std::string line;
                    std::cout << "是否重新生成课表后再导出? (y/N): ";
                    std::getline(std::cin, line);
                    line = trim(line);
                    if (line == "y" || line == "Y") {
                        if (!regenerateForExport()) {
                            waitForEnter();
                            break;
                        }
                    }
                }

                if (plan.empty()) {
                    std::cout << "当前无可导出课表。\n";
                    waitForEnter();
                    break;
                }

                printPlanPreview(plan, course_map);
                std::cout << "请输入导出文件路径(回车默认 output/plan.txt):";
                std::getline(std::cin, path);
                if (path.empty()) {
                    path = "output/plan.txt";
                }

                if (exportPlanToTxt(path, plan, course_map, err)) {
                    std::cout << "导出排课计划成功:" << path << "\n";
                    if (!writePlanToDbInteractive(plan)) {
                        waitForEnter();
                        break;
                    }
                } else {
                    std::cout << "导出排课计划失败: " << err << "\n";
                }

                waitForEnter();
                break;
            }
            case 5: // 选项 5：管理学生
            {
                std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

                OdbcDb db;
                std::string err;
                if (!connectDbByPrompt(db, err)) {
                    std::cout << "数据库连接失败: " << err << "\n";
                    waitForEnter();
                    break;
                }

                while (1) {
                    clearScreen();
                    int sub = -1;
                    std::cout << "\n--- 学生管理 ---\n";
                    std::cout << "1. 查看学生列表\n";
                    std::cout << "2. 新增学生\n";
                    std::cout << "3. 修改学生\n";
                    std::cout << "4. 删除学生\n";
                    std::cout << "5. 查看某学生课程情况\n";
                    std::cout << "0. 返回主菜单\n";
                    std::cout << "请输入序号: ";

                    if (!(std::cin >> sub)) {
                        std::cout << "输入错误，请输入序号。\n";
                        std::cin.clear();
                        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
                        continue;
                    }
                    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

                    if (sub == 0) {
                        break;
                    }

                    if (sub == 1) {
                        std::vector<StudentInfo> students;
                        if (!db.listStudents(students, err)) {
                            std::cout << "读取学生列表失败: " << err << "\n";
                        } else {
                            printStudentList(students);
                        }
                        waitForEnter();
                    } else if (sub == 2) {
                        std::vector<StudentInfo> students;
                        if (db.listStudents(students, err)) {
                            std::cout << "当前学生列表（新增时可参考）：\n";
                            printStudentList(students);
                        }

                        StudentInfo s;
                        std::string line;
                        std::cout << "学号: ";
                        std::getline(std::cin, s.student_id);
                        s.student_id = trim(s.student_id);
                        std::cout << "姓名: ";
                        std::getline(std::cin, s.name);
                        s.name = trim(s.name);
                        std::cout << "主修: ";
                        std::getline(std::cin, s.major);
                        s.major = trim(s.major);
                        std::cout << "年级(正整数): ";
                        std::getline(std::cin, line);

                        if (!parsePositiveIntStrict(line, s.grade)) {
                            std::cout << "年级输入无效。\n";
                            waitForEnter();
                            continue;
                        }

                        if (!db.addStudent(s, err)) {
                            std::cout << "新增学生失败: " << err << "\n";
                        } else {
                            std::cout << "新增学生成功。\n";
                        }
                        waitForEnter();
                    } else if (sub == 3) {
                        std::vector<StudentInfo> students;
                        if (!db.listStudents(students, err)) {
                            std::cout << "读取学生列表失败: " << err << "\n";
                            waitForEnter();
                            continue;
                        }
                        printStudentList(students);

                        std::string sid;
                        std::cout << "输入要修改的学号: ";
                        std::getline(std::cin, sid);
                        sid = trim(sid);

                        StudentInfo old_s;
                        bool found = false;
                        if (!db.getStudentById(sid, old_s, found, err)) {
                            std::cout << "读取学生信息失败: " << err << "\n";
                            waitForEnter();
                            continue;
                        }
                        if (!found) {
                            std::cout << "未找到学生: " << sid << "\n";
                            waitForEnter();
                            continue;
                        }

                        StudentInfo new_s = old_s;
                        std::string line;
                        std::cout << "姓名(当前: " << old_s.name << ", 回车保持): ";
                        std::getline(std::cin, line);
                        line = trim(line);
                        if (!line.empty()) {
                            new_s.name = line;
                        }

                        std::cout << "主修(当前: " << old_s.major << ", 回车保持): ";
                        std::getline(std::cin, line);
                        line = trim(line);
                        if (!line.empty()) {
                            new_s.major = line;
                        }

                        std::cout << "年级(当前: " << old_s.grade << ", 回车保持): ";
                        std::getline(std::cin, line);
                        line = trim(line);
                        if (!line.empty()) {
                            int g = 0;
                            if (!parsePositiveIntStrict(line, g)) {
                                std::cout << "年级输入无效，取消修改。\n";
                                waitForEnter();
                                continue;
                            }
                            new_s.grade = g;
                        }

                        if (!db.updateStudent(new_s, err)) {
                            std::cout << "修改学生失败: " << err << "\n";
                        } else {
                            std::cout << "修改学生成功。\n";
                        }
                        waitForEnter();
                    } else if (sub == 4) {
                        std::vector<StudentInfo> students;
                        if (!db.listStudents(students, err)) {
                            std::cout << "读取学生列表失败: " << err << "\n";
                            waitForEnter();
                            continue;
                        }
                        printStudentList(students);

                        std::string sid;
                        std::cout << "输入要删除的学号: ";
                        std::getline(std::cin, sid);
                        sid = trim(sid);
                        std::string confirm;
                        std::cout << "确认删除学生 " << sid << " ? (y/N): ";
                        std::getline(std::cin, confirm);
                        confirm = trim(confirm);
                        if (!(confirm == "y" || confirm == "Y")) {
                            std::cout << "已取消删除。\n";
                            waitForEnter();
                            continue;
                        }

                        if (!db.deleteStudent(sid, err)) {
                            std::cout << "删除学生失败: " << err << "\n";
                        } else {
                            std::cout << "删除学生成功。\n";
                        }
                        waitForEnter();
                    } else if (sub == 5) {
                        std::vector<StudentInfo> students;
                        if (!db.listStudents(students, err)) {
                            std::cout << "读取学生列表失败: " << err << "\n";
                            waitForEnter();
                            continue;
                        }
                        printStudentList(students);

                        std::string sid;
                        std::cout << "输入学号以查看课程情况: ";
                        std::getline(std::cin, sid);
                        sid = trim(sid);

                        StudentInfo s;
                        bool found = false;
                        if (!db.getStudentById(sid, s, found, err)) {
                            std::cout << "读取学生信息失败: " << err << "\n";
                            waitForEnter();
                            continue;
                        }
                        if (!found) {
                            std::cout << "未找到学生: " << sid << "\n";
                            waitForEnter();
                            continue;
                        }

                        std::cout << "学生信息: " << s.student_id << " | "
                                  << s.name << " | " << s.major << " | " << s.grade << "\n";
                        std::vector<LearnedCourse> history;
                        if (!db.listStudentCourses(sid, history, err)) {
                            std::cout << "读取学生课程失败: " << err << "\n";
                        } else {
                            printStudentCourseHistory(history);
                        }
                        waitForEnter();
                    } else {
                        std::cout << "无效选项。\n";
                        waitForEnter();
                    }
                }
                break;
            }
            case 0: // 选项 0：退出程序
            
                std::cout << "退出程序\n";
                return 0; // 正常退出程序
            default: // 其他无效选项
                std::cout << "无效选项，请重新输入序号: ";
                break;
            }
        }
    

    return 0;
}
