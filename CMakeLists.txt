cmake_minimum_required(VERSION 3.12)

project(DDR_TLM LANGUAGES CXX)

# 设置C++标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_BUILD_TYPE "Debug")
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# 禁用SystemC强制的C++标准和宏检测检查以防止链接阶段的ABI不匹配报错
add_compile_definitions(SC_DISABLE_API_VERSION_CHECK)

# 添加构建测试的选项
option(BUILD_TEST "Build test files" ON)

# 设置third_party路径
set(THIRD_PARTY_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party)

# 不再查找外部SystemC，而是使用third_party中的SystemC
# list(APPEND CMAKE_PREFIX_PATH $ENV{SYSTEMC_HOME})
# find_package(SystemCLanguage REQUIRED)

# 包含目录 - 添加third_party库的包含路径
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/CHIPort/include
    ${CMAKE_CURRENT_SOURCE_DIR}/Controller/include
    ${CMAKE_CURRENT_SOURCE_DIR}/Common/include
    ${CMAKE_CURRENT_SOURCE_DIR}/DMU/include
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/amba_tlm/include
    # ${CMAKE_CURRENT_SOURCE_DIR}/utils
    ${THIRD_PARTY_DIR}/systemc/include
    ${THIRD_PARTY_DIR}/rapidjson/include
)

# 添加子目录 - 包含third_party库
add_subdirectory(third_party/systemc-2.3.3)
add_subdirectory(third_party/rapidjson)
add_subdirectory(third_party/amba_tlm)
add_subdirectory(Common)
add_subdirectory(CHIPort)
add_subdirectory(Controller)
add_subdirectory(DMU)

# 只有当BUILD_TEST选项开启时才构建测试可执行文件
if(BUILD_TEST)
    # add_executable(ddr_chi_test
    #     CHIPort/test/CHIPortTest.cpp
    #     # 可以添加更多测试文件
    # )

    # target_link_libraries(ddr_chi_test
    #     PRIVATE
    #         CHIPort
    #         Controller
    #         SystemC::systemc
    #         AMBA_TLM_CHI
    # )

    # set_target_properties(ddr_chi_test PROPERTIES
    #     RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    # )
endif()