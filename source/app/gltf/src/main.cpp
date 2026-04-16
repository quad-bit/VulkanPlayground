//#include <taskflow/taskflow.hpp>
#include "EngineManager.h"

#include "Defines.h"
#include <optional>
#include <memory>

int main()
{
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

    auto chessPath = std::string{ ASSETS_PATH } + "/models/ABeautifulGame/glTF/ABeautifulGame.gltf";
    auto suzzanePath = std::string{ ASSETS_PATH } + "/models/Suzanne/Suzanne.gltf";

    std::vector<Common::ModelLoadInfo> gltfInfo;
    gltfInfo.push_back({ 100.0f, chessPath.c_str() });
//    gltfInfo.push_back({ 1.0f, suzzanePath.c_str() });

    constexpr uint32_t windowWidth = 1280;
    constexpr uint32_t windowHeight = 720;

    Common::EngineInfo info{};
    info.m_designSize = Dimension(windowWidth, windowHeight);
    info.m_gltfInfos = gltfInfo;
    info.m_screenSize = Dimension(windowWidth, windowHeight);

    Common::EngineManager engineManager(info);

    engineManager.Init();

    engineManager.Loop();

    engineManager.DeInit();

    return 0;
}