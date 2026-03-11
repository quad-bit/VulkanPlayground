#include "VulkanManager.h"
#include "WireFrameTask.h"
#include "Timer.h"

#include "ImguiUtil.h"
#include "ImguiEditor.h"

#include <taskflow/taskflow.hpp>
#include "GltfLoader.h"
#include "SceneManager.h"

#include <plog/Initializers/RollingFileInitializer.h>
#include <plog/Formatters/TxtFormatter.h>
#include <plog/Appenders/ColorConsoleAppender.h>


#include <optional>
#include <memory>



#define TIMELINE_STAGE_OVERRIDE
enum TimelineStages
{
    UNINITIALIZED = 0,
    WIREFRAME_FINISHED = 1,
    GUI_FINISHED = 2,
    SAFE_TO_PRESENT = 3,
    NUM_STAGES = 4
};

#include "TimelineSemaphore.h"


int main()
{
    static plog::ColorConsoleAppender<plog::TxtFormatter> consoleAppender;
    plog::init(plog::debug, &consoleAppender);

    //tf::Executor executor;
    //tf::Taskflow taskflow;

    //auto [A, B, C, D] = taskflow.emplace(  // create four tasks
    //    []() { std::cout << "TaskA\n"; },
    //    []() { std::cout << "TaskB\n"; },
    //    []() { std::cout << "TaskC\n"; },
    //    []() { std::cout << "TaskD\n"; }
    //);

    //A.precede(B, C);  // A runs before B and C
    //D.succeed(B, C);  // D runs after  B and C

    //executor.run(taskflow).wait();

    auto path = std::string{ ASSETS_PATH } + "/models/ABeautifulGame/glTF/ABeautifulGame.gltf";
    //auto path = std::string{ ASSETS_PATH } + "models/Suzanne/Suzanne.gltf";
    Common::SceneManager sceneManager{path};

    constexpr uint32_t screenWidth = 1024;
    constexpr uint32_t screenHeight = 1024;

    Timer timer(60);

    std::unique_ptr<WindowManager> windowManagerObj = std::make_unique<WindowManager>(screenWidth, screenHeight);
    windowManagerObj->Init();

    std::unique_ptr<VulkanManager> vulkanManager = std::make_unique<VulkanManager>(screenWidth, screenHeight);
    vulkanManager->Init(windowManagerObj->glfwWindow);

    //sceneManager.Initialise(vulkanManager->GetLogicalDevice(), vulkanManager->GetPhysicalDevice(), vulkanManager->GetGraphicsQueue(),
    //    vulkanManager->GetQueueFamilyIndex());

    uint32_t maxFramesInFlight = vulkanManager->GetMaxFramesInFlight();

    std::vector<VkSemaphore> swapchainImageAcquiredSemaphores;
    for (uint32_t i = 0; i < maxFramesInFlight; i++)
    {
        VkSemaphoreCreateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        VkSemaphore semaphore;
        ErrorCheck(vkCreateSemaphore(vulkanManager->GetLogicalDevice(), &info, nullptr, &semaphore));
        swapchainImageAcquiredSemaphores.push_back(semaphore);
    }

    std::vector<std::unique_ptr<TimelineSemaphore>> timelineSemaphores;
    {
        timelineSemaphores.emplace_back(std::make_unique<TimelineSemaphore>(vulkanManager->GetLogicalDevice()));
        timelineSemaphores.emplace_back(std::make_unique<TimelineSemaphore>(vulkanManager->GetLogicalDevice()));
    }

    std::unique_ptr<Common::WireFrameTask> pWireframeTask = std::make_unique<Common::WireFrameTask>(
        vulkanManager->GetLogicalDevice(), vulkanManager->GetPhysicalDevice(), vulkanManager->GetGraphicsQueue(),
        vulkanManager->GetQueueFamilyIndex(), vulkanManager->GetMaxFramesInFlight(), screenWidth, screenHeight,
        vulkanManager->GetDepthFormat(), vulkanManager->GetDefaultColorImageView(), vulkanManager->GetDefaultDepthImageView());

    // imgui
    std::unique_ptr<Common::ImguiUtil > imguiUtil = std::make_unique<Common::ImguiUtil >(
        windowManagerObj->glfwWindow, vulkanManager->GetLogicalDevice(), vulkanManager->GetPhysicalDevice(),
        vulkanManager->GetGraphicsQueue(), vulkanManager->GetQueueFamilyIndex(), vulkanManager->GetMaxFramesInFlight(),
        screenWidth, screenHeight, vulkanManager->GetDepthFormat(), VK_FORMAT_B8G8R8A8_UNORM, vulkanManager->GetDefaultColorImageView());
    imguiUtil->Init();

    Common::ImguiEditor editorObj(*imguiUtil.get(), sceneManager);

    uint64_t frameIndex = 0;

    //timer.Sleep(10);

    while (windowManagerObj->Update())
    {
        timer.StartFrame();

        auto currentFrameInFlight = vulkanManager->GetFrameInFlightIndex();

        imguiUtil->NewFrame();

        if (timelineSemaphores[currentFrameInFlight]->GetFrameIndex() > 0)
        {
            // wait for previous frame's (corresponding frameInFlight) presentation to complete
            // this acts as a fence

            uint64_t value = (timelineSemaphores[currentFrameInFlight]->GetFrameIndex() - 1) * (TimelineStages::NUM_STAGES - 1) + TimelineStages::SAFE_TO_PRESENT;

            VkSemaphoreWaitInfo waitInfo{};
            waitInfo.pSemaphores = &timelineSemaphores[currentFrameInFlight]->GetSemaphore();
            waitInfo.pValues = &value;
            waitInfo.semaphoreCount = 1;
            waitInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO;

            ErrorCheck(vkWaitSemaphores(vulkanManager->GetLogicalDevice(), &waitInfo, UINT64_MAX));
        }

        // Trigger graphics tasks
        {
            uint64_t signalValue = timelineSemaphores[currentFrameInFlight]->GetTimelineValue(TimelineStages::WIREFRAME_FINISHED);
            pWireframeTask->Update(frameIndex, currentFrameInFlight, timelineSemaphores[currentFrameInFlight]->GetSemaphore(), signalValue, std::nullopt);
        }

        // Trigger imgui
        {
            uint64_t signalValue = timelineSemaphores[currentFrameInFlight]->GetTimelineValue(TimelineStages::GUI_FINISHED);
            uint64_t waitValue = timelineSemaphores[currentFrameInFlight]->GetTimelineValue(TimelineStages::UNINITIALIZED);
            imguiUtil->Render(currentFrameInFlight, timelineSemaphores[currentFrameInFlight]->GetSemaphore(), signalValue, waitValue);
        }

        // Get the active swapchain index
        uint32_t activeSwapchainImageindex = vulkanManager->GetActiveSwapchainImageIndex(swapchainImageAcquiredSemaphores[currentFrameInFlight]);

        // End the frame (increments index counters)
        {
            uint64_t waitValue = timelineSemaphores[currentFrameInFlight]->GetTimelineValue(TimelineStages::GUI_FINISHED);
            uint64_t signalValue = timelineSemaphores[currentFrameInFlight]->GetTimelineValue(TimelineStages::SAFE_TO_PRESENT);
            vulkanManager->CopyAndPresent(vulkanManager->GetDefaultColorImages()[currentFrameInFlight], timelineSemaphores[currentFrameInFlight]->GetSemaphore(),
                swapchainImageAcquiredSemaphores[currentFrameInFlight], waitValue, signalValue);
        }

        frameIndex++;
        timelineSemaphores[currentFrameInFlight]->IncrementFrameIndex();

        timer.EndFrame();

    }

    if (vulkanManager->AreTheQueuesIdle())
    {
        //vkDestroySemaphore(vulkanManager->GetLogicalDevice(), timelineSemaphore, nullptr);

        for(auto& sem : swapchainImageAcquiredSemaphores)
            vkDestroySemaphore(vulkanManager->GetLogicalDevice(), sem, nullptr);

        for (auto& sem : timelineSemaphores)
            sem.reset();
        timelineSemaphores.clear();

        pWireframeTask.reset();
        pWireframeTask = nullptr;

        sceneManager.DeInitialise();

        imguiUtil->Cleanup();
        imguiUtil.reset();
        imguiUtil = nullptr;
    }

    vulkanManager->DeInit();
    windowManagerObj->DeInit();

    return 0;
}