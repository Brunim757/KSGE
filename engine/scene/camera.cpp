#include "engine/scene/camera.hpp"

#include <cmath>
#include <cstddef>
#include <limits>

namespace ksge {

namespace {

constexpr float kEpsilon = 1.0e-6f;

}

DirectX::XMMATRIX worldMatrix(const Transform& transform)
{
    const DirectX::XMMATRIX translation =
        DirectX::XMMatrixTranslationFromVector(math::load(transform.position));
    const DirectX::XMMATRIX rotation =
        DirectX::XMMatrixRotationQuaternion(DirectX::XMLoadFloat4(&transform.rotation));
    const DirectX::XMMATRIX scaling =
        DirectX::XMMatrixScalingFromVector(math::load(transform.scale));
    return scaling * rotation * translation;
}

void cameraFrame(const Transform& transform, const Camera& camera, CameraFrame& out)
{
    const DirectX::XMVECTOR eye = math::load(transform.position);
    const DirectX::XMVECTOR quat = DirectX::XMLoadFloat4(&transform.rotation);
    const DirectX::XMVECTOR forward =
        DirectX::XMVector3Rotate(DirectX::XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f), quat);
    const DirectX::XMVECTOR worldUp = DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
    const DirectX::XMVECTOR right =
        DirectX::XMVector3Normalize(DirectX::XMVector3Cross(worldUp, forward));
    const DirectX::XMVECTOR up =
        DirectX::XMVector3Normalize(DirectX::XMVector3Cross(forward, right));

    const DirectX::XMMATRIX view = DirectX::XMMatrixLookToRH(eye, forward, up);
    const DirectX::XMMATRIX projection =
        math::perspectiveFov(camera.fovYDegrees, camera.aspectRatio, camera.nearPlane, camera.farPlane);

    math::store(out.view, view);
    math::store(out.projection, projection);
    math::store(out.viewProjection, view * projection);
}

void extractFrustum(const CameraFrame& frame, Frustum& out)
{
    const math::Mat4& m = frame.viewProjection;

    const DirectX::XMFLOAT4 columns[4] = {
        {m._11, m._21, m._31, m._41},
        {m._12, m._22, m._32, m._42},
        {m._13, m._23, m._33, m._43},
        {m._14, m._24, m._34, m._44},
    };

    const DirectX::XMFLOAT4 raw[6] = {
        {columns[3].x + columns[0].x, columns[3].y + columns[0].y, columns[3].z + columns[0].z, columns[3].w + columns[0].w},
        {columns[3].x - columns[0].x, columns[3].y - columns[0].y, columns[3].z - columns[0].z, columns[3].w - columns[0].w},
        {columns[3].x + columns[1].x, columns[3].y + columns[1].y, columns[3].z + columns[1].z, columns[3].w + columns[1].w},
        {columns[3].x - columns[1].x, columns[3].y - columns[1].y, columns[3].z - columns[1].z, columns[3].w - columns[1].w},
        {columns[2].x, columns[2].y, columns[2].z, columns[2].w},
        {columns[3].x - columns[2].x, columns[3].y - columns[2].y, columns[3].z - columns[2].z, columns[3].w - columns[2].w},
    };

    for (std::size_t i = 0; i < 6; ++i)
    {
        const float invLength =
            1.0f / std::sqrt(raw[i].x * raw[i].x + raw[i].y * raw[i].y + raw[i].z * raw[i].z);
        out.planes[i] = {
            raw[i].x * invLength,
            raw[i].y * invLength,
            raw[i].z * invLength,
            raw[i].w * invLength,
        };
    }
}

bool intersects(const Frustum& frustum, const math::Vec3& center, const math::Vec3& halfExtents)
{
    for (const DirectX::XMFLOAT4& plane : frustum.planes)
    {
        const float distance =
            plane.x * center.x + plane.y * center.y + plane.z * center.z + plane.w;
        const float extent =
            std::abs(plane.x) * halfExtents.x +
            std::abs(plane.y) * halfExtents.y +
            std::abs(plane.z) * halfExtents.z;
        if (distance + extent < 0.0f)
        {
            return false;
        }
    }
    return true;
}

void screenToRay(
    const Camera& camera,
    const CameraFrame& frame,
    float mouseX,
    float mouseY,
    float viewportWidth,
    float viewportHeight,
    Ray& out)
{
    const DirectX::XMMATRIX view = math::load(frame.view);
    const DirectX::XMMATRIX projection = math::load(frame.projection);
    const DirectX::XMMATRIX world = DirectX::XMMatrixIdentity();

    const DirectX::XMVECTOR nearPoint = DirectX::XMVector3Unproject(
        DirectX::XMVectorSet(mouseX, mouseY, 0.0f, 0.0f),
        0.0f,
        0.0f,
        viewportWidth,
        viewportHeight,
        camera.nearPlane,
        camera.farPlane,
        projection,
        view,
        world);

    const DirectX::XMVECTOR farPoint = DirectX::XMVector3Unproject(
        DirectX::XMVectorSet(mouseX, mouseY, 1.0f, 0.0f),
        0.0f,
        0.0f,
        viewportWidth,
        viewportHeight,
        camera.nearPlane,
        camera.farPlane,
        projection,
        view,
        world);

    math::store(out.origin, nearPoint);
    const DirectX::XMVECTOR direction =
        DirectX::XMVector3Normalize(DirectX::XMVectorSubtract(farPoint, nearPoint));
    DirectX::XMStoreFloat3(&out.direction, direction);
}

bool intersectAabb(const Ray& ray, const math::Vec3& minBounds, const math::Vec3& maxBounds, float& tHit)
{
    float tNear = -std::numeric_limits<float>::infinity();
    float tFar = std::numeric_limits<float>::infinity();

    const float position[3] = {ray.origin.x, ray.origin.y, ray.origin.z};
    const float direction[3] = {ray.direction.x, ray.direction.y, ray.direction.z};
    const float minBoundsArray[3] = {minBounds.x, minBounds.y, minBounds.z};
    const float maxBoundsArray[3] = {maxBounds.x, maxBounds.y, maxBounds.z};

    for (int axis = 0; axis < 3; ++axis)
    {
        if (std::abs(direction[axis]) < kEpsilon)
        {
            if (position[axis] < minBoundsArray[axis] || position[axis] > maxBoundsArray[axis])
            {
                return false;
            }
            continue;
        }

        const float inverse = 1.0f / direction[axis];
        float t1 = (minBoundsArray[axis] - position[axis]) * inverse;
        float t2 = (maxBoundsArray[axis] - position[axis]) * inverse;
        if (t1 > t2)
        {
            const float swap = t1;
            t1 = t2;
            t2 = swap;
        }
        tNear = tNear > t1 ? tNear : t1;
        tFar = tFar < t2 ? tFar : t2;
        if (tNear > tFar)
        {
            return false;
        }
    }

    if (tFar < 0.0f)
    {
        return false;
    }

    tHit = tNear > 0.0f ? tNear : 0.0f;
    return true;
}

}