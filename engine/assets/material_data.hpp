#pragma once

#include <cstdint>

#include <DirectXMath.h>

namespace ksge {

struct MaterialData
{
    DirectX::XMFLOAT4 baseColorFactor{1.0f, 1.0f, 1.0f, 1.0f};
    DirectX::XMFLOAT3 emissiveFactor{0.0f, 0.0f, 0.0f};
    float metallicFactor = 1.0f;
    float roughnessFactor = 1.0f;
    float normalScale = 1.0f;
    std::int32_t baseColorTexture = -1;
    std::int32_t metallicRoughnessTexture = -1;
    std::int32_t normalTexture = -1;
    std::int32_t occlusionTexture = -1;
    std::int32_t emissiveTexture = -1;
    bool doubleSided = false;
};

}