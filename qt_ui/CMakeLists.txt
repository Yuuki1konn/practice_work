cmake_minimum_required(VERSION 3.16)
project(SchedulerQt LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets)
qt_standard_project_setup()

add_executable(scheduler_qt
    main.cpp
    AppSession.h
    LoginDialog.h
    LoginDialog.cpp
    MainWindow.h
    MainWindow.cpp
    StudentPage.h
    StudentPage.cpp
    StudentEditDialog.h
    StudentEditDialog.cpp
    StudentCoursePage.h
    StudentCoursePage.cpp
    CoursePage.h
    CoursePage.cpp
    CourseEditDialog.h
    CourseEditDialog.cpp
    PlanPage.h
    PlanPage.cpp
    UiMessages.h
)

target_include_directories(scheduler_qt PRIVATE
    ../include
)

target_sources(scheduler_qt PRIVATE
    ../src/db_odbc.cpp
    ../src/course_io.cpp
    ../src/scheduler.cpp
)

target_link_libraries(scheduler_qt PRIVATE
    Qt6::Widgets
    odbc32
)
