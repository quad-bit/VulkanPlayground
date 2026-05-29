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
    flecs::entity m_cone;
    flecs::entity m_cube;
    flecs::entity m_camera;

public:
    ApplicationHandler(flecs::world& world) : m_world(world)
    {
        m_cone = m_world.lookup("bvhDemo_Root::Cone");
        m_cube = m_world.lookup("bvhDemo_Root::Cube.001");
        m_camera = m_world.lookup("MainCamera");

        Loops::ASSERT_MSG(m_camera.is_valid(), "Camera not found");

        Loops::Transform& camTransform = m_camera.get_mut<Loops::Transform>();
        //camTransform.m_position = glm::vec3(0, 5, -15);
        //camTransform.m_eulerAngles = glm::vec3(glm::radians(20.0f), glm::radians(0.0f), 0);

        camTransform.m_position = glm::vec3(20, 0, 0);
        camTransform.m_eulerAngles = glm::vec3(glm::radians(0.0f), glm::radians(-40.0f), 0);

        //camTransform.m_position = glm::vec3(0, 15, 0);
        //camTransform.m_eulerAngles = glm::vec3(glm::radians(90.0f), glm::radians(0.0f), 0);
    }

    void Update(const double& deltaTime)
    {
        if (m_cone.is_valid())
        {
            const float speed = 2.0f;
            Loops::Transform& t = m_cone.get_mut<Loops::Transform>();
            t.m_eulerAngles.z += deltaTime * speed;
            //t.m_position = glm::vec3(5.0f, 4.0f, 0.0f);
        }

#if 0
        if (m_cube.is_valid())
        {
            Loops::Transform& t = m_cube.get_mut<Loops::Transform>();
            static float totalTime = 0;
            auto pingPong = [](float time, float length) -> float
            {
                float t = fmod(time, length * 2.0f);
                return length - fabs(t - length);
            };

            glm::vec3 startPos(5.0f, 0.0f, 0.0f);
            glm::vec3 endPos(-5.0f, 0.0f, 0.0f);
            float duration = 2.0f;
            totalTime += deltaTime;

            float val = pingPong(totalTime, duration)/duration;

            t.m_position.x = glm::lerp(startPos.x, endPos.x, val);// 1.0f - (float)glm::exp(-10.0f * deltaTime));
        }
#endif
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

    auto bvhDemo = std::string{ ASSETS_PATH } + "/models/bvhDemo/bvhDemo.gltf";

    std::vector<Loops::ModelLoadInfo> gltfInfo;
    gltfInfo.push_back({ 1.0f, bvhDemo.c_str() });

    constexpr uint32_t windowWidth = 1280;
    constexpr uint32_t windowHeight = 720;

    Loops::EngineInfo info{};
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

    Loops::EngineManager engineManager(info, callback);

    engineManager.Loop();

    engineManager.DeInit();

    return 0;
}