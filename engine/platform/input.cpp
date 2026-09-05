#include "engine/platform/input.hpp"

#include "engine/platform/window.hpp"

#include <GLFW/glfw3.h>

namespace ksge {

void Input::capture(Window& window)
{
    GLFWwindow* handle = window.handle();

    snapshot_.pressed = 0u;

    auto setIf = [&](KeyCode code, int glfwKey)
    {
        if (window.keyDown(glfwKey))
        {
            snapshot_.pressed |= 1u << static_cast<unsigned>(code);
        }
    };

    setIf(KeyW, GLFW_KEY_W);
    setIf(KeyA, GLFW_KEY_A);
    setIf(KeyS, GLFW_KEY_S);
    setIf(KeyD, GLFW_KEY_D);
    setIf(KeyQ, GLFW_KEY_Q);
    setIf(KeyE, GLFW_KEY_E);
    setIf(KeyR, GLFW_KEY_R);
    setIf(KeyF, GLFW_KEY_F);
    setIf(KeyZ, GLFW_KEY_Z);
    setIf(KeyX, GLFW_KEY_X);
    setIf(KeyC, GLFW_KEY_C);
    setIf(KeyV, GLFW_KEY_V);
    setIf(KeySpace, GLFW_KEY_SPACE);
    setIf(KeyLeftControl, GLFW_KEY_LEFT_CONTROL);
    setIf(KeyLeftShift, GLFW_KEY_LEFT_SHIFT);
    setIf(KeyEscape, GLFW_KEY_ESCAPE);
    setIf(KeyDelete, GLFW_KEY_DELETE);
    setIf(KeyDigit0, GLFW_KEY_0);
    setIf(KeyDigit1, GLFW_KEY_1);
    setIf(KeyDigit2, GLFW_KEY_2);
    setIf(KeyDigit3, GLFW_KEY_3);
    setIf(KeyDigit4, GLFW_KEY_4);
    setIf(KeyDigit5, GLFW_KEY_5);
    setIf(KeyDigit6, GLFW_KEY_6);

    const int leftButton = glfwGetMouseButton(handle, GLFW_MOUSE_BUTTON_LEFT);
    const int rightButton = glfwGetMouseButton(handle, GLFW_MOUSE_BUTTON_RIGHT);
    const int middleButton = glfwGetMouseButton(handle, GLFW_MOUSE_BUTTON_MIDDLE);
    snapshot_.mouseButtons = static_cast<std::uint8_t>(
        (rightButton == GLFW_PRESS ? 2u : 0u) |
        (middleButton == GLFW_PRESS ? 4u : 0u) |
        (leftButton == GLFW_PRESS ? 1u : 0u));

    double cursorX = 0.0;
    double cursorY = 0.0;
    glfwGetCursorPos(handle, &cursorX, &cursorY);

    if (!initialized_)
    {
        previousCursorX_ = cursorX;
        previousCursorY_ = cursorY;
        initialized_ = true;
    }

    snapshot_.mouseDX = static_cast<float>(cursorX - previousCursorX_);
    snapshot_.mouseDY = static_cast<float>(cursorY - previousCursorY_);
    previousCursorX_ = cursorX;
    previousCursorY_ = cursorY;

    snapshot_.mouseX = static_cast<float>(cursorX);
    snapshot_.mouseY = static_cast<float>(cursorY);
    snapshot_.scrollDelta = static_cast<float>(window.takeScrollDelta());
}

const InputSnapshot& Input::snapshot() const
{
    return snapshot_;
}

}