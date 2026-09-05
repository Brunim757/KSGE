#pragma once

#include <array>
#include <cstdint>

struct GLFWwindow;

namespace ksge {

constexpr std::size_t kGlfwKeyCount = 512u;

class Window
{
public:
    Window(std::int32_t width, std::int32_t height, const char* title);
    ~Window();

    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    bool shouldClose() const;
    void pollEvents();
    bool takeResize(std::int32_t& width, std::int32_t& height);

    void* nativeHandle() const;
    GLFWwindow* handle() const;
    double takeScrollDelta();

    bool keyDown(int glfwKey) const;

private:
    static void onKey(GLFWwindow* window, int key, int scancode, int action, int mods);
    static void onFocus(GLFWwindow* window, int focused);
    static void onFramebufferResize(GLFWwindow* window, int width, int height);
    static void onScroll(GLFWwindow* window, double offsetX, double offsetY);

    GLFWwindow* handle_;
    std::int32_t width_;
    std::int32_t height_;
    std::int32_t pendingWidth_;
    std::int32_t pendingHeight_;
    double pendingScroll_;
    bool resized_;
    std::array<std::uint8_t, kGlfwKeyCount> keyState_;
};

}