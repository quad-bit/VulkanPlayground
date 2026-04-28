#include "EventBus.h"

Common::Events::EventBus* Common::Events::EventBus::instance = nullptr;
Common::Events::EventBus * Common::Events::EventBus::GetInstance()
{
    if (instance == nullptr)
    {
        instance = new Common::Events::EventBus();
    }
    return instance;
}

void Common::Events::EventBus::Init()
{

}

void Common::Events::EventBus::DeInit()
{
    m_subscribers.clear();
}
