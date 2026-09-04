#pragma once

#include "engine/scene/math.hpp"

namespace ksge {

struct Transform
{
    math::Vec3 position{0.0f, 0.0f, 0.0f};
    DirectX::XMFLOAT4 rotation{0.0f, 0.0f, 0.0f, 1.0f};
    math::Vec3 scale{1.0f, 1.0f, 1.0f};
};

struct Camera
{
    float yaw = 0.0f;
    float pitch = 0.0f;
    float fovYDegrees = 60.0f;
    float nearPlane = 0.1f;
    float farPlane = 5000.0f;
    float aspectRatio = 16.0f / 9.0f;
};

struct CameraFrame
{
    math::Mat4 view;
    math::Mat4 projection;
    math::Mat4 viewProjection;
};

struct Frustum
{
    DirectX::XMFLOAT4 planes[6];
};

struct Ray
{
    math::Vec3 origin;
    math::Vec3 direction;
};

}