#pragma once

#include <cstdint>
#include <vector>

namespace ksge {

enum class TextureFormat : std::uint32_t
{
    R8G8B8A8Unorm
};

struct TextureData
{
    std::uint32_t width = 0u;
    std::uint32_t height = 0u;
    std::uint32_t mipCount = 1u;
    TextureFormat format = TextureFormat::R8G8B8A8Unorm;
    std::vector<std::uint8_t> pixels;
};

inline std::uint32_t textureBytesPerPixel(TextureFormat format)
{
    return format == TextureFormat::R8G8B8A8Unorm ? 4u : 0u;
}

inline std::uint32_t textureMipDimension(std::uint32_t baseDimension, std::uint32_t mip)
{
    const std::uint32_t shrunk = baseDimension >> mip;
    return shrunk > 0u ? shrunk : 1u;
}

}