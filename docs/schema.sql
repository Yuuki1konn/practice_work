-- 创建数据库 university_scheduler（如果不存在）
-- 该数据库用于存储学生、课程等核心信息，为排课系统提供数据支持。
CREATE DATABASE IF NOT EXISTS university_scheduler
    CHARACTER SET utf8mb4 -- 设置字符集为 utf8mb4，支持存储表情符号和所有 Unicode 字符
    COLLATE utf8mb4_unicode_ci; -- 设置排序规则为 utf8mb4_unicode_ci，基于 Unicode 标准进行字符串比较，适合多语言环境
-- 切换到刚创建（或已存在）的数据库，后续操作均在此数据库中进行
USE university_scheduler;

-- 1) 学生表
CREATE TABLE IF NOT EXISTS student(
    student_id VARCHAR(20) PRIMARY KEY, -- 学生学号，作为主键
    name VARCHAR(50) NOT NULL, -- 姓名
    major VARCHAR(100) NOT NULL, -- 专业
    grade INT NOT NULL, -- 年级
    create_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP-- 创建时间，默认值为当前时间戳
);

-- 2)课程表
CREATE TABLE IF NOT EXISTS course(
    course_id VARCHAR(20) PRIMARY KEY,-- 课程编号，作为主键
    course_name VARCHAR(100) NOT NULL, -- 课程名称
    credit INT NOT NULL CHECK (credit > 0), -- 课程学分，必须大于 0
    create_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP-- 创建时间，默认值为当前时间戳
);

-- 3) 学生-课程关系表
CREATE TABLE IF NOT EXISTS student_course(
    id BIGINT PRIMARY KEY AUTO_INCREMENT,-- 关系表主键，自动递增
    student_id VARCHAR(20) NOT NULL,-- 学生学号，引用学生表
    course_id VARCHAR(20) NOT NULL,-- 课程编号，引用课程表
    semester INT NOT NULL,-- 学期
    score DECIMAL(5,2) NULL,-- 成绩(总长5位，2位小数)， nullable 允许为空
    status VARCHAR(20) NOT NULL DEFAULT 'PLANNED',-- 状态，默认值为 'PLANNED'可选值包括规划、已选、完成、不及格、退课等
    create_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,-- 创建时间，默认值为当前时间戳
    -- 外键约束：关联 student 表，引用 student_id 字段
    CONSTRAINT fk_sc_student
      FOREIGN KEY (student_id) REFERENCES student(student_id)
      ON UPDATE CASCADE -- 当 student_id 更新时，自动更新本表（例如学号变更）
      ON DELETE RESTRICT,-- -- 阻止删除被引用的学生，需先解除选课关系

    -- 外键约束：关联 course 表，引用 course_id 字段
    CONSTRAINT fk_sc_course
      FOREIGN KEY (course_id) REFERENCES course(course_id)
      ON UPDATE CASCADE -- 课程编号更新时自动更新本表
      ON DELETE RESTRICT,-- 阻止删除被引用的课程

    -- 唯一约束：确保同一学生、同一课程、同一学期只能有一条记录
    -- 防止重复选课
    CONSTRAINT uq_student_course_semester
      UNIQUE (student_id, course_id , semester),

    -- 检查约束：学期必须大于 0（防止负数或零）
    CONSTRAINT chk_semester_positive CHECK (semester > 0),

    -- 检查约束：状态字段必须为指定的几个值之一
    CONSTRAINT chk_status
      CHECK (status IN ('PLANNED', 'ENROLLED', 'COMPLETED','FAILED','DROPPED'))
);

-- test data
INSERT INTO student (student_id, name, major, grade) VALUES
('S2026001', '张三', 'Computer Science', 2026),
('S2026002', '李四', 'Software Engineering', 2026);

INSERT INTO course (course_id, course_name, credit) VALUES
('C01', '高等数学', 4),
('C02', '程序设计', 3),
('C03', '数据结构', 4);

INSERT INTO student_course (student_id, course_id, semester, score, status) VALUES
('S2026001', 'C01', 1, 88.5, 'COMPLETED'),
('S2026001', 'C02', 1, 92.0, 'COMPLETED'),
('S2026001', 'C03', 2, NULL, 'PLANNED');
