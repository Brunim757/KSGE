#include "engine/core/world.hpp"
#include "engine/graphics/device.hpp"
#include "engine/graphics/mesh_upload.hpp"
#include "engine/graphics/renderer.hpp"
#include "engine/platform/input.hpp"
#include "engine/platform/window.hpp"
#include "engine/scene/camera_service.hpp"
#include "engine/scene/components.hpp"

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

void spawnDemoScene(
    flecs::world& world,
    std::uint32_t floorMesh,
    std::uint32_t sphereMesh,
    std::uint32_t cubeMesh)
{
    auto spawn = [&](std::uint32_t meshAsset,
                     const ksge::math::Vec3& position,
                     const ksge::math::Vec3& scale,
                     const ksge::PbrMaterial& material)
    {
        world.entity()
            .set<ksge::Transform>({position, {0.0f, 0.0f, 0.0f, 1.0f}, scale})
            .set<ksge::MeshRenderer>({meshAsset})
            .set<ksge::PbrMaterial>(material);
    };

    ksge::PbrMaterial concrete;
    concrete.metallicFactor = 0.0f;
    concrete.roughnessFactor = 0.9f;
    spawn(floorMesh, {0.0f, -1.0f, 0.0f}, {30.0f, 1.0f, 30.0f}, concrete);

    ksge::PbrMaterial metal;
    metal.metallicFactor = 1.0f;
    metal.roughnessFactor = 0.15f;
    spawn(sphereMesh, {0.0f, 0.6f, 0.0f}, {1.0f, 1.0f, 1.0f}, metal);

    ksge::PbrMaterial rough;
    rough.metallicFactor = 0.0f;
    rough.roughnessFactor = 1.0f;
    spawn(sphereMesh, {-2.2f, 0.6f, 0.0f}, {1.0f, 1.0f, 1.0f}, rough);

    ksge::PbrMaterial glow;
    glow.metallicFactor = 0.0f;
    glow.roughnessFactor = 0.5f;
    glow.emissiveFactor = {1.0f, 0.3f, 0.1f};
    spawn(cubeMesh, {2.2f, 0.6f, 0.0f}, {1.0f, 1.0f, 1.0f}, glow);

    ksge::PbrMaterial stone;
    stone.metallicFactor = 0.1f;
    stone.roughnessFactor = 0.65f;
    spawn(cubeMesh, {-1.4f, 0.4f, -2.0f}, {0.8f, 0.8f, 0.8f}, stone);

    ksge::PbrMaterial polished;
    polished.metallicFactor = 0.9f;
    polished.roughnessFactor = 0.35f;
    spawn(sphereMesh, {1.4f, 0.4f, -2.0f}, {0.9f, 0.9f, 0.9f}, polished);
}

}

int main(int argc, char** argv)
{
    const bool selfTest = hasFlag(argc, argv, "--selftest");

    ksge::Window window(kDefaultWidth, kDefaultHeight, "KSGE Editor");
    ksge::GraphicsDevice device(window.nativeHandle(), kDefaultWidth, kDefaultHeight);
    ksge::World world;
    ksge::Input input;
    ksge::CameraService cameraService(world.handle());

    ksge::Renderer renderer(device, world.handle());
    const std::uint32_t cubeMesh = renderer.uploadMesh(ksge::makeCube(1.0f));
    const std::uint32_t sphereMesh = renderer.uploadMesh(ksge::makeSphere(48u, 24u, 1.0f));
    spawnDemoScene(world.handle(), cubeMesh, sphereMesh, cubeMesh);

    std::int32_t frameCount = 0;
    while (!window.shouldClose())
    {
        window.pollEvents();

        std::int32_t width = 0;
        std::int32_t height = 0;
        if (window.takeResize(width, height))
        {
            device.resize(width, height);
            cameraService.editorCamera().get_mut<ksge::Camera>().aspectRatio =
                static_cast<float>(width) / static_cast<float>(height);
        }

        input.capture(window);
        cameraService.update(input.snapshot());

        world.step();
        device.beginFrame(kClearColor);
        renderer.render();
        device.present();

        ++frameCount;
        if (selfTest && frameCount >= kSelfTestFrames)
        {
            break;
        }
    }

    return 0;
}