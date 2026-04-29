#include "EventBus.h"

Loops::Events::EventBus* Loops::Events::EventBus::instance = nullptr;
Loops::Events::EventBus * Loops::Events::EventBus::GetInstance()
{
    if (instance == nullptr)
    {
        instance = new Loops::Events::EventBus();
    }
    return instance;
}

void Loops::Events::EventBus::Init()
{

}

void Loops::Events::EventBus::DeInit()
{
    m_subscribers.clear();
}
