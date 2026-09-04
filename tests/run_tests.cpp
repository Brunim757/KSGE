#include "engine/scene/camera.hpp"
#include "engine/scene/components.hpp"
#include "engine/scene/math.hpp"

#include <DirectXMath.h>
#include <cmath>
#include <cstdio>

namespace {

int failures = 0;

void expect(bool condition, const char* what)
{
    if (!condition)
    {
        std::printf("FAIL: %s\n", what);
        ++failures;
    }
}

#define CHECK(...) expect((__VA_ARGS__), #__VA_ARGS__)

float abs(float value)
{
    return value < 0.0f ? -value : value;
}

void checkNear(float actual, float expected, float tolerance, const char* what)
{
    if (abs(actual - expected) > tolerance)
    {
        std::printf("FAIL: %s (actual=%.5f expected=%.5f)\n", what, actual, expected);
        ++failures;
    }
}

void testWorldMatrix()
{
    ksge::Transform transform;
    transform.position = {1.0f, 2.0f, 3.0f};
    transform.rotation = {0.0f, 0.0f, 0.0f, 1.0f};
    transform.scale = {2.0f, 3.0f, 4.0f};

    const DirectX::XMVECTOR point = DirectX::XMVectorSet(1.0f, 0.0f, 0.0f, 1.0f);
    const DirectX::XMMATRIX matrix = ksge::worldMatrix(transform);
    const DirectX::XMVECTOR result = DirectX::XMVector3Transform(point, matrix);

    DirectX::XMFLOAT3 out;
    DirectX::XMStoreFloat3(&out, result);
    checkNear(out.x, 3.0f, 1.0e-4f, "worldMatrix x");
    checkNear(out.y, 2.0f, 1.0e-4f, "worldMatrix y");
    checkNear(out.z, 3.0f, 1.0e-4f, "worldMatrix z");
}

void testProjectCenter()
{
    ksge::Transform transform;
    transform.position = {0.0f, 0.0f, 0.0f};
    transform.rotation = {0.0f, 0.0f, 0.0f, 1.0f};

    ksge::Camera camera;
    camera.fovYDegrees = 60.0f;
    camera.aspectRatio = 16.0f / 9.0f;
    camera.nearPlane = 0.1f;
    camera.farPlane = 1000.0f;

    ksge::CameraFrame frame;
    ksge::cameraFrame(transform, camera, frame);
    ksge::Frustum frustum;
    ksge::extractFrustum(frame, frustum);

    const ksge::math::Vec3 inFront = {0.0f, 0.0f, 20.0f};
    const ksge::math::Vec3 half = {1.0f, 1.0f, 1.0f};
    CHECK(ksge::intersects(frustum, inFront, half));

    const ksge::math::Vec3 behind = {0.0f, 0.0f, -20.0f};
    CHECK(!ksge::intersects(frustum, behind, half));

    const ksge::math::Vec3 beyondFar = {0.0f, 0.0f, 1500.0f};
    CHECK(!ksge::intersects(frustum, beyondFar, half));

    const DirectX::XMVECTOR point = DirectX::XMLoadFloat3(&ksge::math::Vec3{0.0f, 0.0f, 10.0f});
    const DirectX::XMVECTOR projected = DirectX::XMVector3Project(
        point,
        0.0f,
        0.0f,
        1600.0f,
        900.0f,
        camera.nearPlane,
        camera.farPlane,
        ksge::math::load(frame.projection),
        ksge::math::load(frame.view),
        DirectX::XMMatrixIdentity());

    DirectX::XMFLOAT3 out;
    DirectX::XMStoreFloat3(&out, projected);
    checkNear(out.x, 800.0f, 2.0f, "projected x center");
    checkNear(out.y, 450.0f, 2.0f, "projected y center");
}

void testYawProjection()
{
    const float yaw = ksge::math::radians(90.0f);
    const DirectX::XMVECTOR axis = DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
    const DirectX::XMVECTOR quat = DirectX::XMQuaternionRotationAxis(axis, yaw);
    DirectX::XMFLOAT4 rotation;
    DirectX::XMStoreFloat4(&rotation, quat);

    ksge::Transform transform;
    transform.position = {0.0f, 0.0f, 0.0f};
    transform.rotation = rotation;

    ksge::Camera camera;
    camera.fovYDegrees = 60.0f;
    camera.aspectRatio = 16.0f / 9.0f;
    camera.nearPlane = 0.1f;
    camera.farPlane = 1000.0f;

    ksge::CameraFrame frame;
    ksge::cameraFrame(transform, camera, frame);

    const DirectX::XMVECTOR point = DirectX::XMLoadFloat3(&ksge::math::Vec3{10.0f, 0.0f, 0.0f});
    const DirectX::XMVECTOR projected = DirectX::XMVector3Project(
        point,
        0.0f,
        0.0f,
        1600.0f,
        900.0f,
        camera.nearPlane,
        camera.farPlane,
        ksge::math::load(frame.projection),
        ksge::math::load(frame.view),
        DirectX::XMMatrixIdentity());

    DirectX::XMFLOAT3 out;
    DirectX::XMStoreFloat3(&out, projected);
    checkNear(out.x, 800.0f, 2.0f, "yaw 90 projected x center");
    checkNear(out.y, 450.0f, 2.0f, "yaw 90 projected y center");
}

void testScreenToRay()
{
    ksge::Transform transform;
    transform.position = {0.0f, 0.0f, 0.0f};
    transform.rotation = {0.0f, 0.0f, 0.0f, 1.0f};

    ksge::Camera camera;
    ksge::CameraFrame frame;
    ksge::cameraFrame(transform, camera, frame);

    ksge::Ray ray;
    ksge::screenToRay(camera, frame, 800.0f, 450.0f, 1600.0f, 900.0f, ray);

    CHECK(abs(ray.origin.x) < 1.0e-3f);
    CHECK(abs(ray.origin.y) < 1.0e-3f);
    CHECK(abs(ray.origin.z) < 1.0e-3f);
    checkNear(ray.direction.z, 1.0f, 1.0e-3f, "ray forward z");
    CHECK(abs(ray.direction.x) < 1.0e-3f);
    CHECK(abs(ray.direction.y) < 1.0e-3f);
}

void testRayAabb()
{
    ksge::Ray hit;
    hit.origin = {0.0f, 0.0f, 0.0f};
    hit.direction = {0.0f, 0.0f, 1.0f};

    float t = 0.0f;
    CHECK(ksge::intersectAabb(
        hit, ksge::math::Vec3{0.5f, 0.5f, 5.0f}, ksge::math::Vec3{1.5f, 1.5f, 6.0f}, t));
    checkNear(t, 5.0f, 1.0e-3f, "ray aabb hit t");

    float missT = 0.0f;
    CHECK(!ksge::intersectAabb(
        hit,
        ksge::math::Vec3{2.0f, 2.0f, 2.0f},
        ksge::math::Vec3{3.0f, 3.0f, 3.0f},
        missT));

    ksge::Ray side;
    side.origin = {0.0f, 0.0f, 0.0f};
    side.direction = {1.0f, 0.0f, 0.0f};
    CHECK(!ksge::intersectAabb(
        side,
        ksge::math::Vec3{0.5f, 0.5f, 5.0f},
        ksge::math::Vec3{1.5f, 1.5f, 6.0f},
        missT));

    ksge::Ray inside;
    inside.origin = {0.0f, 0.0f, 0.0f};
    inside.direction = {0.0f, 0.0f, 1.0f};
    float insideT = 0.0f;
    CHECK(ksge::intersectAabb(
        inside,
        ksge::math::Vec3{-1.0f, -1.0f, -1.0f},
        ksge::math::Vec3{1.0f, 1.0f, 1.0f},
        insideT));
    checkNear(insideT, 0.0f, 1.0e-4f, "ray inside aabb t");
}

}

int main()
{
    testWorldMatrix();
    testProjectCenter();
    testYawProjection();
    testScreenToRay();
    testRayAabb();

    if (failures == 0)
    {
        std::printf("KSGE tests: all passed\n");
        return 0;
    }

    std::printf("KSGE tests: %d failure(s)\n", failures);
    return 1;
}