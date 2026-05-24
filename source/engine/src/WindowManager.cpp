#include "WindowManager.h"
#include <assert.h>

Loops::WindowManager::WindowManager(uint32_t screenWidth, uint32_t screenHeight) :
    m_screenWidth(screenWidth), m_screenHeight(screenHeight)
{
}

void Loops::WindowManager::Init()
{
    InitOSWindow();
}

void Loops::WindowManager::DeInit()
{
    DeInitOSWindow();
}

void Loops::WindowManager::Close()
{
    windowShouldRun = false;
}

bool Loops::WindowManager::Update()
{
    UpdateOSWindow();
    return windowShouldRun;
}

void Loops::WindowManager::InitOSWindow()
{
    glfwInit();
    if (glfwVulkanSupported() == GLFW_FALSE)
    {
        assert(0);
    }
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindow = glfwCreateWindow(m_screenWidth, m_screenHeight, "Vulkan", nullptr, nullptr);
    //glfwWindow = glfwCreateWindow(m_screenWidth, m_screenHeight, "Vulkan", glfwGetPrimaryMonitor(), nullptr);
    glfwGetFramebufferSize(glfwWindow, (int*)&m_screenWidth, (int*)&m_screenHeight);
}

void Loops::WindowManager::DeInitOSWindow()
{
    glfwDestroyWindow(glfwWindow);
    glfwTerminate();
}

void Loops::WindowManager::UpdateOSWindow()
{
    glfwPollEvents();

    if (glfwWindowShouldClose(glfwWindow))
    {
        Close();
    }
}

