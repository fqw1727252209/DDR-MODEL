cmake_minimum_required(VERSION 3.12)
project(CHIPortStandalone LANGUAGES CXX)

# 设置C++标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_BUILD_TYPE "Debug")

# 查找上级目录的third_party
get_filename_component(PARENT_DIR ${CMAKE_CURRENT_SOURCE_DIR} DIRECTORY)
set(THIRD_PARTY_DIR ${PARENT_DIR}/third_party)
set(COMMON_DIR ${PARENT_DIR}/Common)
set(PORT_DIR ${PARENT_DIR}/CHIPort)
set(CONTROLLER_DIR ${PARENT_DIR}/Controller)

# 如果third_party目录不存在，则尝试使用环境变量
if(NOT EXISTS ${THIRD_PARTY_DIR})
    if(DEFINED ENV{THIRD_PARTY_ROOT})
        set(THIRD_PARTY_DIR $ENV{THIRD_PARTY_ROOT})
    else()
        message(FATAL_ERROR "无法找到third_party目录,请设置THIRD_PARTY_ROOT环境变量")
    endif()
endif()

# 如果Common目录不存在，则尝试使用环境变量
if(NOT EXISTS ${COMMON_DIR})
    if(DEFINED ENV{COMMON_ROOT})
        set(THIRD_PARTY_DIR $ENV{COMMON_ROOT})
    else()
        message(FATAL_ERROR "无法找到Common目录,请设置COMMON_ROOT环境变量")
    endif()
endif()

# 包含third_party库
include_directories(
    # ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${THIRD_PARTY_DIR}/amba_tlm/include
    ${THIRD_PARTY_DIR}/systemc-2.3.3/src
    ${THIRD_PARTY_DIR}/rapidjson/include
    ${COMMON_DIR}/include
)

# 添加third_party子目录
add_subdirectory(${THIRD_PARTY_DIR}/systemc-2.3.3 systemc-2.3.3)
add_subdirectory(${THIRD_PARTY_DIR}/amba_tlm amba_tlm)
add_subdirectory(${THIRD_PARTY_DIR}/rapidjson rapidjson)
# 添加Common库
add_subdirectory(${COMMON_DIR} Common)
# 添加CHIPort库
add_subdirectory(${PORT_DIR} CHIPort)
# 添加Controller库
add_subdirectory(${CONTROLLER_DIR} Controller)

# 获取所有源文件
file(GLOB_RECURSE DMU_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
)

# 创建DMU库
add_library(DMU STATIC ${DMU_SOURCES})

# 链接SystemC和第三方库
target_link_libraries(DMU
    PUBLIC
        SystemC::systemc
        AMBA_TLM_CHI
        DMU_COMMON
        CHIPort
        Controller
)

# 设置包含目录
target_include_directories(DMU
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

add_executable(dmu_test ${CMAKE_CURRENT_SOURCE_DIR}/src/test_example.cpp)

target_link_libraries(dmu_test
    PUBLIC
        DMU
)

# 添加编译定义
target_compile_definitions(dmu_test
    PUBLIC
        SC_INCLUDE_DYNAMIC_PROCESSES
        # ENABLE_DEBUG_PRINT
)