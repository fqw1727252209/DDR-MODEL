cmake_minimum_required(VERSION 3.24.0)
project(DDR_TLM)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)


set(CMAKE_BUILD_TYPE "Debug")
set(CMAKE_EXPORT_COMPILE_COMMANDS True)
message("CMAKE_BINARY_DIR: ${CMAKE_BINARY_DIR}")

include(FetchContent)


set(Third_LIBRARY_HOME ${CMAKE_CURRENT_SOURCE_DIR}/lib)
set(AMBA_TLM_HOME ${Third_LIBRARY_HOME}/amba_tlm)
set(DRAMsys_HOME ${Third_LIBRARY_HOME}/DRAMsys)
set(TEST_HOME ${CMAKE_CURRENT_SOURCE_DIR}/test)
### complie output option ###
set(EXECUTABLE_OUTPUT_PATH ${CMAKE_CURRENT_SOURCE_DIR}/build/bin)

if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
endif()

### Thread ###
find_package(Threads REQUIRED)

### SystemC ###
# 设置SYSTEMC_HOME（可通过环境变量覆盖）
if(DEFINED ENV{SYSTEMC_HOME})
    set(SYSTEMC_HOME $ENV{SYSTEMC_HOME})
else()
    set(SYSTEMC_HOME "/opt/systemc-2")  # 默认路径
endif()

set(DISABLE_COPYRIGHT_MESSAGE True)

# 检查本地SystemC是否存在
if(EXISTS "${SYSTEMC_HOME}/include/systemc.h" AND EXISTS "${SYSTEMC_HOME}/lib/libsystemc.so")
    message(STATUS "Found local SystemC at: ${SYSTEMC_HOME}")
    
    # 为传统安装的SystemC创建IMPORTED target (使用动态库)
    if(NOT TARGET SystemC::systemc)
        add_library(SystemC::systemc SHARED IMPORTED GLOBAL)
        set_target_properties(SystemC::systemc PROPERTIES
            IMPORTED_LOCATION "${SYSTEMC_HOME}/lib/libsystemc.so"
            INTERFACE_INCLUDE_DIRECTORIES "${SYSTEMC_HOME}/include"
            INTERFACE_COMPILE_DEFINITIONS "SC_INCLUDE_DYNAMIC_PROCESSES"
            INTERFACE_LINK_LIBRARIES "Threads::Threads"
        )
    endif()
    # 创建SystemC::SystemC target（一些库使用这个名称）
    if(NOT TARGET SystemC::SystemC)
        add_library(SystemC::SystemC SHARED IMPORTED GLOBAL)
        set_target_properties(SystemC::SystemC PROPERTIES
            IMPORTED_LOCATION "${SYSTEMC_HOME}/lib/libsystemc.so"
            INTERFACE_INCLUDE_DIRECTORIES "${SYSTEMC_HOME}/include"
            INTERFACE_COMPILE_DEFINITIONS "SC_INCLUDE_DYNAMIC_PROCESSES"
            INTERFACE_LINK_LIBRARIES "Threads::Threads"
        )
    endif()
else()
    # 如果本地没有找到，尝试从gitee下载
    message(STATUS "Local SystemC not found at ${SYSTEMC_HOME}, fetching from gitee...")
    FetchContent_Declare(
        systemc
        GIT_REPOSITORY https://gitee.com/mirrors/systemc.git
        GIT_TAG 2.3.4
    )
    FetchContent_MakeAvailable(systemc)
endif()

### amba tlm ###
add_subdirectory(${Third_LIBRARY_HOME}/amba_tlm)
### DRAMsys ###
add_subdirectory(${Third_LIBRARY_HOME}/DRAMsys)
### CHI Port ###
set(CHI_PORT_HOME ${CMAKE_CURRENT_SOURCE_DIR}/CHIPort)
add_subdirectory(${CHI_PORT_HOME})
### DMU ###
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/DMU)
### Controller ###
# add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/Controller)

### test ###
# add_subdirectory(${TEST_HOME}/DDR_test)

### complie option ###
# set(SYSTEMC_FLAGS -pthread -fpermissive -DSC_INCLUDE_DYNAMIC_PROCESSES)
# set(ALL_FLAGS -g ${SYSTEMC_FLAGS})


