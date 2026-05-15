# 检查是否作为独立项目构建
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
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
else()
    # 作为子项目时的配置保持不变
endif()

# 获取所有源文件
file(GLOB_RECURSE CHIPORT_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
)

# 创建CHIPort库
add_library(CHIPort STATIC ${CHIPORT_SOURCES})

# 链接SystemC和第三方库
target_link_libraries(CHIPort
    PUBLIC
        SystemC::systemc
        AMBA_TLM_CHI
        DMU_COMMON
)

# 设置包含目录
target_include_directories(CHIPort
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# 添加编译定义
target_compile_definitions(CHIPort
    PUBLIC
        SC_INCLUDE_DYNAMIC_PROCESSES
        ENABLE_DEBUG_PRINT
)

# 在独立项目模式或者开启了测试选项下添加可执行文件
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    # 添加CHITrafficExample.cpp作为可执行文件PortTest
    add_executable(PortTest ${CMAKE_CURRENT_SOURCE_DIR}/src/CHITrafficExample.cpp)

    # 链接到CHIPort库
    target_link_libraries(PortTest
        PRIVATE
            CHIPort
    )
    
endif()