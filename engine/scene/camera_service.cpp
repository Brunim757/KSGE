#include "engine/scene/camera_service.hpp"

#include "engine/core/world.hpp"
#include "engine/scene/camera.hpp"
#include "engine/scene/components.hpp"

#include <DirectXMath.h>
#include <algorithm>
#include <cmath>

namespace ksge {

namespace {

constexpr float kMoveSpeed = 24.0f;
constexpr float kLookSpeed = 0.0025f;
constexpr float kPitchLimit = 1.55f;
constexpr float kEpsilon = 1.0e-6f;

void buildBasis(float yaw, float pitch, math::Vec3& right, math::Vec3& up, math::Vec3& forward)
{
    const float cy = std::cos(yaw);
    const float sy = std::sin(yaw);
    const float cp = std::cos(pitch);
    const float sp = std::sin(pitch);

    forward = {cp * sy, sp, cp * cy};
    right = {cy, 0.0f, -sy};
    up = {-sp * sy, cp, -sp * cy};
}

}

CameraService::CameraService(flecs::world& world)
    : world_(world)
{
    camera_ = world_.entity("editor_camera")
                  .set<Transform>({})
                  .set<Camera>({0.0f, 0.0f, 60.0f, 0.1f, 5000.0f, 16.0f / 9.0f});
}

flecs::entity CameraService::editorCamera() const
{
    return camera_;
}

void CameraService::update(const InputSnapshot& input)
{
    const FrameTime& frameTime = world_.get<FrameTime>();
    const float deltaSeconds = static_cast<float>(frameTime.deltaSeconds);

    Transform& transform = camera_.get_mut<Transform>();
    Camera& camera = camera_.get_mut<Camera>();

    const bool lookEnabled = (input.mouseButtons & 2u) != 0u;
    if (lookEnabled)
    {
        camera.yaw += input.mouseDX * kLookSpeed;
        camera.pitch += input.mouseDY * kLookSpeed;
        camera.pitch = std::clamp(camera.pitch, -kPitchLimit, kPitchLimit);
    }

    math::Vec3 right;
    math::Vec3 up;
    math::Vec3 forward;
    buildBasis(camera.yaw, camera.pitch, right, up, forward);

    math::Vec3 horizontal{forward.x, 0.0f, forward.z};
    const float horizontalLength =
        std::sqrt(horizontal.x * horizontal.x + horizontal.z * horizontal.z);
    if (horizontalLength > kEpsilon)
    {
        const float inverseLength = 1.0f / horizontalLength;
        horizontal.x *= inverseLength;
        horizontal.z *= inverseLength;
    }

    const float step = kMoveSpeed * deltaSeconds;
    if (isPressed(input, KeyW))
    {
        transform.position.x += forward.x * step;
        transform.position.y += forward.y * step;
        transform.position.z += forward.z * step;
    }
    if (isPressed(input, KeyS))
    {
        transform.position.x -= forward.x * step;
        transform.position.y -= forward.y * step;
        transform.position.z -= forward.z * step;
    }
    if (isPressed(input, KeyD))
    {
        transform.position.x -= right.x * step;
        transform.position.y -= right.y * step;
        transform.position.z -= right.z * step;
    }
    if (isPressed(input, KeyA))
    {
        transform.position.x += right.x * step;
        transform.position.y += right.y * step;
        transform.position.z += right.z * step;
    }
    if (isPressed(input, KeySpace))
    {
        transform.position.y += step;
    }
    if (isPressed(input, KeyLeftControl))
    {
        transform.position.y -= step;
    }

    const DirectX::XMMATRIX basis = DirectX::XMMatrixSet(
        right.x, right.y, right.z, 0.0f,
        up.x, up.y, up.z, 0.0f,
        forward.x, forward.y, forward.z, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f);
    DirectX::XMStoreFloat4(
        &transform.rotation, DirectX::XMQuaternionRotationMatrix(basis));

    CameraFrame frame;
    ksge::cameraFrame(transform, camera, frame);
    world_.set<CameraFrame>(frame);
}

}