#pragma once

#include <vector>
#include "Event.h"

namespace Loops::IO
{
    class MouseInputManager
    {
    private:
        MouseInputManager() {}
        MouseInputManager(MouseInputManager const&) {}
        MouseInputManager const& operator= (MouseInputManager const&) {}

        static MouseInputManager* instance;

        std::vector<Events::MouseButtonEvent*> mouseButtonEventPool;
        uint16_t buttonEventPoolSize = 10;
        uint16_t buttonEventCounter = 0;

        std::vector<Events::MousePointerEvent*> mousePointerEventPool;
        uint16_t pointerEventPoolSize = 10;
        uint16_t pointerEventCounter = 0;

        std::vector<Events::MouseDragEvent*> mouseDragEventPool;
        uint16_t dragEventPoolSize = 10;
        uint16_t dragEventCounter = 0;

        Events::KeyState currentMouseButtonState;
        Events::MouseButtons currentMouseButtonDown;
        double mouseX, mouseY;

        void CreateMouseDragEvent(Events::KeyState state);
        Events::MouseButtonEvent* FetchMouseButtonEvent();
        Events::MousePointerEvent* FetchMousePointerEvent();
        Events::MouseDragEvent* FetchMouseDragEvent();

    public:
        void Init();
        void DeInit();
        void Update();
        static MouseInputManager* GetInstance();
        ~MouseInputManager();

        void MousePointerEventHandler(double x, double y);
        void MouseButtonEventHandler(const char* buttonName, const char* buttonEvent);
    };
}