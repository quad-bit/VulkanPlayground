//#include <taskflow/taskflow.hpp>
#include <EngineManager.h>
#include <Defines.h>
#include <Assertion.h>
#include <glm/gtx/compatibility.hpp>
#include <optional>
#include <memory>


class ApplicationHandler
{
private:
    flecs::world& m_world;
    flecs::entity m_camera;

public:
    ApplicationHandler(flecs::world& world) : m_world(world)
    {
        m_camera = m_world.lookup("MainCamera");

        Loops::ASSERT_MSG(m_camera.is_valid(), "Camera not found");

        Loops::Transform& camTransform = m_camera.get_mut<Loops::Transform>();
        //camTransform.m_position = glm::vec3(0, 5, -15);
        //camTransform.m_eulerAngles = glm::vec3(glm::radians(20.0f), glm::radians(0.0f), 0);

        //camTransform.m_position = glm::vec3(0, 0, 0);
        //camTransform.m_eulerAngles = glm::vec3(glm::radians(0.0f), glm::radians(0.0f), 0);

        camTransform.m_position = glm::vec3(0, 2, -10);
        camTransform.m_eulerAngles = glm::vec3(glm::radians(5.0f), 0.0f, 0.0f);
    }

    void Update(const double& deltaTime)
    {
        if (m_camera.is_valid())
        {
            Loops::Transform& t = m_camera.get_mut<Loops::Transform>();
            //t.m_eulerAngles.y += deltaTime * speed;
            //t.m_position = glm::vec3(5.0f, 4.0f, 0.0f);
        }
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

    auto pbrDemo = std::string{ ASSETS_PATH } + "/models/ABeautifulGame/glTF/ABeautifulGame.gltf";
    //auto pbrDemo = std::string{ ASSETS_PATH } + "/models/FlightHelmet/FlightHelmet.gltf";

    //auto pbrDemo = std::string{ ASSETS_PATH } + "/models/Cube/Cube.gltf";
    std::vector<Loops::ModelLoadInfo> gltfInfo;
    gltfInfo.push_back({ 10.0f, pbrDemo.c_str() });

    Loops::EngineInfo info{};
#ifdef EXTERNAL_MONITOR
    constexpr uint32_t windowWidth = 1280;
    constexpr uint32_t windowHeight = 720;
#else
    constexpr uint32_t windowWidth = 1920;
    constexpr uint32_t windowHeight = 1200;
    //info.m_enableFullScreen = true;
#endif
    info.m_designSize = Loops::Dimension(windowWidth, windowHeight);
    info.m_screenSize = Loops::Dimension(windowWidth, windowHeight);
    info.m_gltfInfos = gltfInfo;
    info.m_pipelines = { Loops::Tasking::PipelineType::BVH_RENDER };

    Loops::AppCallbacks callback{};
    {
        std::unique_ptr<ApplicationHandler> pAppHandler;
        auto init = [&pAppHandler](flecs::world& world)
            {
                pAppHandler = std::make_unique<ApplicationHandler>(world);
            };

        callback.m_Start.push_back(init);

        auto update = [&pAppHandler](const double& deltaTime)
            {
                pAppHandler->Update(deltaTime);
            };
        //auto update = std::bind(&ApplicationHandler::Update, pAppHandler.get(), std::placeholders::_1);
        callback.m_Update.push_back(update);

        auto DeInit = [&pAppHandler]()
            {
                pAppHandler.reset();
            };
        callback.m_Exit.push_back(DeInit);
    }

    Loops::EngineManager* engineManager = new Loops::EngineManager(info, callback);

    engineManager->Loop();

    engineManager->DeInit();

    delete engineManager;

    return 0;
}