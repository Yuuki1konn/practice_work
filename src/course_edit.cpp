#include "../include/course_edit.h" // 引入对应的头文件，声明了课程编辑函数

// 标准库头文件
#include <algorithm> // 用于 std::all_of
#include <cctype> // 用于 std::isspace, std::isalnum
#include <iostream>
#include <sstream>
#include <unordered_set> // 用于检查先修课程重复

namespace{

/**
 * 去除字符串首尾的空白字符。
 * @param s 输入字符串
 * @return 去除首尾空白后的新字符串
 */
std::string trim(const std::string& s){
    size_t l = 0;
    // 跳过开头的空白字符  static_cast<目标类型>(表达式) 将 s[l]（类型为 char）转换为 unsigned char 类型。
    //std::isspace 函数（以及 <cctype> 中的其他字符分类函数如 isalpha、isdigit 等）接受一个 int 类型的参数，
    //并期望这个 int 的值要么等于 EOF，要么在 unsigned char 的范围内（0～255）。
    //如果直接传递一个 char，而 char 在某些平台上是有符号的（范围可能是 -128～127），那么当字符的 ASCII 码大于 127（例如扩展 ASCII 字符）时，
    //作为有符号 char 传入会变成一个负数，这将导致 isspace 的行为未定义（因为负数既不是有效的 unsigned char 值，也不等于 EOF）。
    //通过先将 char 转换为 unsigned char，可以确保值在 0～255 范围内，从而安全地传递给 isspace。
    while(l < s.size() && std::isspace(static_cast<unsigned char>(s[l])))
        ++l;
    size_t r = s.size();
    // 跳过结尾的空白字符
    while(r > l && std::isspace(static_cast<unsigned char>(s[r - 1])))
        --r;
    // 返回中间非空白部分
    return s.substr(l, r - l);
}

/**
 * 检查课程 ID 是否合法：长度为 3 且全部是字母或数字。
 * @param id 课程 ID 字符串
 * @return 合法返回 true，否则 false
 */
bool isValidCourseId(const std::string& id){
    if(id.size() != 3) return false;
    // std::all_of 检查每个字符是否为字母数字
    return std::all_of(id.begin(),id.end(),[](unsigned char ch){
        //lambda 的作用是：对于每个字符 ch，判断它是否为字母或数字（std::isalnum(ch) != 0），
        //返回 true 或 false。
        return std::isalnum(ch) != 0;
    });
}

/**
 * 在课程列表中根据 ID 查找索引。
 * @param courses 课程列表
 * @param id      要查找的课程 ID
 * @return 如果找到，返回索引（转换为 int）；否则返回 -1
 */
int findIndexById(const std::vector<Course>& courses,const std::string& id){
    for(size_t i = 0; i < courses.size(); ++i){
        if(courses[i].id == id)
        return static_cast<int>(i); // 将无符号索引转换为 int 以便返回 -1 表示未找到
    }
    return -1;
}
} // namespace

Course normalizeCourse(const Course& in){
    Course out = in;// 1. 拷贝原始课程，作为基础
    out.id = trim(out.id);// 2. 去除课程 ID 首尾空白
    out.name = trim(out.name);// 3. 去除课程名称首尾空白

    std::vector<std::string> cleaned;// 存放清理后的先修 ID
    for(auto p : out.prereq_ids){ // 遍历每个先修 ID（注意 p 是副本，修改不影响原列表）
        p = trim(p);// 去除当前 ID 的首尾空白
        if(!p.empty())// 如果清理后不为空（避免出现空字符串）
            cleaned.push_back(p);// 加入结果列表
    }
    out.prereq_ids = cleaned;// 5. 替换原先修列表
    return out;
}
/**
 * 列出所有课程的信息。
 * @param courses 课程列表（常量引用）
 */
void listCourses(const std::vector<Course>& courses){
    if(courses.empty()){
        std::cout << "当前课程目录为空。\n";
        return;
    }
    // 打印表头
    std::cout <<"ID | 名称 | 学分 | 先修课程\n";
    for(const auto& c : courses){
        std::cout << c.id << " | " << c.name << " | " << c.credit << " | ";
        // 输出先修课程列表，以,分隔
        for (size_t i = 0; i < c.prereq_ids.size(); ++i){
            std::cout << c.prereq_ids[i];
            if (i + 1 < c.prereq_ids.size())
                std::cout << ",";
        }
        std::cout << "\n";
    }
}

