#include "../include/course_io.h"

#include <algorithm> // 用于 std::all_of 等算法
#include <cctype> // 用于 std::isspace, std::isalnum 等字符判断
#include <fstream> // 文件输入输出流 std::ifstream, std::ofstream
#include <sstream> // 字符串流 std::stringstream，用于分割字符串
#include <unordered_set> // 用于存储已读课程 ID，检查重复和先修课程存在性

// 匿名命名空间：内部链接，这些辅助函数只在当前编译单元可见，避免与其他文件同名函数冲突

namespace {


/**
 * 去除字符串首尾的空白字符（空格、制表符、换行等）。
 * @param input 输入字符串
 * @return 去除首尾空白后的新字符串
 */
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


/**
 * 使用指定分隔符分割字符串。
 * @param input 输入字符串
 * @param delimiter 分隔符字符
 * @return 分割后的子串列表（不包含分隔符）
 */
std::vector<std::string> split(const std::string& input, char delimiter) {
    std::vector<std::string> result;
    std::stringstream ss(input);
    std::string token;
    // 逐次读取由 delimiter 分隔的片段
    while (std::getline(ss, token, delimiter)) {
        result.push_back(token);
    }
    if(!input.empty() && input.back() == delimiter){
        result.push_back("");
    }// 处理末尾分隔符后的空字符串
    return result;
}

/**
 * 检查课程 ID 是否合法：必须为 3 个字符且全部是字母或数字。
 * @param id 课程 ID 字符串
 * @return 合法返回 true，否则 false
 */
bool isValidCourseId(const std::string& id) {
    if (id.size() != 3) {
        return false;
    }
    // std::all_of 检查每个字符是否都是字母数字
    return std::all_of(id.begin(), id.end(), [](unsigned char c) {
        return std::isalnum(c) != 0;
    });
}

}// namespace


/**
 * 从文本文件导入课程目录。
 * 
 * 文件格式约定：
 *   第一行：课程数量（整数）
 *   后续每行：课程ID|课程名|学分|先修课程ID列表（多个ID用逗号分隔，可为空）
 *   示例：
 *     3
 *     C01|数据结构|3|C00
 *     C02|高等数学|4|
 *     C03|算法|3|C01,C02
 * 
 * @param path 文件路径
 * @param courses [输出] 存储解析成功的课程列表
 * @param error_message [输出] 错误信息
 * @return 成功返回 true，失败返回 false
 */
