#include "engine/graphics/mesh_upload.hpp"

#include <algorithm>
#include <cmath>

namespace ksge {

namespace {

constexpr std::size_t kOffsetPosition = 0u;
constexpr std::size_t kOffsetNormal = 12u;
constexpr std::size_t kOffsetUv = 24u;
constexpr std::size_t kOffsetTangent = 32u;

void writeVertexFloat3(std::uint8_t* destination, const float* value)
{
    std::memcpy(destination + kOffsetPosition, value, 12u);
}

void writeNormal(std::uint8_t* destination, const float* value)
{
    std::memcpy(destination + kOffsetNormal, value, 12u);
}

void writeUv(std::uint8_t* destination, const float* value)
{
    std::memcpy(destination + kOffsetUv, value, 8u);
}

void writeTangent(std::uint8_t* destination, const float* value, float sign)
{
    float tangent[4] = {value[0], value[1], value[2], sign};
    std::memcpy(destination + kOffsetTangent, tangent, 16u);
}

float dot3(const float* a, const float* b)
{
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

void cross3(const float* a, const float* b, float* out)
{
    out[0] = a[1] * b[2] - a[2] * b[1];
    out[1] = a[2] * b[0] - a[0] * b[2];
    out[2] = a[0] * b[1] - a[1] * b[0];
}

void normal3(const float* value, float* out)
{
    const float length = std::sqrt(dot3(value, value));
    const float inverse = length > 1.0e-8f ? 1.0f / length : 0.0f;
    out[0] = value[0] * inverse;
    out[1] = value[1] * inverse;
    out[2] = value[2] * inverse;
}

void subtract3(const float* a, const float* b, float* out)
{
    out[0] = a[0] - b[0];
    out[1] = a[1] - b[1];
    out[2] = a[2] - b[2];
}

}

void prepareMesh(const MeshData& source, PreparedMesh& out)
{
    std::uint32_t stride = source.vertexStride;
    const std::uint8_t* vertexData = source.vertices.data();
    const std::uint32_t vertexCount = source.vertexCount;

    const bool hasNormal = (source.vertexMask & VertexNormal) != 0u;
    const bool hasUv = (source.vertexMask & VertexUv) != 0u;
    const bool hasTangent = (source.vertexMask & VertexTangent) != 0u;

    const std::size_t sourceNormalOffset = hasNormal ? 12u : 0u;
    const std::size_t sourceUvOffset = 12u + (hasNormal ? 12u : 0u);

    std::vector<std::uint8_t> unpacked;
    std::vector<float> normals;
    std::vector<float> uvs;
    std::vector<float> tangents;
    std::vector<float> tangentsSign;

    if (!hasNormal || !hasUv || !hasTangent)
    {
        unpacked.resize(static_cast<std::size_t>(vertexCount) * 3u * 4u);
        normals.assign(vertexCount * 3u, 0.0f);
        uvs.assign(vertexCount * 2u, 0.0f);
        tangents.assign(vertexCount * 3u, 0.0f);
        tangentsSign.assign(vertexCount, 1.0f);

        for (std::uint32_t i = 0u; i < vertexCount; ++i)
        {
            std::memcpy(
                unpacked.data() + static_cast<std::size_t>(i) * 12u,
                vertexData + static_cast<std::size_t>(i) * stride,
                12u);
            if (hasNormal)
            {
                std::memcpy(
                    normals.data() + static_cast<std::size_t>(i) * 3u,
                    vertexData + static_cast<std::size_t>(i) * stride + sourceNormalOffset,
                    12u);
            }
            if (hasUv)
            {
                std::memcpy(
                    uvs.data() + static_cast<std::size_t>(i) * 2u,
                    vertexData + static_cast<std::size_t>(i) * stride + sourceUvOffset,
                    8u);
            }
        }

        std::vector<std::uint32_t> geometryIndices;
        if (source.indices.size() >= 3u)
        {
            geometryIndices = source.indices;
        }
        else
        {
            geometryIndices.resize(vertexCount);
            for (std::uint32_t i = 0u; i < vertexCount; ++i)
            {
                geometryIndices[i] = i;
            }
        }

        const std::size_t triangleCount = geometryIndices.size() / 3u;
        for (std::size_t triangle = 0u; triangle < triangleCount; ++triangle)
        {
            const std::uint32_t i0 = geometryIndices[triangle * 3u + 0u];
            const std::uint32_t i1 = geometryIndices[triangle * 3u + 1u];
            const std::uint32_t i2 = geometryIndices[triangle * 3u + 2u];

            const float* p0 = reinterpret_cast<const float*>(unpacked.data() + static_cast<std::size_t>(i0) * 12u);
            const float* p1 = reinterpret_cast<const float*>(unpacked.data() + static_cast<std::size_t>(i1) * 12u);
            const float* p2 = reinterpret_cast<const float*>(unpacked.data() + static_cast<std::size_t>(i2) * 12u);

            float e1[3];
            float e2[3];
            subtract3(p1, p0, e1);
            subtract3(p2, p0, e2);

            float faceNormal[3];
            cross3(e1, e2, faceNormal);
            if (!hasNormal)
            {
                normals[i0 * 3u + 0u] += faceNormal[0];
                normals[i0 * 3u + 1u] += faceNormal[1];
                normals[i0 * 3u + 2u] += faceNormal[2];
                normals[i1 * 3u + 0u] += faceNormal[0];
                normals[i1 * 3u + 1u] += faceNormal[1];
                normals[i1 * 3u + 2u] += faceNormal[2];
                normals[i2 * 3u + 0u] += faceNormal[0];
                normals[i2 * 3u + 1u] += faceNormal[1];
                normals[i2 * 3u + 2u] += faceNormal[2];
            }

            if (!hasTangent)
            {
                float uv0[2] = {0.0f, 0.0f};
                float uv1[2] = {1.0f, 0.0f};
                float uv2[2] = {0.0f, 1.0f};
                if (hasUv)
                {
                    std::memcpy(uv0, uvs.data() + static_cast<std::size_t>(i0) * 2u, 8u);
                    std::memcpy(uv1, uvs.data() + static_cast<std::size_t>(i1) * 2u, 8u);
                    std::memcpy(uv2, uvs.data() + static_cast<std::size_t>(i2) * 2u, 8u);
                }

                float du1[2] = {uv1[0] - uv0[0], uv1[1] - uv0[1]};
                float du2[2] = {uv2[0] - uv0[0], uv2[1] - uv0[1]};
                const float denominator = du1[0] * du2[1] - du1[1] * du2[0];
                if (std::abs(denominator) > 1.0e-8f)
                {
                    float tangent[3];
                    for (int channel = 0; channel < 3; ++channel)
                    {
                        tangent[channel] =
                            (e2[channel] * du1[0] - e1[channel] * du2[0]) / denominator;
                    }
                    tangents[i0 * 3u + 0u] += tangent[0];
                    tangents[i0 * 3u + 1u] += tangent[1];
                    tangents[i0 * 3u + 2u] += tangent[2];
                    tangents[i1 * 3u + 0u] += tangent[0];
                    tangents[i1 * 3u + 1u] += tangent[1];
                    tangents[i1 * 3u + 2u] += tangent[2];
                    tangents[i2 * 3u + 0u] += tangent[0];
                    tangents[i2 * 3u + 1u] += tangent[1];
                    tangents[i2 * 3u + 2u] += tangent[2];
                }
            }
        }
    }

    out.vertexCount = vertexCount;
    out.vertices.resize(static_cast<std::size_t>(vertexCount) * kPreparedStride);
    out.indices = source.indices;

    for (std::uint32_t i = 0u; i < vertexCount; ++i)
    {
        std::uint8_t* destination = out.vertices.data() + static_cast<std::size_t>(i) * kPreparedStride;

        if (!hasNormal || !hasUv || !hasTangent)
        {
            float position[3];
            std::memcpy(position, unpacked.data() + static_cast<std::size_t>(i) * 12u, 12u);
            writeVertexFloat3(destination, position);

            float normal[3];
            if (hasNormal)
            {
                std::memcpy(normal, normals.data() + static_cast<std::size_t>(i) * 3u, 12u);
                normal3(normal, normal);
            }
            else
            {
                normal3(normals.data() + static_cast<std::size_t>(i) * 3u, normal);
            }
            writeNormal(destination, normal);

            float uv[2] = {0.0f, 0.0f};
            if (hasUv)
            {
                std::memcpy(uv, uvs.data() + static_cast<std::size_t>(i) * 2u, 8u);
            }
            writeUv(destination, uv);

            float tangent[3] = {1.0f, 0.0f, 0.0f};
            if (hasTangent)
            {
                std::memcpy(tangent, vertexData + static_cast<std::size_t>(i) * stride + 24u, 12u);
                normal3(tangent, tangent);
            }
            else
            {
                const float* raw = tangents.data() + static_cast<std::size_t>(i) * 3u;
                if (dot3(raw, raw) > 1.0e-8f)
                {
                    normal3(raw, tangent);
                }
            }

            float tangentSigned[3];
            float binormal[3];
            cross3(normal, tangent, binormal);
            cross3(binormal, tangent, tangentSigned);
            const float sign = dot3(tangentSigned, normal) < 0.0f ? -1.0f : 1.0f;
            writeTangent(destination, tangent, sign);
        }
        else
        {
            std::memcpy(destination, vertexData + static_cast<std::size_t>(i) * stride, kPreparedStride);
        }
    }

    out.indexCount = static_cast<std::uint32_t>(out.indices.size());
}

MeshData makeCube(float size)
{
    const float half = size * 0.5f;

    const float faceNormals[6][3] = {
        {1.0f, 0.0f, 0.0f},
        {-1.0f, 0.0f, 0.0f},
        {0.0f, 1.0f, 0.0f},
        {0.0f, -1.0f, 0.0f},
        {0.0f, 0.0f, 1.0f},
        {0.0f, 0.0f, -1.0f},
    };
    const float faceTangents[6][3] = {
        {0.0f, 0.0f, -1.0f},
        {0.0f, 0.0f, -1.0f},
        {1.0f, 0.0f, 0.0f},
        {1.0f, 0.0f, 0.0f},
        {1.0f, 0.0f, 0.0f},
        {-1.0f, 0.0f, 0.0f},
    };
    const float faceBinormals[6][3] = {
        {0.0f, 1.0f, 0.0f},
        {0.0f, -1.0f, 0.0f},
        {0.0f, 0.0f, -1.0f},
        {0.0f, 0.0f, 1.0f},
        {0.0f, 1.0f, 0.0f},
        {0.0f, 1.0f, 0.0f},
    };

    MeshData mesh;
    mesh.vertexCount = 24u;
    mesh.vertexMask = VertexPosition | VertexNormal | VertexUv | VertexTangent;
    mesh.vertexStride = kPreparedStride;
    mesh.vertices.resize(24u * kPreparedStride);

    for (int face = 0; face < 6; ++face)
    {
        for (int corner = 0; corner < 4; ++corner)
        {
            const float signU = (corner & 1u) != 0u ? 1.0f : -1.0f;
            const float signV = (corner & 2u) != 0u ? 1.0f : -1.0f;

            float position[3];
            for (int channel = 0; channel < 3; ++channel)
            {
                position[channel] =
                    faceNormals[face][channel] * half +
                    faceTangents[face][channel] * (half * signU) +
                    faceBinormals[face][channel] * (half * signV);
            }

            const float uv[2] = {signU > 0.0f ? 1.0f : 0.0f, signV > 0.0f ? 1.0f : 0.0f};

            std::uint8_t* vertex =
                mesh.vertices.data() + (static_cast<std::size_t>(face) * 4u + corner) * kPreparedStride;
            writeVertexFloat3(vertex, position);
            writeNormal(vertex, faceNormals[face]);
            writeUv(vertex, uv);
            writeTangent(vertex, faceTangents[face], 1.0f);
        }

        const std::uint32_t base = static_cast<std::uint32_t>(face * 4);
        mesh.indices.push_back(base + 0u);
        mesh.indices.push_back(base + 1u);
        mesh.indices.push_back(base + 3u);
        mesh.indices.push_back(base + 0u);
        mesh.indices.push_back(base + 3u);
        mesh.indices.push_back(base + 2u);
    }

    mesh.boundsMin = {-half, -half, -half};
    mesh.boundsMax = {half, half, half};
    return mesh;
}

MeshData makeSphere(std::uint32_t slices, std::uint32_t stacks, float radius)
{
    MeshData mesh;
    mesh.vertexCount = (slices + 1u) * (stacks + 1u);
    mesh.vertexMask = VertexPosition | VertexNormal | VertexUv | VertexTangent;
    mesh.vertexStride = kPreparedStride;
    mesh.vertices.resize(static_cast<std::size_t>(mesh.vertexCount) * kPreparedStride);
    mesh.boundsMin = {-radius, -radius, -radius};
    mesh.boundsMax = {radius, radius, radius};

    for (std::uint32_t stack = 0u; stack <= stacks; ++stack)
    {
        const float phi = 3.14159265f * static_cast<float>(stack) / static_cast<float>(stacks);
        const float sinPhi = std::sin(phi);
        const float cosPhi = std::cos(phi);

        for (std::uint32_t slice = 0u; slice <= slices; ++slice)
        {
            const float theta = 2.0f * 3.14159265f * static_cast<float>(slice) / static_cast<float>(slices);
            const float sinTheta = std::sin(theta);
            const float cosTheta = std::cos(theta);

            const float position[3] = {
                radius * sinPhi * cosTheta,
                radius * cosPhi,
                radius * sinPhi * sinTheta,
            };
            const float normal[3] = {
                sinPhi * cosTheta,
                cosPhi,
                sinPhi * sinTheta,
            };
            const float uv[2] = {
                static_cast<float>(slice) / static_cast<float>(slices),
                static_cast<float>(stack) / static_cast<float>(stacks),
            };
            const float tangent[3] = {
                -sinTheta,
                0.0f,
                cosTheta,
            };

            const std::uint32_t index = stack * (slices + 1u) + slice;
            std::uint8_t* vertex = mesh.vertices.data() + static_cast<std::size_t>(index) * kPreparedStride;
            writeVertexFloat3(vertex, position);
            writeNormal(vertex, normal);
            writeUv(vertex, uv);
            writeTangent(vertex, tangent, 1.0f);
        }
    }

    for (std::uint32_t stack = 0u; stack < stacks; ++stack)
    {
        for (std::uint32_t slice = 0u; slice < slices; ++slice)
        {
            const std::uint32_t a = stack * (slices + 1u) + slice;
            const std::uint32_t b = a + 1u;
            const std::uint32_t c = a + slices + 1u;
            const std::uint32_t d = c + 1u;

            mesh.indices.push_back(a);
            mesh.indices.push_back(c);
            mesh.indices.push_back(b);
            mesh.indices.push_back(b);
            mesh.indices.push_back(c);
            mesh.indices.push_back(d);
        }
    }

    return mesh;
}

MeshData makeQuad(float width, float depth)
{
    const float halfWidth = width * 0.5f;
    const float halfDepth = depth * 0.5f;

    MeshData mesh;
    mesh.vertexCount = 4u;
    mesh.vertexMask = VertexPosition | VertexNormal | VertexUv | VertexTangent;
    mesh.vertexStride = kPreparedStride;
    mesh.vertices.resize(4u * kPreparedStride);

    const float positions[4][3] = {
        {-halfWidth, 0.0f, -halfDepth},
        {halfWidth, 0.0f, -halfDepth},
        {halfWidth, 0.0f, halfDepth},
        {-halfWidth, 0.0f, halfDepth},
    };
    const float normal[3] = {0.0f, 1.0f, 0.0f};
    const float tangent[3] = {1.0f, 0.0f, 0.0f};
    const float uvs[4][2] = {
        {0.0f, 0.0f},
        {1.0f, 0.0f},
        {1.0f, 1.0f},
        {0.0f, 1.0f},
    };

    for (std::uint32_t corner = 0u; corner < 4u; ++corner)
    {
        std::uint8_t* vertex = mesh.vertices.data() + static_cast<std::size_t>(corner) * kPreparedStride;
        writeVertexFloat3(vertex, positions[corner]);
        writeNormal(vertex, normal);
        writeUv(vertex, uvs[corner]);
        writeTangent(vertex, tangent, 1.0f);
    }

    mesh.indices = {0u, 2u, 1u, 0u, 3u, 2u};
    mesh.boundsMin = {-halfWidth, 0.0f, -halfDepth};
    mesh.boundsMax = {halfWidth, 0.0f, halfDepth};
    return mesh;
}

}