#pragma once

#include "engine/scene/math.hpp"

namespace ksge {

struct Transform
{
    math::Vec3 position{0.0f, 0.0f, 0.0f};
    DirectX::XMFLOAT4 rotation{0.0f, 0.0f, 0.0f, 1.0f};
    math::Vec3 scale{1.0f, 1.0f, 1.0f};
};

struct MeshRenderer
{
    std::uint32_t meshAsset = ~0u;
};

struct PbrMaterial
{
    DirectX::XMFLOAT4 baseColorFactor{1.0f, 1.0f, 1.0f, 1.0f};
    DirectX::XMFLOAT3 emissiveFactor{0.0f, 0.0f, 0.0f};
    float metallicFactor = 0.0f;
    float roughnessFactor = 0.5f;
    float aoFactor = 1.0f;
    std::int32_t baseColorTexture = -1;
    std::int32_t metallicRoughnessTexture = -1;
    std::int32_t normalTexture = -1;
    std::int32_t occlusionTexture = -1;
    bool doubleSided = false;
};

struct DirectionalLight
{
    math::Vec3 direction{0.3f, -0.8f, -0.5f};
    float intensity = 3.0f;
    DirectX::XMFLOAT3 color{1.0f, 0.95f, 0.85f};
    float ambientStrength = 0.05f;
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