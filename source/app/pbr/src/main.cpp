//#include <taskflow/taskflow.hpp>
#include <EngineManager.h>
#include <Defines.h>
#include <Assertion.h>
#include <glm/gtx/compatibility.hpp>
#include <optional>
#include <memory>
#include <LightManager.h>
#include <math/Colors.h>

#define CUBE 0
#define CHESS 1
#define OPTION CHESS

class OrbitalCamera
{
private:
    flecs::entity& m_cameraEntity;
    Loops::Transform& m_cameraTransform;

    glm::vec3 m_targetPosition;
    float m_orbitRadius = 10.0f;

    float m_yaw = 0.0f;
    float m_pitch = 0.0f;
    float m_roll = 0.0f;
    float m_initialRoll = 0.0f;
    float m_initialYaw = 0.0f;

    // Interaction limits
    static constexpr float m_minRadius = 1.0f;
    static constexpr float m_maxRadius = 20.0f;

public:
    OrbitalCamera(flecs::entity& cameraEntity,
        Loops::Transform& cameraTransform,
        const glm::vec3& target, const float& radius) :
        m_cameraEntity(cameraEntity),
        m_cameraTransform(cameraTransform),
        m_targetPosition(target),
        m_orbitRadius(radius)
    {
        m_pitch = m_cameraTransform.m_eulerAngles.x;
        m_yaw = m_cameraTransform.m_eulerAngles.y;
    }

    // Update rotation angles based on mouse movement deltas
    void Rotate(float deltaYaw, float deltaPitch)
    {
        m_yaw += deltaYaw;
        m_pitch += deltaPitch;

        // Constraint to avoid flipping the camera upside down
        constexpr float limit = glm::radians(89.0f);
        m_pitch = glm::clamp(m_pitch, -limit, limit);

        m_cameraTransform.m_position = GetPosition();
        //m_cameraTransform.m_eulerAngles.y -= deltaYaw;
        //m_cameraTransform.m_eulerAngles.x -= deltaPitch;

        int k = 0;
    }

    // Zoom in or out by adjusting the radius
    void Zoom(float deltaRadius)
    {
        m_orbitRadius = glm::clamp(m_orbitRadius - deltaRadius, m_minRadius, m_maxRadius);
    }

    glm::vec3 GetPosition() const
    {
        static const float yOffset = m_cameraTransform.m_position.y;
        glm::vec3 offset;
        offset.x = m_orbitRadius * cos(m_pitch) * sin(m_yaw);
        offset.y = -m_orbitRadius * sin(m_pitch);
        offset.z = m_orbitRadius * cos(m_pitch) * cos(m_yaw);
        return m_targetPosition + offset;
    }
};

//// Inside your mouse movement callback logic
//void onMouseMove(float xoffset, float yoffset)
// {
//    float sensitivity = 0.005f; // Tweak for responsiveness
//
//    // Pass horizontal and vertical updates directly into the class
//    camera.Rotate(xoffset * sensitivity, yoffset * sensitivity);
//}
//
//// Inside your scroll wheel callback logic
//void onMouseScroll(float yoffset)
// {
//    float zoomSpeed = 0.5f;
//    camera.Zoom(yoffset * zoomSpeed);
//}



class ApplicationHandler
{
private:
    flecs::world& m_world;
    flecs::entity m_camera;

