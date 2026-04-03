project(AMBA_TLM)

# CHI相关源文件
file(GLOB_RECURSE CHI_SOURCE_FILES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/libarmchi.cpp)
# AXI相关源文件
file(GLOB_RECURSE AXI_SOURCE_FILES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/libarmaxi4.cpp)

# 创建CHI库
add_library(${PROJECT_NAME}_CHI SHARED ${CHI_SOURCE_FILES})
target_link_libraries(${PROJECT_NAME}_CHI PUBLIC SystemC::systemc)
target_include_directories(${PROJECT_NAME}_CHI
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
)
target_compile_definitions(${PROJECT_NAME}_CHI
    PUBLIC
        SC_INCLUDE_DYNAMIC_PROCESSES
)

# 创建AXI库
add_library(${PROJECT_NAME}_AXI SHARED ${AXI_SOURCE_FILES})
target_link_libraries(${PROJECT_NAME}_AXI PUBLIC SystemC::systemc)
target_include_directories(${PROJECT_NAME}_AXI
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
)
target_compile_definitions(${PROJECT_NAME}_AXI
    PUBLIC
        SC_INCLUDE_DYNAMIC_PROCESSES
)