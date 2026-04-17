#include "WindowManager.h"
#include <assert.h>

Common::WindowManager::WindowManager(uint32_t screenWidth, uint32_t screenHeight) :
    m_screenWidth(screenWidth), m_screenHeight(screenHeight)
{
}

void Common::WindowManager::Init()
{
    InitOSWindow();
}

void Common::WindowManager::DeInit()
{
    DeInitOSWindow();
}

void Common::WindowManager::Close()
{
    windowShouldRun = false;
}

bool Common::WindowManager::Update()
{
    UpdateOSWindow();
    return windowShouldRun;
}

void Common::WindowManager::InitOSWindow()
{
    glfwInit();
    if (glfwVulkanSupported() == GLFW_FALSE)
    {
        assert(0);
    }
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindow = glfwCreateWindow(m_screenWidth, m_screenHeight, "Vulkan", nullptr, nullptr);
    glfwGetFramebufferSize(glfwWindow, (int*)&m_screenWidth, (int*)&m_screenHeight);
}

void Common::WindowManager::DeInitOSWindow()
{
    glfwDestroyWindow(glfwWindow);
    glfwTerminate();
}

void Common::WindowManager::UpdateOSWindow()
{
    glfwPollEvents();

    if (glfwWindowShouldClose(glfwWindow))
    {
        Close();
    }
}

