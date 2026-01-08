cmake_minimum_required(VERSION 3.24.0)
project(CHITrafficExample)

file(GLOB_RECURSE SOURCE_FILES CONFIGURE_DEPENDS *.cpp)
file(GLOB_RECURSE HEADER_FILES CONFIGURE_DEPENDS include/*.h;*.hpp)
message(STATUS "source files: ${SOURCE_FILES}")
message(STATUS "head files: ${HEADER_FILES}")

list(FILTER SOURCE_FILES EXCLUDE REGEX "CHITrafficExample.cpp")
set(EXEFILE CHITrafficExample.cpp)

add_library(CHIPort STATIC ${SOURCE_FILES} ${HEADER_FILES})
target_link_libraries(CHIPort
    PUBLIC
        Threads::Threads
        SystemC::systemc
        amba_tlm_chi
)
target_include_directories(CHIPort 
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)
# target_compile_options(CHIPort PRIVATE "-DCHIPort_TEST")
