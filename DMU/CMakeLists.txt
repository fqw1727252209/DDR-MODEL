project(DMU)
file(GLOB_RECURSE SOURCE_FILES CONFIGURE_DEPENDS *.cpp;)
file(GLOB_RECURSE HEADER_FILES CONFIGURE_DEPENDS *.h;*.hpp)
list(FILTER SOURCE_FILES EXCLUDE REGEX "main.cpp")
add_library(DMU STATIC ${SOURCE_FILES} ${HEADER_FILES})
target_link_libraries(DMU
    PUBLIC
        CHIPort
                stdc++fs
        DRAMSys::libdramsys
)
target_include_directories(DMU
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)


add_executable(dmutest main.cc)
# target_compile_options(dmutest PRIVATE "-fsanitize=address")
target_link_libraries(dmutest
    PRIVATE
        DMU

        # asan
)
target_include_directories(dmutest
    PRIVATE
        ${CHI_PORT_HOME}/include
        ${DRAMsys_HOME}/src

)
