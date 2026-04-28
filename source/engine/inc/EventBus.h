#ifndef EVENT_BUS
#define EVENT_BUS

#include <unordered_map>
#include <list>
#include <typeindex>
#include <functional>

#include "Event.h"

namespace Common::Events
{
    class EventBus
    {
    private:
        std::unordered_map<std::type_index, std::vector<std::function<void(const Event*)>>> m_subscribers;

        EventBus() {};
        EventBus(EventBus const&) {}
        EventBus const& operator= (EventBus const&) {}

        static EventBus* instance;

    public:
        static EventBus* GetInstance();
        void Init();
        void DeInit();

        template <typename EventType>
        void Publish(const Event* event) const
        {
            std::type_index typeIndex = typeid(EventType);
            auto it = m_subscribers.find(typeIndex);
            if ( it != m_subscribers.end())
            {
                auto& funcs = m_subscribers.at(typeIndex);

                for (auto& func : funcs)
                {
                    func(event);
                }
            }
        }

        template<typename EventType>
        void Subscribe(const std::function<void(const Event*)>& function)
        {
            auto& functionList = m_subscribers[typeid(EventType)];
            functionList.push_back(function);
        }
    };
}

#endif // !EVENT_BUS