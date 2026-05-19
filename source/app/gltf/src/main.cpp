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
    flecs::entity m_knight;
    flecs::entity m_suzanne;

public:
    ApplicationHandler(flecs::world& world) : m_world(world)
    {
        //// beautiful game camera
        //camTransform.m_position = glm::vec3(0, 30, -70);
        //camTransform.m_eulerAngles = glm::vec3(glm::radians(20.0f), glm::radians(0.0f), 0);

        Loops::ASSERT_MSG(0, "FInd Camera And assign above values");

        m_suzanne = m_world.lookup("Suzanne_Root::Suzanne");
        Loops::Transform& t = m_suzanne.get_mut<Loops::Transform>();
        t.m_position.y = 30.0f;

        m_knight = m_world.lookup("ABeautifulGame_Root::Knight_W1");
        if (!m_knight.is_valid())
        {
            assert(0);
        }
    }

    void Update(const double& deltaTime)
    {
        if (m_knight.is_valid())
        {
            const float speed = 2.0f;
            Loops::Transform& t = m_knight.get_mut<Loops::Transform>();
            t.m_eulerAngles.y += deltaTime * speed;
        }

        if (m_suzanne.is_valid())
        {
            Loops::Transform& t = m_suzanne.get_mut<Loops::Transform>();
            static float totalTime = 0;
            auto pingPong = [](float time, float length) -> float
            {
                float t = fmod(time, length * 2.0f);
                return length - fabs(t - length);
            };

            glm::vec3 startPos(50.0f, 0.0f, 0.0f);
            glm::vec3 endPos(-50.0f, 0.0f, 0.0f);
            float duration = 2.0f;
            totalTime += deltaTime;

            float val = pingPong(totalTime, duration)/duration;

            t.m_position.x = glm::lerp(startPos.x, endPos.x, val);// 1.0f - (float)glm::exp(-10.0f * deltaTime));
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

    auto chessPath = std::string{ ASSETS_PATH } + "/models/ABeautifulGame/glTF/ABeautifulGame.gltf";
    auto suzzanePath = std::string{ ASSETS_PATH } + "/models/Suzanne/Suzanne.gltf";

    std::vector<Loops::ModelLoadInfo> gltfInfo;
    gltfInfo.push_back({ 100.0f, chessPath.c_str() });
    gltfInfo.push_back({ 5.0f, suzzanePath.c_str() });

    constexpr uint32_t windowWidth = 1280;
    constexpr uint32_t windowHeight = 720;

    Loops::EngineInfo info{};
    info.m_designSize = Loops::Dimension(windowWidth, windowHeight);
    info.m_gltfInfos = gltfInfo;
    info.m_screenSize = Loops::Dimension(windowWidth, windowHeight);
    info.m_pipelines = { Loops::Tasking::PipelineType::WIREFRAME };

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