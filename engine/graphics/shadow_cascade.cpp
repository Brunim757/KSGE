#include "engine/graphics/shadow_cascade.hpp"

#include <algorithm>
#include <cmath>

namespace ksge {

void computeCascadeSplits(float nearPlane, float farPlane, float splitsOut[kShadowCascades + 1u])
{
    splitsOut[0] = nearPlane;
    for (std::uint32_t cascade = 1u; cascade <= kShadowCascades; ++cascade)
    {
        const float t = static_cast<float>(cascade) / static_cast<float>(kShadowCascades);
        splitsOut[cascade] = nearPlane * std::pow(farPlane / nearPlane, t);
    }
    splitsOut[kShadowCascades] = std::max(splitsOut[kShadowCascades], nearPlane * 1.01f);
}

namespace {

void cascadeCorners(
    const DirectX::XMMATRIX& inverseView,
    float tanHalfFovY,
    float aspectRatio,
    float nearDepth,
    float farDepth,
    DirectX::XMVECTOR* cornersOut)
{
    const float nearHalfY = nearDepth * tanHalfFovY;
    const float nearHalfX = nearHalfY * aspectRatio;
    const float farHalfY = farDepth * tanHalfFovY;
    const float farHalfX = farHalfY * aspectRatio;

    const DirectX::XMVECTOR nearCorners[4] = {
        DirectX::XMVectorSet(-nearHalfX, -nearHalfY, -nearDepth, 1.0f),
        DirectX::XMVectorSet(nearHalfX, -nearHalfY, -nearDepth, 1.0f),
        DirectX::XMVectorSet(nearHalfX, nearHalfY, -nearDepth, 1.0f),
        DirectX::XMVectorSet(-nearHalfX, nearHalfY, -nearDepth, 1.0f),
    };
    const DirectX::XMVECTOR farCorners[4] = {
        DirectX::XMVectorSet(-farHalfX, -farHalfY, -farDepth, 1.0f),
        DirectX::XMVectorSet(farHalfX, -farHalfY, -farDepth, 1.0f),
        DirectX::XMVectorSet(farHalfX, farHalfY, -farDepth, 1.0f),
        DirectX::XMVectorSet(-farHalfX, farHalfY, -farDepth, 1.0f),
    };
    for (std::uint32_t i = 0u; i < 4u; ++i)
    {
        cornersOut[i] = DirectX::XMVector3Transform(nearCorners[i], inverseView);
        cornersOut[i + 4u] = DirectX::XMVector3Transform(farCorners[i], inverseView);
    }
}

}

void computeCascadeMatrices(
    const CameraFrame& frame,
    const DirectX::XMFLOAT3& lightDirection,
    float nearPlane,
    float farPlane,
    const float splits[kShadowCascades + 1u],
    DirectX::XMFLOAT4X4 viewProjectionOut[kShadowCascades])
{
    (void)nearPlane;
    (void)farPlane;

    const DirectX::XMMATRIX viewMatrix = math::load(frame.view);
    DirectX::XMVECTOR determinant = DirectX::XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f);
    const DirectX::XMMATRIX inverseView = DirectX::XMMatrixInverse(&determinant, viewMatrix);
    const DirectX::XMMATRIX projection = math::load(frame.projection);
    DirectX::XMFLOAT4X4 projectionStored;
    DirectX::XMStoreFloat4x4(&projectionStored, projection);
    const float cotFovYHalf = projectionStored._22;
    const float cotFovXHalf = projectionStored._11;
    const float tanHalfFovY = 1.0f / cotFovYHalf;
    const float aspectRatio = cotFovYHalf / cotFovXHalf;

    DirectX::XMVECTOR lightDir = DirectX::XMLoadFloat3(&lightDirection);
    lightDir = DirectX::XMVector3Normalize(lightDir);
    DirectX::XMVECTOR lightUp = DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
    if (std::abs(DirectX::XMVectorGetX(DirectX::XMVector3Dot(lightDir, lightUp))) > 0.99f)
    {
        lightUp = DirectX::XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
    }

    for (std::uint32_t cascade = 0u; cascade < kShadowCascades; ++cascade)
    {
        DirectX::XMVECTOR corners[8];
        cascadeCorners(inverseView, tanHalfFovY, aspectRatio, splits[cascade], splits[cascade + 1u], corners);

        DirectX::XMVECTOR center = DirectX::XMVectorZero();
        for (std::uint32_t i = 0u; i < 8u; ++i)
        {
            center = DirectX::XMVectorAdd(center, corners[i]);
        }
        center = DirectX::XMVectorScale(center, 0.125f);

        DirectX::XMVECTOR extent = DirectX::XMVectorZero();
        for (std::uint32_t i = 0u; i < 8u; ++i)
        {
            const DirectX::XMVECTOR offset = DirectX::XMVectorSubtract(corners[i], center);
            extent = DirectX::XMVectorMax(extent, DirectX::XMVector3Length(offset));
        }
        const float radius = DirectX::XMVectorGetX(extent);
        const float halfSide = std::max(radius, 0.5f);

        const DirectX::XMVECTOR eye = DirectX::XMVectorSubtract(
            center, DirectX::XMVectorScale(lightDir, halfSide * 3.0f));
        const DirectX::XMMATRIX lightView = DirectX::XMMatrixLookAtRH(eye, center, lightUp);
        const DirectX::XMMATRIX lightProjection =
            DirectX::XMMatrixOrthographicOffCenterRH(-halfSide, halfSide, -halfSide, halfSide, 0.0f, halfSide * 6.0f);

        const DirectX::XMMATRIX combined = DirectX::XMMatrixMultiply(lightView, lightProjection);
        DirectX::XMStoreFloat4x4(&viewProjectionOut[cascade], combined);
    }
}

}