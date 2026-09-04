#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

#include "engine/assets/material_data.hpp"
#include "engine/assets/mesh_data.hpp"
#include "engine/assets/texture_data.hpp"

namespace ksge {

class AssetRegistry
{
public:
    static constexpr std::uint32_t kInvalidIndex = ~0u;

    std::uint32_t loadMesh(const std::filesystem::path& path, std::uint32_t meshIndex = 0u);
    std::uint32_t loadTexture(const std::filesystem::path& path, std::uint32_t maxMips = 0u);
    std::uint32_t loadMaterial(const std::filesystem::path& path, std::uint32_t materialIndex = 0u);

    const MeshData& mesh(std::uint32_t index) const;
    const TextureData& texture(std::uint32_t index) const;
    const MaterialData& material(std::uint32_t index) const;

    std::uint32_t meshCount() const;
    std::uint32_t textureCount() const;
    std::uint32_t materialCount() const;

    const std::string& lastError() const;

private:
    std::string keyFor(const std::filesystem::path& path, std::uint32_t variant) const;

    std::vector<MeshData> meshes_;
    std::vector<TextureData> textures_;
    std::vector<MaterialData> materials_;
    std::unordered_map<std::string, std::uint32_t> meshIndexByKey_;
    std::unordered_map<std::string, std::uint32_t> textureIndexByKey_;
    std::unordered_map<std::string, std::uint32_t> materialIndexByKey_;
    std::string lastError_;
};

}