/**
 * 添加一门新课程。
 * @param courses 课程列表（引用，会被修改）
 * @param c       要添加的课程对象
 * @param err     输出错误信息
 * @return 成功添加返回 true，否则 false
 */
bool addCourse(std::vector<Course>& courses, const Course& c, std::string& err){
    Course normalized = normalizeCourse(c);
    // 检查课程 ID 格式
    if(!isValidCourseId(normalized.id)){
        err = "课程ID格式错误，必须为3个字母数字。";
        return false;
    }
    // 检查课程名称是否为空
    if (normalized.name.empty()){
        err = "课程名称不能为空。";
        return false;
    }
    // 检查学分是否大于 0
    if (normalized.credit <= 0){
        err = "课程学分必须大于0。";
        return false;
    }
    // 检查课程 ID 是否已存在
    if(findIndexById(courses,normalized.id) != -1){
        err = "课程ID已存在：" + normalized.id;
        return false;
    }
    // 用于检查同一课程内先修 ID 是否重复
    std::unordered_set<std::string> seen;
    for(const auto& p : normalized.prereq_ids){
        if(!isValidCourseId(p)){
            // 检查先修 ID 格式
            err = "先修课程ID格式错误：" + p + "\nID格式应为3个字母数字。";
            return false;
        }
        // 检查是否将自己设为先修
        if(p == normalized.id){
            err = "课程ID不能作为自己的先修课程：" + normalized.id;
            return false;
        }
        // 检查先修 ID 在本课程列表内是否重复
        if(!seen.insert(p).second){
            //seen.insert(p) 尝试将 p 插入集合中。它的返回值是一个 std::pair<iterator, bool> 类型的对象。
            //如果插入成功（即 p 之前不在集合中），则返回值的 second 成员为 true。
            //如果插入失败（即 p 已经存在于集合中），则返回值的 second 成员为 false。
            //!seen.insert(p).second 的意思是：如果插入失败（即 p 已经存在于集合中），
            //说明这个先修课程 ID 在列表中出现了两次，是重复的，需要报错。
            err = "先修课程ID重复：" + p;
            return false;
        }
        // 检查先修课程是否已存在于整个课程列表中
        if(findIndexById(courses,p) == -1){
            err = "先修课程ID不存在：" + p;
            return false;
        }
    }

    // 所有检查通过，将课程添加到列表末尾
    courses.push_back(normalized);
    return true;
}

/**
 * 更新一门已存在的课程信息。
 * 
 * @param courses 课程列表的引用，成功更新后原列表会被修改。
 * @param updated 包含更新后信息的课程对象（必须包含有效 ID，其他字段为更新后的值）。
 * @param err     输出参数，如果更新失败，此处会存储具体的错误描述。
 * @return        成功更新返回 true，失败返回 false。
 * 
 * 更新逻辑：
 *   1. 检查要更新的课程 ID 是否存在，若不存在则失败。
 *   2. 检查更新后的课程名称、学分等字段是否合法（名称非空、学分 > 0）。
 *   3. 检查先修课程列表的合法性：每个先修 ID 格式正确、不自引用、不重复、且存在于当前课程列表中。
 *   4. 通过所有检查后，用 updated 替换原列表中的课程。
 */
bool updateCourse(std::vector<Course>& courses, const Course& updated, std::string& err){
    Course normalized = normalizeCourse(updated);
    // 1. 查找要更新的课程在列表中的索引
    int idx = findIndexById(courses,normalized.id);
    if(idx == -1){
        err = "课程ID不存在：" + normalized.id;
        return false;
    }
    // 2. 检查更新后的课程名称不能为空
    if(normalized.name.empty()){
        err = "课程名称不能为空。";
        return false;
    }
    // 3. 检查更新后的学分必须大于 0
    if(normalized.credit <= 0){
        err = "课程学分必须大于0。";
        return false;
    }

    // 4. 验证先修课程列表的合法性
    std::unordered_set<std::string> seen; // 用于检测同一课程内的先修 ID 是否重复
    for(const auto& p : normalized.prereq_ids){
        if(!isValidCourseId(p)){
            err = "先修课程ID格式错误：" + p + "\nID格式应为3个字母数字。";
            return false;
        }
        if(p == normalized.id){
            err = "课程ID不能作为自己的先修课程：" + normalized.id;
            return false;
        }
        if(!seen.insert(p).second){
            err = "先修课程ID重复：" + p;
            return false;
        }
        if(findIndexById(courses,p) == -1){
            err = "先修课程ID不存在：" + p;
            return false;
        }
    }

    // 5. 所有检查通过，用 normalized 覆盖原课程
    courses[idx] = normalized;
    return true;
}

