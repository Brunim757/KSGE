#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "engine/assets/material_data.hpp"
#include "engine/assets/mesh_data.hpp"

namespace ksge {

struct LoadedAsset
{
    std::vector<MeshData> meshes;
    std::vector<MaterialData> materials;
};

bool loadGltfFile(const std::filesystem::path& path, LoadedAsset& out, std::string& error);

}