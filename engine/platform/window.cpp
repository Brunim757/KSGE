#include <windows.h>
#define GLFW_EXPOSE_NATIVE_WIN32

#include "engine/platform/window.hpp"

#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

namespace ksge {

namespace {

void ensureGlfwInitialized()
{
    static bool initialized = false;
    if (!initialized)
    {
        glfwInit();
        initialized = true;
    }
}

}

Window::Window(std::int32_t width, std::int32_t height, const char* title)
    : handle_(nullptr)
    , width_(width)
    , height_(height)
    , pendingWidth_(width)
    , pendingHeight_(height)
    , pendingScroll_(0.0)
    , resized_(false)
    , keyState_{}
{
    ensureGlfwInitialized();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
    handle_ = glfwCreateWindow(width, height, title, nullptr, nullptr);
    glfwSetWindowUserPointer(handle_, this);
    glfwSetKeyCallback(handle_, &Window::onKey);
    glfwSetWindowFocusCallback(handle_, &Window::onFocus);
    glfwSetFramebufferSizeCallback(handle_, &Window::onFramebufferResize);
    glfwSetScrollCallback(handle_, &Window::onScroll);
}

Window::~Window()
{
    if (handle_)
    {
        glfwDestroyWindow(handle_);
    }
    glfwTerminate();
}

bool Window::shouldClose() const
{
    return glfwWindowShouldClose(handle_) != 0;
}

void Window::pollEvents()
{
    glfwPollEvents();
}

bool Window::takeResize(std::int32_t& width, std::int32_t& height)
{
    if (!resized_)
    {
        return false;
    }
    resized_ = false;
    width_ = pendingWidth_;
    height_ = pendingHeight_;
    width = width_;
    height = height_;
    return true;
}

void* Window::nativeHandle() const
{
    return glfwGetWin32Window(handle_);
}

GLFWwindow* Window::handle() const
{
    return handle_;
}

double Window::takeScrollDelta()
{
    const double delta = pendingScroll_;
    pendingScroll_ = 0.0;
    return delta;
}

bool Window::keyDown(int glfwKey) const
{
    if (glfwKey < 0 || glfwKey >= static_cast<int>(keyState_.size()))
    {
        return false;
    }
    return keyState_[static_cast<std::size_t>(glfwKey)] != 0u;
}

void Window::onKey(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    (void)scancode;
    (void)mods;
    Window* self = static_cast<Window*>(glfwGetWindowUserPointer(window));
    if (self && key >= 0 && key < static_cast<int>(self->keyState_.size()))
    {
        self->keyState_[static_cast<std::size_t>(key)] =
            (action == GLFW_PRESS || action == GLFW_REPEAT) ? 1u : 0u;
    }
}

void Window::onFocus(GLFWwindow* window, int focused)
{
    Window* self = static_cast<Window*>(glfwGetWindowUserPointer(window));
    if (self && !focused)
    {
        self->keyState_.fill(0u);
    }
}

void Window::onFramebufferResize(GLFWwindow* window, int width, int height)
{
    Window* self = static_cast<Window*>(glfwGetWindowUserPointer(window));
    if (self && width > 0 && height > 0)
    {
        self->pendingWidth_ = width;
        self->pendingHeight_ = height;
        self->resized_ = true;
    }
}

void Window::onScroll(GLFWwindow* window, double offsetX, double offsetY)
{
    (void)offsetX;
    Window* self = static_cast<Window*>(glfwGetWindowUserPointer(window));
    if (self)
    {
        self->pendingScroll_ += offsetY;
    }
}

}