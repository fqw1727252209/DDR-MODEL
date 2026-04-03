# 检查是否作为独立项目构建
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    cmake_minimum_required(VERSION 3.20)
    project(ControllerStandalone LANGUAGES CXX)

    # 设置C++标准和构建类型（强制缓存覆盖，防止SystemC改回C++11）
    set(CMAKE_CXX_STANDARD 17 CACHE STRING "C++ standard" FORCE)
    set(CMAKE_CXX_STANDARD_REQUIRED ON CACHE BOOL "C++ standard required" FORCE)
    set(CMAKE_CXX_EXTENSIONS OFF CACHE BOOL "Use compiler-specific extensions" FORCE)
    if(NOT CMAKE_BUILD_TYPE)
        set(CMAKE_BUILD_TYPE "Debug")
    endif()

    # 查找 third_party 目录
    get_filename_component(PARENT_DIR ${CMAKE_CURRENT_SOURCE_DIR} DIRECTORY)
    set(THIRD_PARTY_DIR ${PARENT_DIR}/third_party)
    set(COMMON_DIR ${PARENT_DIR}/Common)

    # 如果未找到，则尝试从环境变量获取
    if(NOT EXISTS ${THIRD_PARTY_DIR})
        if(DEFINED ENV{THIRD_PARTY_ROOT})
            set(THIRD_PARTY_DIR $ENV{THIRD_PARTY_ROOT})
        else()
            message(FATAL_ERROR "无法找到third_party目录，请设置THIRD_PARTY_ROOT环境变量")
        endif()
    endif()

    # 输出第三方库路径用于调试
    message(STATUS "Using third_party root: ${THIRD_PARTY_DIR}")

    # 包含必要的头文件目录
    # include_directories(
    #     ${CMAKE_CURRENT_SOURCE_DIR}/include
    #     ${THIRD_PARTY_DIR}/systemc-2.3.3/src
    #     ${THIRD_PARTY_DIR}/rapidjson/include
    # )
    set(SYSTEMC_INCLUDE_DIRS ${THIRD_PARTY_DIR}/systemc-2.3.3/src)
    set(RAPIDJSON_INCLUDE_DIRS ${THIRD_PARTY_DIR}/rapidjson/include)
    # 添加第三方库子项目
    add_subdirectory(${THIRD_PARTY_DIR}/systemc-2.3.3 systemc-2.3.3)

    add_subdirectory(${THIRD_PARTY_DIR}/rapidjson rapidjson)
    # 添加Common库
    add_subdirectory(${COMMON_DIR} Common)

else()

endif()

# 获取所有源文件
file(GLOB_RECURSE CONTROLLER_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/common/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/utils/*.cpp
)

# 创建 Controller 静态库（统一一次定义）
add_library(Controller STATIC ${CONTROLLER_SOURCES})

get_filename_component(PARENT_DIR ${CMAKE_CURRENT_SOURCE_DIR} DIRECTORY)
set(THIRD_PARTY_DIR ${PARENT_DIR}/third_party)
set(COMMON_DIR ${PARENT_DIR}/Common)

# 公共包含目录
target_include_directories(Controller
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        ${COMMON_DIR}/include
)

# 编译定义
target_compile_definitions(Controller
    PUBLIC
        DEBUG
        ENABLE_DEBUG_PRINT
)

# 链接依赖库
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    # 独立构建：链接通过 add_subdirectory 引入的库
    target_link_libraries(Controller PUBLIC SystemC::systemc RapidJSON DMU_COMMON)
else()
    # 子项目模式：链接已发现的系统库
    target_link_libraries(Controller PUBLIC SystemC::systemc RapidJSON)
    target_include_directories(Controller PRIVATE ${RAPIDJSON_INCLUDE_DIRS})
endif()

# 独立构建时可添加测试或示例程序
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    # 示例程序（用户可根据需要取消注释）
    # enable_language(CXX)
    set(TEST_DIR ${CMAKE_CURRENT_LIST_DIR}/test)
    add_executable(controller_test ${TEST_DIR}/mc_test.cpp)
    target_link_libraries(controller_test PRIVATE Controller)
endif()