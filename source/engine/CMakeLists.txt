include("${CMAKE_SOURCE_DIR}/cmake/Util.cmake")
GROUP_SOURCE_FILES(${CMAKE_CURRENT_SOURCE_DIR})

set(CORE_FILES
    inc/VulkanManager.h
    inc/Utils.h
    inc/WindowManager.h
    inc/ValidationManager.h
    inc/Timer.h
    inc/ImguiUtil.h
    inc/ImguiGlfwHelper.h
    inc/GltfLoader.h
    inc/SceneManager.h
    inc/ImguiEditor.h
    inc/TimelineSemaphore.h
    inc/Camera.h
    inc/Components.h
    inc/RenderData.h
    inc/Defines.h
    inc/BoundsManager.h
    inc/VulkanWrappers.h
    inc/EngineManager.h
    inc/Assertion.h
    inc/EventBus.h
    inc/Event.h

    inc/tasks/WireFrameTask.h
    inc/tasks/ColorUnlitTask.h
    inc/tasks/BoundsRenderTask.h
    inc/tasks/Task.h
    inc/pipelines/Pipeline.h
    inc/pipelines/WireframePipeline.h

    inc/input/InputManager.h
    inc/input/MouseInputManager.h

    inc/memory/MemoryManager.h
    inc/memory/MemoryUtils.h
    inc/memory/LinearPoolAllocator.h
    inc/memory/StackPoolAllocator.h

    src/VulkanManager.cpp
    src/ValidationManager.cpp
    src/WindowManager.cpp
    src/Utils.cpp
    src/ImguiUtil.cpp
    src/GltfLoader.cpp
    src/SceneManager.cpp
    src/ImguiEditor.cpp
    src/Camera.cpp
    src/BoundsManager.cpp
    src/EngineManager.cpp
    src/Assertion.cpp
    src/EventBus.cpp

    src/input/InputManager.cpp
    src/input/InputManagerGLFW.cpp

    src/tasks/WireFrameTask.cpp
    src/tasks/ColorUnlitTask.cpp
    src/tasks/BoundsRenderTask.cpp
    src/tasks/Task.cpp
    src/pipelines/Pipeline.cpp
    src/pipelines/WireframePipeline.cpp

    src/memory/MemoryManager.cpp
    src/memory/LinearPoolAllocator.cpp
    src/memory/StackPoolAllocator.cpp
)

#create library
set(TARGET_NAME "engine")
add_library(${TARGET_NAME} ${CORE_FILES})
add_library(VulkanPlayground::${TARGET_NAME} ALIAS ${TARGET_NAME})

target_link_libraries(${TARGET_NAME} 
    PRIVATE 
        Vulkan::Vulkan glfw  
    PUBLIC 
        glm::glm plog::plog imgui::imgui Taskflow::Taskflow flecs::flecs rapidjson GPUOpen::VulkanMemoryAllocator
)

target_include_directories(${TARGET_NAME}
    PUBLIC
        $<INSTALL_INTERFACE:inc>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/inc>
        # $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/inc/memory>
        # $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/inc/pipelines>
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

    # COMMAND glslangvalidator -V -I${CMAKE_CURRENT_SOURCE_DIR}/inc ${ASSETS_PATH}/shaders/UnlitColor.vert -o ${SPV_PATH}/UnlitColorVert.spv
    COMMAND glslangvalidator -V ${ASSETS_PATH}/shaders/UnlitColor.vert -o ${SPV_PATH}/UnlitColorVert.spv
    COMMAND glslangvalidator -V ${ASSETS_PATH}/shaders/UnlitColor.frag -o ${SPV_PATH}/UnlitColorFrag.spv
    COMMAND glslangvalidator -V ${ASSETS_PATH}/shaders/Bounds.vert -o ${SPV_PATH}/BoundsVert.spv
    COMMAND glslangvalidator -V ${ASSETS_PATH}/shaders/Bounds.frag -o ${SPV_PATH}/BoundsFrag.spv
    COMMAND glslangvalidator -V ${ASSETS_PATH}/shaders/ClothSim.comp -o ${SPV_PATH}/ClothSim.spv
    COMMAND glslangvalidator -V ${ASSETS_PATH}/shaders/UnlitTextured.vert -o ${SPV_PATH}/UnlitTexturedVert.spv
    COMMAND glslangvalidator -V ${ASSETS_PATH}/shaders/UnlitTextured.frag -o ${SPV_PATH}/UnlitTexturedFrag.spv
)

add_dependencies(${TARGET_NAME} generateShaderBinaries)