bool importCoursesFromTxt(
    const std::string& path,
    std::vector<Course>& courses,
    std::string& error_message
) {
    // 打开文件
    std::ifstream fin(path);
    if (!fin.is_open()) {
        error_message = "无法打开课程文件: " + path;
        return false;
    }

    // 读取第一行（课程数量）
    std::string first_line;
    if (!std::getline(fin, first_line)) {
        error_message = "课程文件为空";
        return false;
    }

    int expected_count = 0;
    try {
        // 去除首尾空白后转换为整数
        expected_count = std::stoi(trim(first_line)); //将文件第一行字符串转换为整数，并存入 expected_count 变量。
    } catch (...) {
        error_message = "第一行必须是课程数量整数";
        return false;
    }

    if (expected_count < 0) {
        error_message = "课程数量不能为负数";
        return false;
    }

    // 用于存储解析出的课程，预分配容量提升性能
    std::vector<Course> parsed;
    parsed.reserve(static_cast<size_t>(expected_count));
    // 记录已经出现的课程 ID，用于检查重复和后续先修课程存在性
    std::unordered_set<std::string> id_set;

    // 循环读取每一门课程，行号从第二行开始（line_no 从 2 起计数）
    for (int line_no = 2; line_no < expected_count + 2; ++line_no) {
        std::string line;
        if (!std::getline(fin, line)) {
            error_message = "课程行数不足，期望 " + std::to_string(expected_count) + " 行";
            return false;
        }

        // 按 '|' 分割一行
        auto parts = split(line, '|');
        if (parts.size() != 4) {
            error_message = "第 " + std::to_string(line_no) + " 行格式错误，应为: 课程号|课程名|学分|先修列表";
            return false;
        }

        Course c;
        c.id = trim(parts[0]);
        c.name = trim(parts[1]);
        const std::string credit_text = trim(parts[2]);
        const std::string prereq_text = trim(parts[3]);

        // 验证课程 ID 合法性
        if (!isValidCourseId(c.id)) {
            error_message = "第 " + std::to_string(line_no) + " 行课程号非法: " + c.id;
            return false;
        }
        // 检查课程 ID 是否重复
        if (id_set.count(c.id) != 0) {
            error_message = "第 " + std::to_string(line_no) + " 行课程号重复: " + c.id;
            return false;
        }
        // 课程名不能为空
        if (c.name.empty()) {
            error_message = "第 " + std::to_string(line_no) + " 行课程名不能为空";
            return false;
        }

        // 解析学分（整数）
        try {
            c.credit = std::stoi(credit_text);
        } catch (...) {
            error_message = "第 " + std::to_string(line_no) + " 行学分不是整数";
            return false;
        }
        if (c.credit <= 0) {
            error_message = "第 " + std::to_string(line_no) + " 行学分必须 > 0";
            return false;
        }

        // 解析先修课程列表（若不为空）
        if (!prereq_text.empty()) {
            auto prereq_parts = split(prereq_text, ',');
            std::unordered_set<std::string> prereq_seen; // 用于去重同一行中的重复先修 ID
            for (const std::string& raw_id : prereq_parts) {
                const std::string prereq_id = trim(raw_id);
                if (prereq_id.empty()) {
                    continue; // 忽略空白部分（例如 "C01,,C02" 中的空串）
                }
                // 验证先修课程 ID 格式
                if (!isValidCourseId(prereq_id)) {
                    error_message = "第 " + std::to_string(line_no) + " 行存在非法先修课程号: " + prereq_id;
                    return false;
                }
                // 课程不能将自己作为先修
                if (prereq_id == c.id) {
                    error_message = "第 " + std::to_string(line_no) + " 行课程不能将自己设为先修: " + c.id;
                    return false;
                }
                // 如果先修 ID 尚未在这一行中出现过，则加入 prereq_ids，并记录到 prereq_seen 去重
                if (prereq_seen.insert(prereq_id).second) {
                    c.prereq_ids.push_back(prereq_id);
                }
                // 如果已存在，则忽略重复，但不会报错（允许重复但只添加一次）
            }
        }
        // 将当前课程 ID 加入集合，标记为已存在
        id_set.insert(c.id);
        // 暂存解析好的课程
        parsed.push_back(c);
    }

    // 第二遍检查：验证每门课程的所有先修课程是否都在已解析的课程中（即 ID 存在）
    for (const Course& c : parsed) {
        for (const std::string& prereq_id : c.prereq_ids) {
            if (id_set.count(prereq_id) == 0) {
                error_message = "课程 " + c.id + " 的先修课程不存在: " + prereq_id;
                return false;
            }
        }
    }

    // 全部成功，将 parsed 移动给输出参数 courses（避免拷贝）
    courses = std::move(parsed);
    return true;
}


/**
 * 将排课计划导出到文本文件。
 * 
 * 输出格式示例：
 *   Course Schedule Plan
 *   Total semesters: 2
 * 
 *   Semester 1
 *   ID|Name|Credit
 *   C01|数据结构|3
 *   C02|高等数学|4
 *   Total Credits: 7
 * 
 *   Semester 2
 *   ID|Name|Credit
 *   C03|算法|3
 *   Total Credits: 3
 * 
 * @param path 导出文件路径
 * @param plan 排课计划（每个学期的课程 ID 列表）
 * @param course_map 课程映射表（ID -> Course），用于获取课程详细信息
 * @param error_message [输出] 错误信息
 * @return 成功返回 true，失败返回 false
 */

bool exportPlanToTxt(
    const std::string& path,
    const std::vector<SemesterPlan>& plan,
    const std::unordered_map<std::string, Course>& course_map,
    std::string& error_message
) {
    // 打开文件准备写入（如果文件存在会被覆盖）
    std::ofstream fout(path);
    if (!fout.is_open()) {
        error_message = "无法写入导出文件: " + path;
        return false;
    }
    // 写入文件头
    fout << "课程安排\n";
    fout << "总学期数: " << plan.size() << "\n\n";

    // 遍历每个学期
    for (size_t i = 0; i < plan.size(); ++i) {
        const SemesterPlan& semester = plan[i];
        fout << "学期 " << (i + 1) << "\n";
        fout << "ID|课程名|学分\n";

        int computed_credits = 0; // 用于重新计算该学期总学分
        for (const std::string& course_id : semester.course_ids) {
            auto it = course_map.find(course_id);
            // 如果课程 ID 在映射中不存在，导出失败
            if (it == course_map.end()) {
                error_message = "导出失败，课程不存在: " + course_id;
                return false;
            }
            const Course& course = it->second;
            // 写入课程信息
            fout << course.id << "|" << course.name << "|" << course.credit << "\n";
            computed_credits += course.credit;
        }

        // 决定输出的总学分：如果 semester.total_credits 已设置（>0），则用它；否则用重新计算的值
        // 这样即使 semester.total_credits 未正确设置，也能得到正确数值
        const int credits_to_print = (semester.total_credits > 0) ? semester.total_credits : computed_credits;
        fout << "总学分: " << credits_to_print << "\n\n";
    }

    return true;
}
