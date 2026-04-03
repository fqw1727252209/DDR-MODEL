project(DMU_COMMON)

set(COMMON_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/include)

# 相关源文件
file(GLOB_RECURSE COMMON_SOURCE_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Common/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Configure/*.cpp
)


# 查找 third_party 目录
get_filename_component(PARENT_DIR ${CMAKE_CURRENT_SOURCE_DIR} DIRECTORY)
set(THIRD_PARTY_DIR ${PARENT_DIR}/third_party)

# 如果未找到，则尝试从环境变量获取
if(NOT EXISTS ${THIRD_PARTY_DIR})
    if(DEFINED ENV{THIRD_PARTY_ROOT})
        set(THIRD_PARTY_DIR $ENV{THIRD_PARTY_ROOT})
    else()
        message(FATAL_ERROR "无法找到third_party目录，请设置THIRD_PARTY_ROOT环境变量")
    endif()
endif()

set(SYSTEMC_INCLUDE_DIRS ${THIRD_PARTY_DIR}/systemc-2.3.3/src)
set(RAPIDJSON_INCLUDE_DIRS ${THIRD_PARTY_DIR}/rapidjson/include)
# 添加第三方库子项目
# add_subdirectory(${THIRD_PARTY_DIR}/systemc-2.3.3 sysytemc-2.3.3)
# add_subdirectory(${THIRD_PARTY_DIR}/rapidjson rapidjson)


# 创建DMU_COMMON_LIBRARY
add_library(${PROJECT_NAME} SHARED ${COMMON_SOURCE_FILES})
target_link_libraries(${PROJECT_NAME} PUBLIC
    SystemC::systemc
    RapidJSON
)
target_include_directories(${PROJECT_NAME}
    PUBLIC
        ${SYSTEMC_INCLUDE_DIRS}
        ${RAPIDJSON_INCLUDE_DIRS}
        ${COMMON_INCLUDE_DIRS}
)
target_compile_definitions(${PROJECT_NAME}
    PUBLIC
        SC_INCLUDE_DYNAMIC_PROCESSES
)