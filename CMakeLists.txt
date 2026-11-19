cmake_minimum_required(VERSION 3.16)

project(
    InstructionScheduler
    VERSION 1.0
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(
    sched
    src/main.cpp
    src/utils.cpp
    src/parser.cpp
    src/dependency_graph.cpp
    src/scheduler.cpp
)

target_include_directories(
    sched
    PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)

target_compile_options(
    sched
    PRIVATE
    -Wall
    -Wextra
    -Wpedantic
)