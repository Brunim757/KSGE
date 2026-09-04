#include "engine/assets/gltf.hpp"

#include <fastgltf/core.hpp>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <limits>

namespace ksge {

namespace {

constexpr float kMaximum = std::numeric_limits<float>::max();
constexpr float kMinimum = -std::numeric_limits<float>::max();

std::size_t componentSize(fastgltf::ComponentType type)
{
    switch (type)
    {
        case fastgltf::ComponentType::Byte:
        case fastgltf::ComponentType::UnsignedByte:
            return 1u;
        case fastgltf::ComponentType::Short:
        case fastgltf::ComponentType::UnsignedShort:
            return 2u;
        case fastgltf::ComponentType::UnsignedInt:
        case fastgltf::ComponentType::Float:
            return 4u;
    }
    return 0u;
}

std::size_t typeElements(fastgltf::AccessorType type)
{
    switch (type)
    {
        case fastgltf::AccessorType::Scalar:
            return 1u;
        case fastgltf::AccessorType::Vec2:
            return 2u;
        case fastgltf::AccessorType::Vec3:
            return 3u;
        case fastgltf::AccessorType::Vec4:
            return 4u;
        case fastgltf::AccessorType::Mat2:
            return 4u;
        case fastgltf::AccessorType::Mat3:
            return 9u;
        case fastgltf::AccessorType::Mat4:
            return 16u;
    }
    return 0u;
}

const std::uint8_t* bufferData(const fastgltf::Asset& asset, std::size_t bufferIndex)
{
    if (bufferIndex >= asset.buffers.size())
    {
        return nullptr;
    }
    const fastgltf::Buffer& buffer = asset.buffers[bufferIndex];
    if (const auto* array = std::get_if<fastgltf::sources::Array>(&buffer.data))
    {
        return reinterpret_cast<const std::uint8_t*>(array->bytes.data());
    }
    if (const auto* vector = std::get_if<std::vector<std::byte>>(&buffer.data))
    {
        return reinterpret_cast<const std::uint8_t*>(vector->data());
    }
    return nullptr;
}

struct AccessorRegion
{
    const std::uint8_t* base = nullptr;
    std::size_t stride = 0u;
    std::size_t elementSize = 0u;
};

bool accessorRegion(
    const fastgltf::Asset& asset,
    const fastgltf::Accessor& accessor,
    AccessorRegion& out)
{
    if (!accessor.bufferView)
    {
        return false;
    }
    const fastgltf::BufferView& view = asset.bufferViews[*accessor.bufferView];
    const std::uint8_t* bytes = bufferData(asset, view.buffer);
    if (!bytes)
    {
        return false;
    }

    const std::size_t elementSize = componentSize(accessor.componentType) * typeElements(accessor.type);
    out.stride = view.byteStride.value_or(elementSize);
    out.elementSize = elementSize;
    out.base = bytes + view.byteOffset + accessor.byteOffset;
    return true;
}

float normalizedValue(const std::uint8_t* bytes, fastgltf::ComponentType type, bool normalized)
{
    switch (type)
    {
        case fastgltf::ComponentType::Float:
        {
            std::uint32_t raw = 0u;
            std::memcpy(&raw, bytes, 4u);
            float value = 0.0f;
            std::memcpy(&value, &raw, 4u);
            return value;
        }
        case fastgltf::ComponentType::UnsignedByte:
            return normalized ? bytes[0] / 255.0f : static_cast<float>(bytes[0]);
        case fastgltf::ComponentType::Byte:
            return normalized ? std::max(reinterpret_cast<const std::int8_t*>(bytes)[0] / 127.0f, -1.0f)
                              : static_cast<float>(reinterpret_cast<const std::int8_t*>(bytes)[0]);
        case fastgltf::ComponentType::UnsignedShort:
        {
            std::uint16_t raw = 0u;
            std::memcpy(&raw, bytes, 2u);
            return normalized ? raw / 65535.0f : static_cast<float>(raw);
        }
        case fastgltf::ComponentType::Short:
        {
            std::int16_t raw = 0u;
            std::memcpy(&raw, bytes, 2u);
            return normalized ? std::max(raw / 32767.0f, -1.0f) : static_cast<float>(raw);
        }
        case fastgltf::ComponentType::UnsignedInt:
        {
            std::uint32_t raw = 0u;
            std::memcpy(&raw, bytes, 4u);
            return static_cast<float>(raw);
        }
    }
    return 0.0f;
}

bool copyAccessorFloats(
    const fastgltf::Asset& asset,
    const fastgltf::Accessor& accessor,
    std::size_t channelCount,
    std::vector<float>& out)
{
    AccessorRegion region;
    if (!accessorRegion(asset, accessor, region))
    {
        return false;
    }

    out.resize(accessor.count * channelCount);
    const std::size_t elements = std::min(channelCount, typeElements(accessor.type));
    for (std::size_t index = 0u; index < accessor.count; ++index)
    {
        const std::uint8_t* element = region.base + index * region.stride;
        std::uint8_t components[4][4] = {};
        for (std::size_t channel = 0u; channel < 4u; ++channel)
        {
            const std::size_t offset =
                std::min(channel, elements - 1u) * componentSize(accessor.componentType);
            std::memset(components[channel], 0, 4u);
            std::memcpy(components[channel], element + offset, componentSize(accessor.componentType));
        }
        for (std::size_t channel = 0u; channel < channelCount; ++channel)
        {
            out[index * channelCount + channel] =
                normalizedValue(components[channel], accessor.componentType, accessor.normalized);
        }
    }
    return true;
}

std::uint32_t readIndex(const std::uint8_t* bytes, fastgltf::ComponentType type)
{
    switch (type)
    {
        case fastgltf::ComponentType::UnsignedShort:
        {
            std::uint16_t value = 0u;
            std::memcpy(&value, bytes, 2u);
            return value;
        }
        case fastgltf::ComponentType::UnsignedByte:
            return bytes[0];
        default:
        {
            std::uint32_t value = 0u;
            std::memcpy(&value, bytes, 4u);
            return value;
        }
    }
}

}

bool loadGltfFile(const std::filesystem::path& path, LoadedAsset& out, std::string& error)
{
    auto dataBuffer = fastgltf::GltfDataBuffer::FromPath(path);
    if (dataBuffer.error())
    {
        error = "failed to read gltf file";
        return false;
    }

    fastgltf::Parser parser;
    const auto options =
        fastgltf::Options::LoadExternalBuffers | fastgltf::Options::LoadExternalImages;
    auto expected = parser.loadGltf(&dataBuffer.get(), fastgltf::Path{path.parent_path().string()}, options);
    if (expected.error())
    {
        error = "failed to parse gltf";
        return false;
    }

    const fastgltf::Asset& asset = expected.get();

    for (const fastgltf::Material& material : asset.materials)
    {
        MaterialData materialData;
        materialData.baseColorFactor = {
            material.pbrData.baseColorFactor[0],
            material.pbrData.baseColorFactor[1],
            material.pbrData.baseColorFactor[2],
            material.pbrData.baseColorFactor[3],
        };
        materialData.metallicFactor = material.pbrData.metallicFactor;
        materialData.roughnessFactor = material.pbrData.roughnessFactor;
        materialData.normalScale = material.normalTexture.has_value()
            ? material.normalTexture->scale
            : 1.0f;
        materialData.emissiveFactor = {
            material.emissiveFactor[0],
            material.emissiveFactor[1],
            material.emissiveFactor[2],
        };
        materialData.doubleSided = material.doubleSided;
        materialData.baseColorTexture = material.pbrData.baseColorTexture.has_value()
            ? static_cast<std::int32_t>(
                  asset.textures[material.pbrData.baseColorTexture->textureIndex].image.value_or(0u))
            : -1;
        materialData.metallicRoughnessTexture = material.pbrData.metallicRoughnessTexture.has_value()
            ? static_cast<std::int32_t>(
                  asset.textures[material.pbrData.metallicRoughnessTexture->textureIndex].image.value_or(0u))
            : -1;
        materialData.normalTexture = material.normalTexture.has_value()
            ? static_cast<std::int32_t>(
                  asset.textures[material.normalTexture->textureIndex].image.value_or(0u))
            : -1;
        materialData.occlusionTexture = material.occlusionTexture.has_value()
            ? static_cast<std::int32_t>(
                  asset.textures[material.occlusionTexture->textureIndex].image.value_or(0u))
            : -1;
        materialData.emissiveTexture = material.emissiveTexture.has_value()
            ? static_cast<std::int32_t>(
                  asset.textures[material.emissiveTexture->textureIndex].image.value_or(0u))
            : -1;
        out.materials.push_back(materialData);
    }

    for (const fastgltf::Mesh& mesh : asset.meshes)
    {
        for (const fastgltf::Primitive& primitive : mesh.primitives)
        {
            MeshData meshData;

            std::vector<float> positions;
            const auto positionIt = primitive.attributes.find("POSITION");
            if (positionIt == primitive.attributes.end())
            {
                continue;
            }
            const fastgltf::Accessor& positionAccessor = asset.accessors[positionIt->second];
            if (!copyAccessorFloats(asset, positionAccessor, 3u, positions))
            {
                continue;
            }
            meshData.vertexCount = static_cast<std::uint32_t>(positionAccessor.count);

            std::vector<float> normals;
            const auto normalIt = primitive.attributes.find("NORMAL");
            const bool hasNormals = normalIt != primitive.attributes.end() &&
                copyAccessorFloats(asset, asset.accessors[normalIt->second], 3u, normals);

            std::vector<float> uvs;
            const auto uvIt = primitive.attributes.find("TEXCOORD_0");
            const bool hasUvs = uvIt != primitive.attributes.end() &&
                copyAccessorFloats(asset, asset.accessors[uvIt->second], 2u, uvs);

            std::vector<float> tangents;
            const auto tangentIt = primitive.attributes.find("TANGENT");
            const bool hasTangents = tangentIt != primitive.attributes.end() &&
                copyAccessorFloats(asset, asset.accessors[tangentIt->second], 4u, tangents);

            std::vector<float> colors;
            const auto colorIt = primitive.attributes.find("COLOR_0");
            const bool hasColors = colorIt != primitive.attributes.end() &&
                copyAccessorFloats(asset, asset.accessors[colorIt->second], 4u, colors);

            std::uint32_t mask = VertexPosition;
            math::Vec3 boundsMin{kMaximum, kMaximum, kMaximum};
            math::Vec3 boundsMax{kMinimum, kMinimum, kMinimum};

            if (hasNormals)
            {
                mask |= VertexNormal;
            }
            if (hasUvs)
            {
                mask |= VertexUv;
            }
            if (hasTangents)
            {
                mask |= VertexTangent;
            }
            if (hasColors)
            {
                mask |= VertexColor;
            }

            const std::uint32_t stride =
                (3u + (hasNormals ? 3u : 0u) + (hasUvs ? 2u : 0u) +
                 (hasTangents ? 4u : 0u) + (hasColors ? 4u : 0u)) * sizeof(float);

            meshData.vertexStride = stride;
            meshData.vertexMask = mask;
            meshData.vertices.resize(static_cast<std::size_t>(meshData.vertexCount) * stride);

            std::size_t componentOffset = 0u;
            const auto writeComponent = [&](const std::vector<float>& values, std::size_t count)
            {
                for (std::uint32_t vertex = 0u; vertex < meshData.vertexCount; ++vertex)
                {
                    std::uint8_t* destination = meshData.vertices.data() +
                        static_cast<std::size_t>(vertex) * stride + componentOffset;
                    for (std::size_t channel = 0u; channel < count; ++channel)
                    {
                        const float value = values[static_cast<std::size_t>(vertex) * count + channel];
                        std::memcpy(destination + channel * sizeof(float), &value, sizeof(float));
                    }
                }
                componentOffset += count * sizeof(float);
            };

            for (std::uint32_t vertex = 0u; vertex < meshData.vertexCount; ++vertex)
            {
                const float* position = &positions[static_cast<std::size_t>(vertex) * 3u];
                boundsMin.x = std::min(boundsMin.x, position[0]);
                boundsMin.y = std::min(boundsMin.y, position[1]);
                boundsMin.z = std::min(boundsMin.z, position[2]);
                boundsMax.x = std::max(boundsMax.x, position[0]);
                boundsMax.y = std::max(boundsMax.y, position[1]);
                boundsMax.z = std::max(boundsMax.z, position[2]);
            }
            meshData.boundsMin = boundsMin;
            meshData.boundsMax = boundsMax;

            writeComponent(positions, 3u);
            if (hasNormals)
            {
                writeComponent(normals, 3u);
            }
            if (hasUvs)
            {
                writeComponent(uvs, 2u);
            }
            if (hasTangents)
            {
                writeComponent(tangents, 4u);
            }
            if (hasColors)
            {
                writeComponent(colors, 4u);
            }

            if (primitive.indices.has_value())
            {
                const fastgltf::Accessor& indexAccessor = asset.accessors[*primitive.indices];
                AccessorRegion region;
                if (accessorRegion(asset, indexAccessor, region))
                {
                    meshData.indices.resize(indexAccessor.count);
                    for (std::size_t index = 0u; index < indexAccessor.count; ++index)
                    {
                        meshData.indices[index] =
                            readIndex(region.base + index * region.stride, indexAccessor.componentType);
                    }
                }
            }
            else
            {
                meshData.indices.resize(meshData.vertexCount);
                for (std::uint32_t index = 0u; index < meshData.vertexCount; ++index)
                {
                    meshData.indices[index] = index;
                }
            }

            out.meshes.push_back(std::move(meshData));
        }
    }

    return true;
}

}