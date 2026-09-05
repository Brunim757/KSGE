#pragma once

#include <cstdint>
#include <vector>

#include "engine/assets/mesh_data.hpp"

namespace ksge {

constexpr std::uint32_t kPreparedStride = 48u;

struct PreparedMesh
{
    std::uint32_t vertexCount = 0u;
    std::uint32_t indexCount = 0u;
    std::vector<std::uint8_t> vertices;
    std::vector<std::uint32_t> indices;
};

void prepareMesh(const MeshData& source, PreparedMesh& out);

MeshData makeCube(float size = 1.0f);
MeshData makeSphere(std::uint32_t slices, std::uint32_t stacks, float radius = 0.5f);

}