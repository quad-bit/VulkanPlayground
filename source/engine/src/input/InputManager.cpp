#include "input/InputManager.h"
#include <imgui.h>


Common::IO::InputManager::~InputManager()
{
    //for (uint16_t i = 0; i < m_poolSize; i++)
    //{
    //    //delete m_keyEventPool[i];
    //}

    //m_keyEventPool.clear();
}

void Common::IO::InputManager::DeInit()
{
    PLOGD << "Input manager DeInit";
}

void Common::IO::InputManager::Update()
{
}
