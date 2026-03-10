#include "WindowManager.h"
#include <assert.h>

WindowManager::WindowManager(const uint32_t& screenWidth, const uint32_t& screenHeight) :
    m_screenWidth(screenWidth), m_screenHeight(screenHeight)
{
}

void WindowManager::Init()
{
    InitOSWindow();
}

void WindowManager::DeInit()
{
    DeInitOSWindow();
}

void WindowManager::Close()
{
    windowShouldRun = false;
}

bool WindowManager::Update()
{
    UpdateOSWindow();
    return windowShouldRun;
}

void WindowManager::InitOSWindow()
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

void WindowManager::DeInitOSWindow()
{
    glfwDestroyWindow(glfwWindow);
    glfwTerminate();
}

void WindowManager::UpdateOSWindow()
{
    glfwPollEvents();

    if (glfwWindowShouldClose(glfwWindow))
    {
        Close();
    }
}

