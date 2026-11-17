# 回退方案：源码拉取 OR-Tools。
# 生产/CI 建议优先用预编译包 + find_package(ortools CONFIG REQUIRED)，
# 见 .github/workflows/ci.yml。
include(FetchContent)

set(BUILD_DEPS ON)
# FetchContent 场景下 SCIP 会因依赖问题导致配置失败，必须关掉。
set(USE_SCIP OFF CACHE BOOL "" FORCE)
set(USE_HIGHS OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    or-tools
    GIT_REPOSITORY https://github.com/google/or-tools.git
    GIT_TAG        v9.15          # 固定版本，保证可复现；尝鲜可换 stable / main
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(or-tools)
