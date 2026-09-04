#include "engine/core/world.hpp"
#include "engine/graphics/device.hpp"
#include "engine/platform/window.hpp"

#include <cstdint>
#include <cstring>

namespace {

constexpr float kClearColor[4] = {0.08f, 0.09f, 0.11f, 1.0f};
constexpr std::int32_t kDefaultWidth = 1280;
constexpr std::int32_t kDefaultHeight = 720;
constexpr int kSelfTestFrames = 300;

bool hasFlag(int argc, char** argv, const char* flag)
{
    for (int i = 1; i < argc; ++i)
    {
        if (std::strcmp(argv[i], flag) == 0)
        {
            return true;
        }
    }
    return false;
}

}

int main(int argc, char** argv)
{
    const bool selfTest = hasFlag(argc, argv, "--selftest");

    ksge::Window window(kDefaultWidth, kDefaultHeight, "KSGE Editor");
    ksge::GraphicsDevice device(window.nativeHandle(), kDefaultWidth, kDefaultHeight);
    ksge::World world;

    std::int32_t frameCount = 0;
    while (!window.shouldClose())
    {
        window.pollEvents();

        std::int32_t width = 0;
        std::int32_t height = 0;
        if (window.takeResize(width, height))
        {
            device.resize(width, height);
        }

        world.step();
        device.beginFrame(kClearColor);
        device.present();

        ++frameCount;
        if (selfTest && frameCount >= kSelfTestFrames)
        {
            break;
        }
    }

    return 0;
}