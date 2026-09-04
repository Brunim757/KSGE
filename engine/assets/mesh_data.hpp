#pragma once

#include <cstdint>
#include <vector>

#include "engine/scene/math.hpp"

namespace ksge {

enum VertexMask : std::uint32_t
{
    VertexPosition = 1u,
    VertexNormal = 2u,
    VertexUv = 4u,
    VertexTangent = 8u,
    VertexColor = 16u,
};

struct MeshData
{
    std::uint32_t vertexCount = 0u;
    std::uint32_t vertexStride = 0u;
    std::uint32_t vertexMask = 0u;
    std::vector<std::uint8_t> vertices;
    std::vector<std::uint32_t> indices;
    math::Vec3 boundsMin{0.0f, 0.0f, 0.0f};
    math::Vec3 boundsMax{0.0f, 0.0f, 0.0f};
};

}