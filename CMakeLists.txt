cmake_minimum_required(VERSION 3.18)
project(ortools_rl_sched_env VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# 优先使用系统 / 预编译包安装的 OR-Tools（CI 里走这条，构建快）。
# 找不到时才回退到 FetchContent 从源码拉取并编译（本地开发用）。
option(ORTOOLS_REQUIRE_PREBUILT "找不到预编译 OR-Tools 时直接报错，不拉源码" OFF)
if(ORTOOLS_REQUIRE_PREBUILT)
  find_package(ortools CONFIG REQUIRED)
else()
  find_package(ortools CONFIG QUIET)
  if(NOT ortools_FOUND)
    include(cmake/FetchOR-Tools.cmake)
  endif()
endif()

# --- 环境库 ---
add_library(sched_env STATIC
    src/env/scheduling_env.cpp
)
target_include_directories(sched_env PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(sched_env PUBLIC ortools::ortools)

# --- 演示可执行文件 ---
add_executable(rl_demo src/main.cpp)
target_link_libraries(rl_demo PRIVATE sched_env)

# --- 测试 ---
enable_testing()
add_executable(test_env tests/test_env.cpp)
target_link_libraries(test_env PRIVATE sched_env)
add_test(NAME env_basic COMMAND test_env)
