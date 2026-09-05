#pragma once

#include <cstdint>

namespace ksge {

class Window;

enum KeyCode : std::uint32_t
{
    KeyW = 0,
    KeyA,
    KeyS,
    KeyD,
    KeyQ,
    KeyE,
    KeyR,
    KeyF,
    KeyZ,
    KeyX,
    KeyC,
    KeyV,
    KeySpace,
    KeyLeftControl,
    KeyLeftShift,
    KeyEscape,
    KeyDelete,
    KeyDigit0,
    KeyDigit1,
    KeyDigit2,
    KeyDigit3,
    KeyDigit4,
    KeyDigit5,
    KeyDigit6,
    KeyCount
};

struct InputSnapshot
{
    std::uint32_t pressed = 0u;
    std::uint8_t mouseButtons = 0u;
    float mouseX = 0.0f;
    float mouseY = 0.0f;
    float mouseDX = 0.0f;
    float mouseDY = 0.0f;
    float scrollDelta = 0.0f;
};

inline bool isPressed(const InputSnapshot& snapshot, KeyCode key)
{
    return (snapshot.pressed & (1u << static_cast<unsigned>(key))) != 0u;
}

class Input
{
public:
    Input() = default;

    Input(const Input&) = delete;
    Input& operator=(const Input&) = delete;

    void capture(Window& window);

    const InputSnapshot& snapshot() const;

private:
    InputSnapshot snapshot_;
    double previousCursorX_ = 0.0;
    double previousCursorY_ = 0.0;
    bool initialized_ = false;
};

}