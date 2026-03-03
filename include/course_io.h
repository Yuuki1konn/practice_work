#ifndef COURSE_IO_H // 头文件保护宏，防止同一头文件被多次包含导致重复定义
#define COURSE_IO_H

#include <string>
#include <unordered_map> // 引入 std::unordered_map，用于高效地通过课程 ID 查找课程对象
#include <vector>

#include "models.h"

/**
 * 从文本文件中导入课程目录。
 * 
 * @param path           要读取的文本文件路径（例如 "courses.txt"）。
 * @param courses        [输出参数] 成功解析后，将课程对象添加到该 vector 中。
 * @param error_message  [输出参数] 如果解析失败，此处会存储具体的错误描述。
 * @return               成功导入返回 true，失败返回 false。
 * 
 * 函数预期行为：
 *   - 文件格式通常为每行描述一门课程，例如 "C01 数据结构 3 C00" 等（具体格式由实现定义）。
 *   - 如果文件不存在、格式错误或包含无效数据，应返回 false 并设置 error_message。
 *   - 成功时 courses 将被填充，并返回 true。
 */

bool importCoursesFromTxt(
    const std::string& path,    // 输入：文件路径，使用 const 引用避免拷贝
    std::vector<Course>& courses, // 输出：解析后的课程对象列表
    std::string& error_message    // 输出：错误信息字符串的引用，用于返回详细错误
);


/**
 * 将排课计划导出到文本文件。
 * 
 * @param path           要写入的文本文件路径。
 * @param plan           学期计划列表，每个元素表示一个学期的选课结果。
 * @param course_map     课程映射表，键为课程 ID，值为对应的 Course 对象。
 *                       用于在导出时获取课程名称、学分等信息，以便生成可读性好的输出。
 * @param error_message  [输出参数] 如果写入失败，此处会存储具体的错误描述。
 * @return               成功导出返回 true，失败返回 false。
 * 
 * 函数预期行为：
 *   - 将 plan 中的课程 ID 转换为课程名称（借助 course_map）写入文件。
 *   - 文件格式通常为每个学期以标题分隔，例如 "Semester 1:" 后跟课程列表。
 *   - 如果文件无法创建或写入过程中发生错误，应返回 false 并设置 error_message。
 *   - 成功时文件被创建（或覆盖）并返回 true。
 */
bool exportPlanToTxt(
    const std::string& path,    // 输入：文件路径
    const std::vector<SemesterPlan>& plan, // 输入：学期计划列表，使用 const 引用避免拷贝
    const std::unordered_map<std::string, Course>& course_map, // 输入：课程映射表，便于通过 ID 快速查找课程详情
    std::string& error_message    // 输出：错误信息字符串的引用，用于返回详细错误
);

#endif // COURSE_IO_H    // 头文件保护结束