/**
 * 检查是否存在其他课程依赖于指定课程。
 * 
 * 依赖关系定义为：其他课程的先修课程列表中包含了指定的课程 ID。
 * 此函数常用于删除课程前的安全检查：如果要删除的课程被其他课程作为先修，
 * 则不能直接删除，否则会破坏数据完整性。
 * 
 * @param courses 所有课程的列表（常量引用，不会被修改）
 * @param id      要检查的课程 ID
 * @return        如果存在至少一门其他课程将该课程作为先修，返回 true；
 *                否则返回 false。
 * 
 * 算法逻辑：
 *   1. 遍历 courses 中的每一门课程 c。
 *   2. 对每一门课程 c，遍历它的先修课程列表 prereq_ids。
 *   3. 如果发现某个先修课程 ID 等于指定的 id，则立即返回 true。
 *   4. 如果遍历完所有课程都没有找到匹配项，则返回 false。
 * 
 * 时间复杂度：O(N * M)，其中 N 是课程总数，M 是平均每门课程的先修数量。
 * 对于小型课程目录（如几百门课），这种简单遍历是完全可以接受的。
 */
bool hasDependent(const std::vector<Course>& courses, const std::string& id){
    // 外层循环：遍历所有课程
    for(const auto& c : courses){
        // 内层循环：遍历当前课程的先修课程列表
        for(const auto& p : c.prereq_ids){
            // 如果某个先修课程 ID 等于要检查的 id
            if(p == id){
                // 说明存在依赖，立即返回 true
                return true;
            }
        }
    }
    // 所有课程检查完毕，未发现依赖，返回 false
    return false;
}

/**
 * 从课程列表中删除指定 ID 的课程。
 * 
 * 删除前会进行两项安全检查：
 *   1. 课程 ID 必须存在。
 *   2. 不能有其他课程依赖该课程（即该课程不能出现在任何其他课程的先修列表中）。
 * 
 * @param courses 课程列表的引用，成功删除后原列表会被修改。
 * @param id      要删除的课程 ID。
 * @param err     输出参数，如果删除失败，此处会存储具体的错误描述。
 * @return        成功删除返回 true，失败返回 false。
 * 
 * 删除步骤：
 *   1. 调用 findIndexById 查找要删除课程的索引。
 *      - 如果返回 -1，说明 ID 不存在，设置错误信息并返回 false。
 *   2. 调用 hasDependent 检查是否有其他课程依赖本课程。
 *      - 如果返回 true，说明存在依赖，不能删除，设置错误信息并返回 false。
 *   3. 通过所有检查后，使用 vector::erase 移除该课程（通过索引构造迭代器）。
 *   4. 返回 true 表示删除成功。
 * 
 * 注意：删除操作会使之后的所有迭代器、引用和指针失效，但函数结束即返回，不影响。
 */
bool removeCourse(std::vector<Course>& courses, const std::string& id, std::string& err){
    std::string target = trim(id);
    // 1. 查找课程是否存在
    int idx = findIndexById(courses , target);
    if(idx == -1){
        err = "课程ID不存在：" + target;
        return false;
    }
    // 2. 检查是否有其他课程依赖本课程
    if(hasDependent(courses,target)){
        err = "课程ID存在依赖关系，不能删除：" + target;
        return false;
    }
    // 3. 所有检查通过，从列表中移除该课程
    //courses.begin() + idx 得到指向要删除元素的迭代器
    courses.erase(courses.begin() + idx);
    return true;
}