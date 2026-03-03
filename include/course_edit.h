#ifndef COURSE_EDIT_H // 头文件保护宏，防止重复包含。
#define COURSE_EDIT_H

#include<string>
#include<vector>

#include "models.h"

/**
 * 列出所有课程的基本信息（如 ID、名称、学分）。
 * 该函数仅用于显示，不会修改课程列表。
 * @param courses 常量引用，待显示的课程列表。
 */
void listCourses(const std::vector<Course>& courses);

/**
 * 添加一门新课程到课程列表中。
 * @param courses 课程列表的引用，成功添加后会被修改。
 * @param c       要添加的课程对象（包含 ID、名称、学分、先修课程列表）。
 * @param err     输出参数，如果添加失败，此处会存储具体的错误描述。
 * @return        成功添加返回 true，失败返回 false。
 * 
 * 添加失败的可能原因：
 *   - 课程 ID 已存在（不允许重复 ID）
 *   - 先修课程列表中包含不存在的课程 ID
 *   - 学分 <= 0（根据 models.h 注释要求）
 *   - 其他业务规则（如课程名称为空等）
 */
bool addCourse(std::vector<Course>& courses,const Course& c, std::string& err);

/**
 * 更新一门已存在的课程信息。
 * @param courses 课程列表的引用，成功更新后会被修改。
 * @param updated 包含更新后信息的课程对象（必须包含有效 ID，其他字段为更新后的值）。
 * @param err     输出参数，如果更新失败，此处会存储具体的错误描述。
 * @return        成功更新返回 true，失败返回 false。
 * 
 * 更新失败的可能原因：
 *   - 要更新的课程 ID 不存在
 *   - 新的先修课程列表中出现不存在的课程 ID
 *   - 更新后学分 <= 0
 *   - 其他业务规则冲突
 * 
 * 注意：更新操作可能允许修改课程 ID？通常课程 ID 是唯一标识，不应允许修改。
 * 具体实现可能限制 ID 不可变，或采取先删除后添加的策略。
 */
bool updateCourse(std::vector<Course>& courses,const Course& updated, std::string& err);

/**
 * 从课程列表中删除一门课程。
 * @param courses 课程列表的引用，成功删除后会被修改。
 * @param id      要删除的课程 ID。
 * @param err     输出参数，如果删除失败，此处会存储具体的错误描述。
 * @return        成功删除返回 true，失败返回 false。
 * 
 * 删除失败的可能原因：
 *   - 课程 ID 不存在
 *   - 存在其他课程将该课程作为先修课程（即 hasDependent 返回 true），
 *     此时不允许删除，以免破坏依赖完整性。
 */
bool removeCourse(std::vector<Course>& courses,const std::string& id, std::string& err);

/**
 * 检查是否存在其他课程依赖于指定课程（即该课程被其他课程列为先修课程）。
 * 此函数用于删除前的依赖检查。
 * @param courses 常量引用，课程列表。
 * @param id      要检查的课程 ID。
 * @return        如果存在依赖返回 true，否则返回 false。
 */
bool hasDependent(const std::vector<Course>& courses, const std::string& id);

#endif