#ifndef INPUT_MANAGER_H
#define INPUT_MANAGER_H

#include <GLFW/glfw3.h>
#if defined(GLFW_ENABLED)
#endif

#include "Event.h"
#include "EventBus.h"

#include <queue>
#include <typeindex>
#include <plog/Log.h>

namespace Loops
{
    namespace IO
    {
        class InputManager
        {

        private:
            InputManager() = delete;
            InputManager(InputManager const&) {}
            InputManager const& operator= (InputManager const&) {}

#if defined(GLFW_ENABLED) 
            GLFWwindow* m_windowObj = nullptr;
#endif
            template < typename EventType>
            void QueueEvent(EventType* event, std::queue<EventType>& queue)
            {
                const std::type_index typeIndex = typeid(EventType);
                //PLOGD << typeIndex.name();

                EventType e = *event;
                queue.push(e);
            }

        public:

#if defined(GLFW_ENABLED) 
            InputManager(GLFWwindow* windowObj);
#endif

            void DeInit();
            void Update();
            ~InputManager();

            template<typename EventType>
            void EventNotification(const Events::Event* event)
            {
                // befor publishing if in game ui exist, check if event is intended for it or not.

                Events::EventBus::GetInstance()->Publish<EventType>(event);
            }
        };
    }
}

#endif // !INPUT_MANAGER_H