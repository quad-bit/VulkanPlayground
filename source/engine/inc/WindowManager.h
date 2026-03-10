#pragma once

#if defined(GLFW_ENABLED)
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#endif

class WindowManager
{
private:
    void                                InitOSWindow();
    void                                DeInitOSWindow();
    void                                UpdateOSWindow();

    WindowManager() = delete;
    WindowManager(WindowManager const&) = delete;
    WindowManager const& operator= (WindowManager const&) = delete;

    uint32_t m_screenWidth, m_screenHeight;

public:
    ~WindowManager() {}
    WindowManager(const uint32_t& screenWidth, const uint32_t& screenHeight);
    void                                Init();
    void                                DeInit();
    void                                Close();
    bool                                Update();

    bool                                windowShouldRun = true;

#if defined(GLFW_ENABLED) 
    GLFWwindow* glfwWindow = nullptr;
#elif VK_USE_PLATFORM_WIN32_KHR
    HINSTANCE                            win32Instance = NULL;
    HWND                                win32Window = NULL;
    std::string                            win32ClassName;
    static uint64_t                        win32ClassIdCounter;
#elif VK_USE_PLATFORM_XCB_KHR
    xcb_connection_t* xcbConnection = nullptr;
    xcb_screen_t* xcbScreen = nullptr;
    xcb_window_t                        xcbWindow = 0;
    xcb_intern_atom_reply_t* xcbAtomWindowReply = nullptr;
#endif

};