#include "engine/assets/registry.hpp"

#include "engine/assets/gltf.hpp"
#include "engine/assets/texture.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>

namespace ksge {

namespace {

bool readFileBytes(const std::filesystem::path& path, std::vector<std::uint8_t>& bytes)
{
    std::ifstream stream(path, std::ios::binary);
    if (!stream)
    {
        return false;
    }
    stream.seekg(0, std::ios::end);
    const std::streamoff length = stream.tellg();
    stream.seekg(0, std::ios::beg);
    if (length < 0)
    {
        return false;
    }
    bytes.resize(static_cast<std::size_t>(length));
    stream.read(reinterpret_cast<char*>(bytes.data()), length);
    return stream.good() || bytes.empty();
}

}

std::uint32_t AssetRegistry::loadMesh(const std::filesystem::path& path, std::uint32_t meshIndex)
{
    const std::string key = keyFor(path, meshIndex);
    const auto cached = meshIndexByKey_.find(key);
    if (cached != meshIndexByKey_.end())
    {
        return cached->second;
    }

    LoadedAsset asset;
    std::string error;
    if (!loadGltfFile(path, asset, error))
    {
        lastError_ = error.empty() ? "gltf load failed" : error;
        return kInvalidIndex;
    }
    if (meshIndex >= asset.meshes.size())
    {
        lastError_ = "mesh index out of range";
        return kInvalidIndex;
    }

    std::uint32_t index = static_cast<std::uint32_t>(meshes_.size());
    meshes_.push_back(std::move(asset.meshes[meshIndex]));
    meshIndexByKey_.emplace(key, index);
    lastError_.clear();
    return index;
}

std::uint32_t AssetRegistry::loadTexture(const std::filesystem::path& path, std::uint32_t maxMips)
{
    const std::string key = keyFor(path, maxMips);
    const auto cached = textureIndexByKey_.find(key);
    if (cached != textureIndexByKey_.end())
    {
        return cached->second;
    }

    std::vector<std::uint8_t> fileBytes;
    if (!readFileBytes(path, fileBytes))
    {
        lastError_ = "texture file read failed";
        return kInvalidIndex;
    }

    TextureData texture;
    std::string error;
    if (!decodeTexture(fileBytes, texture, error))
    {
        lastError_ = error.empty() ? "texture decode failed" : error;
        return kInvalidIndex;
    }

    generateMipChain(texture, maxMips);

    std::uint32_t index = static_cast<std::uint32_t>(textures_.size());
    textures_.push_back(std::move(texture));
    textureIndexByKey_.emplace(key, index);
    lastError_.clear();
    return index;
}

std::uint32_t AssetRegistry::loadMaterial(const std::filesystem::path& path, std::uint32_t materialIndex)
{
    const std::string key = keyFor(path, materialIndex);
    const auto cached = materialIndexByKey_.find(key);
    if (cached != materialIndexByKey_.end())
    {
        return cached->second;
    }

    LoadedAsset asset;
    std::string error;
    if (!loadGltfFile(path, asset, error))
    {
        lastError_ = error.empty() ? "gltf load failed" : error;
        return kInvalidIndex;
    }
    if (materialIndex >= asset.materials.size())
    {
        lastError_ = "material index out of range";
        return kInvalidIndex;
    }

    std::uint32_t index = static_cast<std::uint32_t>(materials_.size());
    materials_.push_back(asset.materials[materialIndex]);
    materialIndexByKey_.emplace(key, index);
    lastError_.clear();
    return index;
}

const MeshData& AssetRegistry::mesh(std::uint32_t index) const
{
    return meshes_[index];
}

const TextureData& AssetRegistry::texture(std::uint32_t index) const
{
    return textures_[index];
}

const MaterialData& AssetRegistry::material(std::uint32_t index) const
{
    return materials_[index];
}

std::uint32_t AssetRegistry::meshCount() const
{
    return static_cast<std::uint32_t>(meshes_.size());
}

std::uint32_t AssetRegistry::textureCount() const
{
    return static_cast<std::uint32_t>(textures_.size());
}

std::uint32_t AssetRegistry::materialCount() const
{
    return static_cast<std::uint32_t>(materials_.size());
}

const std::string& AssetRegistry::lastError() const
{
    return lastError_;
}

std::string AssetRegistry::keyFor(const std::filesystem::path& path, std::uint32_t variant) const
{
    std::string key = std::filesystem::weakly_canonical(path).generic_string();
    for (char& character : key)
    {
        character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
    }
    key += "/" + std::to_string(variant);
    return key;
}

}