    std::unique_ptr<OrbitalCamera> m_orbitScript;

public:
    ApplicationHandler(flecs::world& world) : m_world(world)
    {
        m_camera = m_world.lookup("MainCamera");
        Loops::ASSERT_MSG(m_camera.is_valid(), "Camera not found");

#if OPTION == CUBE
        Loops::Transform& camTransform = m_camera.get_mut<Loops::Transform>();
        camTransform.m_position = glm::vec3(-30, 2, 0);
        camTransform.m_eulerAngles = glm::vec3(glm::radians(0.0f), glm::radians(89.9f), 0.0f);
        auto cube = m_world.lookup("scene_Root::Sketchfab_model");
        if (cube.is_valid())
        {
            auto& cubeTransform = cube.get_mut<Loops::Transform>();
            cubeTransform.m_eulerAngles = glm::vec3(glm::radians(0.0f), glm::radians(40.0f), glm::radians(40.0f));
        }

#elif OPTION == CHESS
        Loops::Transform& camTransform = m_camera.get_mut<Loops::Transform>();
        camTransform.m_position = glm::vec3(-10, 5, 0);
        camTransform.m_eulerAngles = glm::vec3(glm::radians(-25.0f), glm::radians(89.9f), glm::radians(0.0f));

#endif
        m_orbitScript = std::make_unique<OrbitalCamera>(m_camera, camTransform, glm::vec3(0.0f),
            glm::abs(camTransform.m_position.x));

        {
            Loops::Transform transform{};
            transform.m_position = glm::vec3(0, 5, -30);//-10
            transform.m_eulerAngles = glm::vec3(glm::radians(10.0f), 0.0f, 0.0f);//20.0f
            //camTransform.m_position = glm::vec3(0, 5, -15);
            //camTransform.m_eulerAngles = glm::vec3(glm::radians(10.0f), 0.0f, 0.0f);

            auto dirLightData = Loops::LightManager::GetInstance()->GetDirectionalLight();
            dirLightData->m_diffuse = Loops::Math::WHITE_COLOR;
            dirLightData->m_specular = glm::vec3(.3f, .3f, .5f);
            dirLightData->m_ambient = Loops::Math::WHITE_COLOR/1.5f;// glm::vec3(.3f, .3f, .3f);

            Loops::Light dirLight{};
            dirLight.m_data = dirLightData;

            auto e = world.entity("DirectionalLight");
            e.emplace<Loops::Transform>(transform);
            e.emplace<Loops::Light>(dirLight);
        }
    }

    glm::mat4 GetViewMatrix(const glm::vec3& cameraPos, const glm::vec3& cameraFront, const glm::vec3& cameraUp)
    {
        // Target position is always camera center + direction vector
        return glm::lookAt(cameraPos, cameraPos + cameraFront, cameraUp);
    }

    glm::vec3 RotateCamera(float xOffset, float yOffset,
        float& yaw, float& pitch, float sensitivity = 0.1f)
    {
        xOffset *= sensitivity;
        yOffset *= sensitivity;

        yaw += xOffset;
        pitch += yOffset;

        // Constrain pitch to prevent flipping upside down
        if (pitch > 89.0f)  pitch = 89.0f;
        if (pitch < -89.0f) pitch = -89.0f;

        // Calculate the new Direction Vector using trigonometry
        glm::vec3 direction;
        direction.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
        direction.y = sin(glm::radians(pitch));
        direction.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));

        // Update camera direction
        auto cameraFront = glm::normalize(direction);
        return cameraFront;
    }

    void Update(const double& deltaTime)
    {
        if (m_camera.is_valid())
        {
            {
                m_orbitScript->Rotate(0.004f, 0.0f);
            }
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

    //auto pbrDemo = std::string{ ASSETS_PATH } + "/models/ABeautifulGame/glTF/ABeautifulGame.gltf";
    //auto pbrDemo = std::string{ ASSETS_PATH } + "/models/FlightHelmet/FlightHelmet.gltf";

#if OPTION == CUBE
    auto pbrDemo = std::string{ ASSETS_PATH } + "/models/RubiksCube/scene.gltf";
    std::vector<Loops::ModelLoadInfo> gltfInfo;
    gltfInfo.push_back({ 1.0f, pbrDemo.c_str() });
#elif OPTION == CHESS
    auto pbrDemo = std::string{ ASSETS_PATH } + "/models/ABeautifulGame/glTF/ABeautifulGame.gltf";
    std::vector<Loops::ModelLoadInfo> gltfInfo;
    gltfInfo.push_back({ 10.0f, pbrDemo.c_str() });
#endif

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
    info.m_pipelines = { Loops::Tasking::PipelineType::TEXTURED };

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