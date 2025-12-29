project(AMBA_TLM)

file(GLOB_RECURSE CHI_SOURCE_FILES CONFIGURE_DEPENDS src/libarmchi.cpp)
file(GLOB_RECURSE AXI_SOURCE_FILES CONFIGURE_DEPENDS src/libarmaxi4.cpp)
# set(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/lib)

# axi4 static library#
# add_library(amba_tlm_axi4 STATIC ${AXI_SOURCE_FILES})
# target_include_directories(amba_tlm_axi4 
#   PUBLIC 
#       ${PROJECT_SOURCE_DIR}/include>
# )
# axi4 dynamic library#
# add_library(amba_tlm_axi4_shared SHARED ${AXI_SOURCE_FILES})
# target_include_directories(amba_tlm_axi4_shared
#   PUBLIC 
#       ${PROJECT_SOURCE_DIR}/include>
# )
# set_target_properties(amba_tlm_axi4_shared PROPERTIES
#     POSITION_INDEPENDENT_CODE ON
#     OUTPUT_NAME "amba_tlm_axi4"
# )
# chi static library#
add_library(amba_tlm_chi STATIC ${CHI_SOURCE_FILES})
target_include_directories(amba_tlm_chi 
  PUBLIC 
      ${PROJECT_SOURCE_DIR}/include
)
target_link_libraries(amba_tlm_chi
  PUBLIC
      SystemC::SystemC
)
# chi dynamic library#
add_library(amba_tlm_chi_shared SHARED ${CHI_SOURCE_FILES})
target_link_libraries(amba_tlm_chi_shared
  PUBLIC
      SystemC::SystemC
)
target_include_directories(amba_tlm_chi_shared
  PUBLIC 
      ${PROJECT_SOURCE_DIR}/include
)
# set_target_properties(amba_tlm_chi_shared PROPERTIES
#     POSITION_INDEPENDENT_CODE ON
#     OUTPUT_NAME "amba_tlm_chi"
# )