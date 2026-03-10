include("${CMAKE_SOURCE_DIR}/cmake/Util.cmake")
GROUP_SOURCE_FILES(${CMAKE_CURRENT_SOURCE_DIR})

set(SourceList
src/main.cpp
)

#create executable
set(TARGET_NAME "gltf")
add_Executable(${TARGET_NAME} ${SourceList})
set_target_properties(${TARGET_NAME} PROPERTIES FOLDER "applications")

target_link_libraries(${TARGET_NAME} PRIVATE VulkanPlayground::engine)
target_include_directories(${TARGET_NAME}
    PUBLIC
        $<INSTALL_INTERFACE:inc>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/inc>
        $<BUILD_INTERFACE:${plog_INCLUDE_DIR}>
)
