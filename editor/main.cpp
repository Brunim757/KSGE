#include "engine/core/world.hpp"
#include "engine/graphics/device.hpp"
#include "engine/graphics/mesh_upload.hpp"
#include "engine/graphics/renderer.hpp"
#include "engine/platform/input.hpp"
#include "engine/platform/window.hpp"
#include "engine/scene/camera_service.hpp"
#include "engine/scene/components.hpp"
#include "engine/world/chunk_streamer.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>

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

int parameterValue(int argc, char** argv, const char* flag, int fallback)
{
    for (int i = 1; i < argc - 1; ++i)
    {
        if (std::strcmp(argv[i], flag) == 0)
        {
            return std::atoi(argv[i + 1]);
        }
    }
    return fallback;
}

void spawnStressScene(flecs::world& world, std::uint32_t count, std::uint32_t mesh)
{
    const std::uint32_t side = static_cast<std::uint32_t>(std::ceil(std::sqrt(static_cast<float>(count))));
    const float spacing = 1.6f;
    for (std::uint32_t index = 0u; index < count; ++index)
    {
        const float x = (static_cast<float>(index % side) - static_cast<float>(side) * 0.5f) * spacing;
        const float z = (static_cast<float>(index / side) - static_cast<float>(side) * 0.5f) * spacing;

        ksge::PbrMaterial material;
        if ((index % 2u) == 0u)
        {
            material.metallicFactor = 1.0f;
            material.roughnessFactor = 0.2f;
        }
        else
        {
            material.metallicFactor = 0.0f;
            material.roughnessFactor = 0.8f;
        }

        world.entity()
            .set<ksge::Transform>({{x, 0.35f, z}, {0.0f, 0.0f, 0.0f, 1.0f}, {0.8f, 0.8f, 0.8f}})
            .set<ksge::MeshRenderer>({mesh})
            .set<ksge::PbrMaterial>(material);
    }
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
    spawn(floorMesh, {0.0f, -1.0f, 0.0f}, {20.0f, 1.0f, 20.0f}, concrete);

    ksge::PbrMaterial metal;
    metal.metallicFactor = 1.0f;
    metal.roughnessFactor = 0.15f;
    spawn(sphereMesh, {-2.5f, 0.6f, 0.0f}, {1.0f, 1.0f, 1.0f}, metal);

    ksge::PbrMaterial rough;
    rough.metallicFactor = 0.0f;
    rough.roughnessFactor = 0.7f;
    rough.emissiveFactor = {0.4f, 0.15f, 0.05f};
    spawn(cubeMesh, {2.5f, 0.6f, 0.0f}, {1.0f, 1.0f, 1.0f}, rough);
}

}

int main(int argc, char** argv)
{
    const bool selfTest = hasFlag(argc, argv, "--selftest");
    const int stressCount = parameterValue(argc, argv, "--stress", 0);

    ksge::Window window(kDefaultWidth, kDefaultHeight, "KSGE Editor");
    ksge::GraphicsDevice device(window.nativeHandle(), kDefaultWidth, kDefaultHeight);
    ksge::World world;
    ksge::Input input;
    ksge::CameraService cameraService(world.handle());

    ksge::Renderer renderer(device, world.handle());
    const std::uint32_t cubeMesh = renderer.uploadMesh(ksge::makeCube(1.0f));
    const std::uint32_t sphereMesh = renderer.uploadMesh(ksge::makeSphere(48u, 24u, 1.0f));
    const std::uint32_t mediumSphere = renderer.uploadMesh(ksge::makeSphere(20u, 10u, 1.0f));
    const std::uint32_t lowSphere = renderer.uploadMesh(ksge::makeSphere(8u, 4u, 1.0f));
    renderer.setLodChain(sphereMesh, mediumSphere, lowSphere);
    spawnDemoScene(world.handle(), cubeMesh, sphereMesh, cubeMesh);
    if (stressCount > 0)
    {
        spawnStressScene(world.handle(), static_cast<std::uint32_t>(stressCount), cubeMesh);
    }

    ksge::ChunkStreamer streamer(world.handle(), cubeMesh, sphereMesh);
    streamer.setRadius(120.0f);

    std::int32_t frameCount = 0;
    std::uint32_t previousDebugKeys = 0u;
    constexpr ksge::KeyCode kDebugKeys[7] = {
        ksge::KeyDigit0,
        ksge::KeyDigit1,
        ksge::KeyDigit2,
        ksge::KeyDigit3,
        ksge::KeyDigit4,
        ksge::KeyDigit5,
        ksge::KeyDigit6,
    };

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

        std::uint32_t debugKeys = 0u;
        for (int index = 0; index < 7; ++index)
        {
            if (ksge::isPressed(input.snapshot(), kDebugKeys[index]))
            {
                debugKeys |= 1u << static_cast<unsigned>(index);
            }
        }
        for (int index = 0; index < 7; ++index)
        {
            const std::uint32_t mask = 1u << static_cast<unsigned>(index);
            if ((debugKeys & mask) != 0u && (previousDebugKeys & mask) == 0u)
            {
                renderer.setDebugMode(static_cast<std::uint32_t>(index));
            }
        }
        previousDebugKeys = debugKeys;

        world.step();
        if (selfTest && frameCount == 150)
        {
            cameraService.editorCamera().get_mut<ksge::Transform>().position = {4200.0f, 5.0f, -3000.0f};
            streamer.saveAll();
        }
        const ksge::math::Vec3 cameraPosition =
            cameraService.editorCamera().get<ksge::Transform>().position;
        streamer.update(cameraPosition.x, cameraPosition.z);
        device.beginFrame(kClearColor);
        renderer.render();
        if (selfTest && (frameCount % 30) == 0)
        {
            const std::uint32_t mode = static_cast<std::uint32_t>(frameCount / 30) % 7u;
            renderer.setDebugMode(mode);
            renderer.updateGpuTime();
            float luminance = 0.0f;
            device.readAverageLuminance(luminance);
            std::printf(
                "KSGE selftest frame %d mode %u luminance %.4f cpu %.2fms gpu %.2fms "
                "entities %u(transform %u) gathered %u pushed %u gbPushed %u chunks %u pending %u "
                "draws %u(gb %u sh %u) instances %u(gb %u sh %u)\n",
                frameCount,
                static_cast<unsigned>(mode),
                luminance,
                renderer.frameCpuMs(),
                renderer.frameGpuMs(),
                static_cast<unsigned>(3u + static_cast<std::uint32_t>(stressCount)),
                static_cast<unsigned>(world.handle().count<ksge::Transform>()),
                renderer.frameGathered(),
                renderer.framePushed(),
                renderer.gbufferPushed(),
                static_cast<unsigned>(streamer.activeChunks()),
                static_cast<unsigned>(streamer.pendingJobs()),
                renderer.frameDraws(),
                renderer.gbufferDraws(),
                renderer.shadowDraws(),
                renderer.frameInstances(),
                renderer.gbufferInstances(),
                renderer.shadowInstances());
        }
        device.present();

        ++frameCount;
        if (selfTest && frameCount >= kSelfTestFrames)
        {
            break;
        }
    }

    return 0;
}