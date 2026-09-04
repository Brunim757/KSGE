#pragma once

#include "engine/scene/components.hpp"

namespace ksge {

DirectX::XMMATRIX worldMatrix(const Transform& transform);

void cameraFrame(const Transform& transform, const Camera& camera, CameraFrame& out);

void extractFrustum(const CameraFrame& frame, Frustum& out);

bool intersects(const Frustum& frustum, const math::Vec3& center, const math::Vec3& halfExtents);

void screenToRay(
    const Camera& camera,
    const CameraFrame& frame,
    float mouseX,
    float mouseY,
    float viewportWidth,
    float viewportHeight,
    Ray& out);

bool intersectAabb(const Ray& ray, const math::Vec3& minBounds, const math::Vec3& maxBounds, float& tHit);

}