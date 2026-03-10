include("${CMAKE_SOURCE_DIR}/cmake/Util.cmake")
GROUP_SOURCE_FILES(${CMAKE_CURRENT_SOURCE_DIR})

set(CORE_FILES
    inc/VulkanManager.h
    inc/Utils.h
    inc/WindowManager.h
    inc/ValidationManager.h
    inc/Common.h
    inc/Timer.h
    inc/ImguiUtil.h
    inc/ImguiGlfwHelper.h
    inc/GltfLoader.h
    inc/SceneNode.h
    inc/SceneManager.h
    inc/Mesh.h
    inc/ImguiEditor.h
    inc/TimelineSemaphore.h
    inc/WireFrameTask.h
    inc/Camera.h

    src/VulkanManager.cpp
    src/ValidationManager.cpp
    src/WindowManager.cpp
    src/Utils.cpp
    src/ImguiUtil.cpp
    src/GltfLoader.cpp
    src/SceneNode.cpp
    src/SceneManager.cpp
    src/ImguiEditor.cpp
    src/WireFrameTask.cpp
    src/Camera.cpp
)

#create library
set(TARGET_NAME "engine")
add_library(${TARGET_NAME} ${CORE_FILES})
add_library(VulkanPlayground::${TARGET_NAME} ALIAS ${TARGET_NAME})

target_link_libraries(${TARGET_NAME} 
    PRIVATE 
        Vulkan::Vulkan glfw  
    PUBLIC 
        glm::glm plog::plog imgui::imgui Taskflow::Taskflow flecs::flecs
)

target_include_directories(${TARGET_NAME}
    PUBLIC
        $<INSTALL_INTERFACE:inc>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/inc>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
    )

target_compile_definitions(${TARGET_NAME} PUBLIC
    ASSETS_PATH="${ASSETS_PATH}" GLFW_ENABLED
    SPV_PATH="${SPV_PATH}"
)

add_custom_target(
    generateShaderBinaries
    DEPENDS ${ASSETS_PATH}
)

add_custom_command(
    TARGET generateShaderBinaries
    COMMAND ${CMAKE_COMMAND} -E make_directory ${SPV_PATH}

    COMMAND glslangvalidator -V ${ASSETS_PATH}/shaders/UnlitColor.vert -o ${SPV_PATH}/UnlitColorVert.spv
    COMMAND glslangvalidator -V ${ASSETS_PATH}/shaders/UnlitColor.frag -o ${SPV_PATH}/UnlitColorFrag.spv

    COMMAND glslangvalidator -V ${ASSETS_PATH}/shaders/ClothSim.comp -o ${SPV_PATH}/ClothSim.spv
    COMMAND glslangvalidator -V ${ASSETS_PATH}/shaders/UnlitTextured.vert -o ${SPV_PATH}/UnlitTexturedVert.spv
    COMMAND glslangvalidator -V ${ASSETS_PATH}/shaders/UnlitTextured.frag -o ${SPV_PATH}/UnlitTexturedFrag.spv
)

add_dependencies(${TARGET_NAME} generateShaderBinaries)