#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "engine/assets/texture_data.hpp"

namespace ksge {

bool decodeTexture(const std::vector<std::uint8_t>& fileBytes, TextureData& out, std::string& error);

void generateMipChain(TextureData& out, std::uint32_t maxMips);

}