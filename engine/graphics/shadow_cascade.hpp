#pragma once

#include <cstdint>

#include <DirectXMath.h>

#include "engine/scene/components.hpp"

namespace ksge {

constexpr std::uint32_t kShadowCascades = 3u;

void computeCascadeSplits(float nearPlane, float farPlane, float splitsOut[kShadowCascades + 1u]);

void computeCascadeMatrices(
    const CameraFrame& frame,
    const DirectX::XMFLOAT3& lightDirection,
    float nearPlane,
    float farPlane,
    const float splits[kShadowCascades + 1u],
    DirectX::XMFLOAT4X4 viewProjectionOut[kShadowCascades]);

}