#pragma once

#include <DirectXMath.h>

namespace ksge {
namespace math {

using Vec2 = DirectX::XMFLOAT2;
using Vec3 = DirectX::XMFLOAT3;
using Mat4 = DirectX::XMFLOAT4X4;

inline DirectX::XMVECTOR load(const Vec3& value)
{
    return DirectX::XMLoadFloat3(&value);
}

inline void store(Vec3& out, DirectX::FXMVECTOR value)
{
    DirectX::XMStoreFloat3(&out, value);
}

inline DirectX::XMMATRIX load(const Mat4& value)
{
    return DirectX::XMLoadFloat4x4(&value);
}

inline void store(Mat4& out, DirectX::FXMMATRIX value)
{
    DirectX::XMStoreFloat4x4(&out, value);
}

inline DirectX::XMMATRIX perspectiveFov(float fovYDegrees, float aspectRatio, float nearPlane, float farPlane)
{
    const float fovY = DirectX::XMConvertToRadians(fovYDegrees);
    return DirectX::XMMatrixPerspectiveFovRH(fovY, aspectRatio, nearPlane, farPlane);
}

inline float radians(float degrees)
{
    return DirectX::XMConvertToRadians(degrees);
}

inline float degrees(float radians)
{
    return DirectX::XMConvertToDegrees(radians);
}

}
}