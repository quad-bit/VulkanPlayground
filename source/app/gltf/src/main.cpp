//#include <taskflow/taskflow.hpp>
#include "EngineManager.h"

#include "Defines.h"
#include <optional>
#include <memory>

class ApplicationHandler
{
private:
    flecs::world& m_world;
    flecs::entity m_blackKnight;

public:
    ApplicationHandler(flecs::world& world) : m_world(world)
    {

    }

    void Update(const double& deltaTime)
    {

    }

    ~ApplicationHandler()
    {

    }
};



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

    std::vector<Loops::ModelLoadInfo> gltfInfo;
    gltfInfo.push_back({ 100.0f, chessPath.c_str() });
//    gltfInfo.push_back({ 1.0f, suzzanePath.c_str() });

    constexpr uint32_t windowWidth = 1280;
    constexpr uint32_t windowHeight = 720;

    Loops::EngineInfo info{};
    info.m_designSize = Loops::Dimension(windowWidth, windowHeight);
    info.m_gltfInfos = gltfInfo;
    info.m_screenSize = Loops::Dimension(windowWidth, windowHeight);


    Loops::AppCallbacks callback{};
    {
        std::unique_ptr<ApplicationHandler> pAppHandler;
        auto init = [&pAppHandler](flecs::world& world)
            {
                pAppHandler = std::make_unique<ApplicationHandler>(world);
            };

        callback.m_Start.push_back(init);

        auto update = std::bind(&ApplicationHandler::Update, pAppHandler.get(), std::placeholders::_1);
        callback.m_Update.push_back(update);
    }

    Loops::EngineManager engineManager(info, callback);

    engineManager.Loop();

    engineManager.DeInit();

    return 0;
}