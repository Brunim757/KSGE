#include <windows.h>

#include "engine/assets/texture.hpp"

#include <wincodec.h>
#include <cstring>

namespace ksge {

namespace {

constexpr std::uint32_t kFourCCDXT1 = 0x31545844u;
constexpr std::uint32_t kFourCCDXT3 = 0x33545844u;
constexpr std::uint32_t kFourCCDXT5 = 0x35545844u;
constexpr std::uint32_t kDdpfFourCC = 0x00000004u;
constexpr std::uint32_t kDdpfRgb = 0x00000040u;

std::uint16_t readU16(const std::uint8_t* bytes)
{
    return static_cast<std::uint16_t>(bytes[0]) |
           (static_cast<std::uint16_t>(bytes[1]) << 8u);
}

std::uint32_t readU32(const std::uint8_t* bytes)
{
    return static_cast<std::uint32_t>(bytes[0]) |
           (static_cast<std::uint32_t>(bytes[1]) << 8u) |
           (static_cast<std::uint32_t>(bytes[2]) << 16u) |
           (static_cast<std::uint32_t>(bytes[3]) << 24u);
}

void pixel565(const std::uint8_t* block, std::uint8_t* out, std::uint8_t alpha)
{
    const std::uint16_t value = readU16(block);
    out[0] = static_cast<std::uint8_t>((((value & 0xF800u) >> 11u) * 255u) / 31u);
    out[1] = static_cast<std::uint8_t>((((value & 0x07E0u) >> 5u) * 255u) / 63u);
    out[2] = static_cast<std::uint8_t>(((value & 0x001Fu) * 255u) / 31u);
    out[3] = alpha;
}

void decodeBc1Colors(
    const std::uint8_t* source,
    std::uint8_t colors[4][4],
    bool interpolateAlways)
{
    std::uint8_t color0[4];
    std::uint8_t color1[4];
    pixel565(source, color0, 255u);
    pixel565(source + 2, color1, 255u);

    const bool fourColors = interpolateAlways || readU16(source) > readU16(source + 2);
    if (fourColors)
    {
        for (int channel = 0; channel < 3; ++channel)
        {
            colors[0][channel] = color0[channel];
            colors[1][channel] = color1[channel];
            colors[2][channel] = static_cast<std::uint8_t>(
                (2u * color0[channel] + color1[channel]) / 3u);
            colors[3][channel] = static_cast<std::uint8_t>(
                (color0[channel] + 2u * color1[channel]) / 3u);
        }
        for (int index = 0; index < 4; ++index)
        {
            colors[index][3] = 255u;
        }
    }
    else
    {
        for (int channel = 0; channel < 3; ++channel)
        {
            colors[0][channel] = color0[channel];
            colors[1][channel] = color1[channel];
            colors[2][channel] = static_cast<std::uint8_t>(
                (color0[channel] + color1[channel]) / 2u);
            colors[3][channel] = 0u;
        }
        colors[0][3] = 255u;
        colors[1][3] = 255u;
        colors[2][3] = 255u;
    }
}

void decodeBc1Block(const std::uint8_t* source, std::uint8_t* destination)
{
    std::uint8_t colors[4][4];
    decodeBc1Colors(source, colors, false);

    const std::uint32_t indices = readU32(source + 4);
    for (std::uint32_t i = 0u; i < 16u; ++i)
    {
        const std::uint32_t selection = (indices >> (i * 2u)) & 3u;
        const std::uint8_t* color = colors[selection];
        std::uint8_t* pixel = destination + i * 4u;
        pixel[0] = color[0];
        pixel[1] = color[1];
        pixel[2] = color[2];
        pixel[3] = color[3];
    }
}

void decodeBc2Block(const std::uint8_t* source, std::uint8_t* destination)
{
    std::uint8_t colors[4][4];
    decodeBc1Colors(source + 8, colors, true);

    const std::uint32_t indices = readU32(source + 12);
    for (std::uint32_t i = 0u; i < 16u; ++i)
    {
        const std::uint8_t alphaSelector = source[i / 2u];
        const std::uint32_t alphaShift = (i & 1u) * 4u;
        const std::uint8_t alpha = static_cast<std::uint8_t>(
            ((alphaSelector >> alphaShift) & 0xFu) * 17u);
        const std::uint32_t selection = (indices >> (i * 2u)) & 3u;
        const std::uint8_t* color = colors[selection];
        std::uint8_t* pixel = destination + i * 4u;
        pixel[0] = color[0];
        pixel[1] = color[1];
        pixel[2] = color[2];
        pixel[3] = alpha;
    }
}

void decodeBc3AlphaBlock(const std::uint8_t* source, std::uint8_t alphas[8], std::uint64_t& indices)
{
    const std::uint8_t alpha0 = source[0];
    const std::uint8_t alpha1 = source[1];

    if (alpha0 > alpha1)
    {
        alphas[0] = alpha0;
        alphas[1] = alpha1;
        const int weightsA[6] = {2, 1, 4, 3, 2, 1};
        const int weightsB[6] = {1, 2, 1, 2, 3, 4};
        const int divisors[6] = {3, 3, 5, 5, 5, 5};
        for (int index = 0; index < 6; ++index)
        {
            alphas[index + 2] = static_cast<std::uint8_t>(
                (weightsA[index] * alpha0 + weightsB[index] * alpha1) / divisors[index]);
        }
    }
    else
    {
        alphas[0] = alpha0;
        alphas[1] = alpha1;
        const int weightsA[4] = {4, 3, 2, 1};
        const int weightsB[4] = {1, 2, 3, 4};
        for (int index = 0; index < 4; ++index)
        {
            alphas[index + 2] = static_cast<std::uint8_t>(
                (weightsA[index] * alpha0 + weightsB[index] * alpha1) / 5);
        }
        alphas[6] = 0u;
        alphas[7] = 255u;
    }

    indices = static_cast<std::uint64_t>(source[2]) |
              (static_cast<std::uint64_t>(source[3]) << 8u) |
              (static_cast<std::uint64_t>(source[4]) << 16u) |
              (static_cast<std::uint64_t>(source[5]) << 24u) |
              (static_cast<std::uint64_t>(source[6]) << 32u) |
              (static_cast<std::uint64_t>(source[7]) << 40u);
}

void decodeBc3Block(const std::uint8_t* source, std::uint8_t* destination)
{
    std::uint8_t alphas[8];
    std::uint64_t alphaIndices = 0u;
    decodeBc3AlphaBlock(source, alphas, alphaIndices);

    std::uint8_t colors[4][4];
    decodeBc1Colors(source + 8, colors, true);

    const std::uint32_t colorIndices = readU32(source + 12);
    for (std::uint32_t i = 0u; i < 16u; ++i)
    {
        const std::uint32_t alphaSelection = (alphaIndices >> (i * 3u)) & 7u;
        const std::uint32_t colorSelection = (colorIndices >> (i * 2u)) & 3u;
        const std::uint8_t* color = colors[colorSelection];
        std::uint8_t* pixel = destination + i * 4u;
        pixel[0] = color[0];
        pixel[1] = color[1];
        pixel[2] = color[2];
        pixel[3] = alphas[alphaSelection];
    }
}

bool ensureComInitialized()
{
    static bool initialized = false;
    if (!initialized)
    {
        const HRESULT result = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
        initialized = SUCCEEDED(result) || result == RPC_E_CHANGED_MODE;
    }
    return initialized;
}

bool decodePng(const std::uint8_t* bytes, std::size_t size, TextureData& out, std::string& error)
{
    if (!ensureComInitialized())
    {
        error = "COM init failed";
        return false;
    }

    IWICImagingFactory* factory = nullptr;
    if (FAILED(CoCreateInstance(
            CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory))))
    {
        error = "WIC factory creation failed";
        return false;
    }

    bool success = false;
    IStream* stream = nullptr;
    IWICBitmapDecoder* decoder = nullptr;
    IWICBitmapFrameDecode* frame = nullptr;
    IWICFormatConverter* converter = nullptr;

    if (SUCCEEDED(CreateStreamOnHGlobal(nullptr, TRUE, &stream)) &&
        SUCCEEDED(stream->Write(bytes, static_cast<ULONG>(size), nullptr)) &&
        SUCCEEDED(stream->Seek({0}, STREAM_SEEK_SET, nullptr)) &&
        SUCCEEDED(factory->CreateDecoderFromStream(
            stream, nullptr, WICDecodeMetadataCacheOnDemand, &decoder)) &&
        SUCCEEDED(decoder->GetFrame(0, &frame)))
    {
        UINT width = 0u;
        UINT height = 0u;
        if (SUCCEEDED(frame->GetSize(&width, &height)) &&
            SUCCEEDED(factory->CreateFormatConverter(&converter)) &&
            SUCCEEDED(converter->Initialize(
                frame,
                GUID_WICPixelFormat32bppPBGRA,
                WICBitmapDitherTypeNone,
                nullptr,
                0.0,
                WICBitmapPaletteTypeCustom)))
        {
            out.width = width;
            out.height = height;
            out.mipCount = 1u;
            out.pixels.resize(static_cast<std::size_t>(width) * height * 4u);

            const UINT stride = width * 4u;
            if (SUCCEEDED(converter->CopyPixels(
                    nullptr, stride, static_cast<UINT>(out.pixels.size()), out.pixels.data())))
            {
                for (std::uint32_t i = 0u; i < width * height; ++i)
                {
                    const std::size_t offset = static_cast<std::size_t>(i) * 4u;
                    const std::uint8_t red = out.pixels[offset + 2];
                    out.pixels[offset + 2] = out.pixels[offset];
                    out.pixels[offset] = red;
                }
                success = true;
            }
        }
    }

    if (converter)
    {
        converter->Release();
    }
    if (frame)
    {
        frame->Release();
    }
    if (decoder)
    {
        decoder->Release();
    }
    if (stream)
    {
        stream->Release();
    }
    factory->Release();

    if (!success)
    {
        error = "PNG decode failed";
    }
    return success;
}

bool decodeDds(const std::uint8_t* bytes, std::size_t size, TextureData& out, std::string& error)
{
    if (size < 128u || std::memcmp(bytes, "DDS ", 4u) != 0 || readU32(bytes + 4u) != 124u)
    {
        error = "invalid DDS header";
        return false;
    }

    const std::uint32_t height = readU32(bytes + 12u);
    const std::uint32_t width = readU32(bytes + 16u);
    const std::uint32_t mipMapCount = readU32(bytes + 28u) > 0u ? readU32(bytes + 28u) : 1u;
    const std::uint32_t pixelFormatOffset = 76u;
    const std::uint32_t formatFlags = readU32(bytes + pixelFormatOffset + 4u);
    const std::uint32_t fourCC = readU32(bytes + pixelFormatOffset + 8u);

    bool compressed = false;
    std::uint32_t blockSize = 0u;
    bool swapChannels = false;

    if ((formatFlags & kDdpfFourCC) != 0u)
    {
        if (fourCC == kFourCCDXT1)
        {
            compressed = true;
            blockSize = 8u;
        }
        else if (fourCC == kFourCCDXT3 || fourCC == kFourCCDXT5)
        {
            compressed = true;
            blockSize = 16u;
        }
    }
    else if ((formatFlags & kDdpfRgb) != 0u)
    {
        swapChannels = true;
    }
    else
    {
        error = "unsupported DDS pixel format";
        return false;
    }

    std::size_t dataOffset = 128u;
    std::uint32_t mipWidth = width;
    std::uint32_t mipHeight = height;

    if (compressed)
    {
        std::size_t totalSize = 0u;
        for (std::uint32_t level = 0u; level < mipMapCount; ++level)
        {
            const std::uint32_t blockW = (mipWidth + 3u) / 4u;
            const std::uint32_t blockH = (mipHeight + 3u) / 4u;
            totalSize += static_cast<std::size_t>(blockW) * blockH * 64u;
            mipWidth = textureMipDimension(width, level + 1u);
            mipHeight = textureMipDimension(height, level + 1u);
        }
        out.width = width;
        out.height = height;
        out.mipCount = mipMapCount;
        out.pixels.resize(totalSize);

        mipWidth = width;
        mipHeight = height;
        std::size_t destinationOffset = 0u;
        for (std::uint32_t level = 0u; level < mipMapCount; ++level)
        {
            const std::uint32_t blockW = (mipWidth + 3u) / 4u;
            const std::uint32_t blockH = (mipHeight + 3u) / 4u;
            const std::size_t blockCount = static_cast<std::size_t>(blockW) * blockH;
            const std::size_t sourceBytes = blockCount * blockSize;
            if (dataOffset + sourceBytes > size)
            {
                error = "DDS data truncated";
                return false;
            }

            std::uint8_t* levelDestination = out.pixels.data() + destinationOffset;
            for (std::uint32_t blockY = 0u; blockY < blockH; ++blockY)
            {
                for (std::uint32_t blockX = 0u; blockX < blockW; ++blockX)
                {
                    const std::size_t blockIndex = static_cast<std::size_t>(blockY) * blockW + blockX;
                    const std::uint8_t* block = bytes + dataOffset + blockIndex * blockSize;
                    std::uint8_t* outputBlock =
                        levelDestination + blockIndex * 64u;

                    if (fourCC == kFourCCDXT1)
                    {
                        decodeBc1Block(block, outputBlock);
                    }
                    else if (fourCC == kFourCCDXT3)
                    {
                        decodeBc2Block(block, outputBlock);
                    }
                    else
                    {
                        decodeBc3Block(block, outputBlock);
                    }
                }
            }

            destinationOffset += blockCount * 64u;
            dataOffset += sourceBytes;
            mipWidth = textureMipDimension(width, level + 1u);
            mipHeight = textureMipDimension(height, level + 1u);
        }
        return true;
    }

    if (swapChannels)
    {
        out.width = width;
        out.height = height;
        out.mipCount = mipMapCount;
        std::size_t totalSize = 0u;
        for (std::uint32_t level = 0u; level < mipMapCount; ++level)
        {
            totalSize += static_cast<std::size_t>(
                textureMipDimension(width, level) * textureMipDimension(height, level) * 4u);
        }
        out.pixels.resize(totalSize);

        mipWidth = width;
        mipHeight = height;
        std::size_t sourceOffset = dataOffset;
        std::size_t destinationOffset = 0u;
        for (std::uint32_t level = 0u; level < mipMapCount; ++level)
        {
            const std::size_t levelSize = static_cast<std::size_t>(mipWidth) * mipHeight * 4u;
            if (sourceOffset + levelSize > size)
            {
                error = "DDS data truncated";
                return false;
            }
            const std::uint8_t* sourceRow = bytes + sourceOffset;
            std::uint8_t* destinationRow = out.pixels.data() + destinationOffset;
            for (std::uint32_t y = 0u; y < mipHeight; ++y)
            {
                const std::uint8_t* sourcePixel =
                    sourceRow + static_cast<std::size_t>(y) * mipWidth * 4u;
                std::uint8_t* destinationPixel =
                    destinationRow + static_cast<std::size_t>(y) * mipWidth * 4u;
                for (std::uint32_t x = 0u; x < mipWidth; ++x)
                {
                    destinationPixel[0] = sourcePixel[2];
                    destinationPixel[1] = sourcePixel[1];
                    destinationPixel[2] = sourcePixel[0];
                    destinationPixel[3] = sourcePixel[3];
                    sourcePixel += 4u;
                    destinationPixel += 4u;
                }
            }
            sourceOffset += levelSize;
            destinationOffset += levelSize;
            mipWidth = textureMipDimension(width, level + 1u);
            mipHeight = textureMipDimension(height, level + 1u);
        }
        return true;
    }

    error = "unsupported DDS variant";
    return false;
}

}

bool decodeTexture(const std::vector<std::uint8_t>& fileBytes, TextureData& out, std::string& error)
{
    if (fileBytes.size() >= 8u &&
        fileBytes[0] == 0x89u && fileBytes[1] == 0x50u &&
        fileBytes[2] == 0x4Eu && fileBytes[3] == 0x47u)
    {
        return decodePng(fileBytes.data(), fileBytes.size(), out, error);
    }
    if (fileBytes.size() >= 4u &&
        fileBytes[0] == 'D' && fileBytes[1] == 'D' &&
        fileBytes[2] == 'S' && fileBytes[3] == ' ')
    {
        return decodeDds(fileBytes.data(), fileBytes.size(), out, error);
    }
    error = "unrecognized texture format";
    return false;
}

void generateMipChain(TextureData& out, std::uint32_t maxMips)
{
    std::uint32_t depth = 1u;
    std::uint32_t trackingWidth = out.width;
    std::uint32_t trackingHeight = out.height;
    while (trackingWidth > 1u || trackingHeight > 1u)
    {
        trackingWidth = trackingWidth > 1u ? trackingWidth / 2u : 1u;
        trackingHeight = trackingHeight > 1u ? trackingHeight / 2u : 1u;
        ++depth;
    }
    if (maxMips > 0u && maxMips < depth)
    {
        depth = maxMips;
    }

    std::size_t totalSize = static_cast<std::size_t>(out.width) * out.height * 4u;
    for (std::uint32_t level = 1u; level < depth; ++level)
    {
        totalSize += static_cast<std::size_t>(
            textureMipDimension(out.width, level) *
            textureMipDimension(out.height, level) * 4u);
    }
    out.pixels.resize(totalSize);

    std::size_t previousOffset = 0u;
    std::uint32_t previousWidth = out.width;
    std::uint32_t previousHeight = out.height;
    std::size_t nextOffset = static_cast<std::size_t>(previousWidth) * previousHeight * 4u;

    for (std::uint32_t level = 1u; level < depth; ++level)
    {
        const std::uint32_t levelWidth = textureMipDimension(out.width, level);
        const std::uint32_t levelHeight = textureMipDimension(out.height, level);
        const std::uint8_t* source = out.pixels.data() + previousOffset;
        std::uint8_t* destination = out.pixels.data() + nextOffset;

        for (std::uint32_t y = 0u; y < levelHeight; ++y)
        {
            for (std::uint32_t x = 0u; x < levelWidth; ++x)
            {
                const std::size_t a = (static_cast<std::size_t>(y * 2u) * previousWidth + x * 2u) * 4u;
                const std::size_t b = a + 4u;
                const std::size_t c = a + static_cast<std::size_t>(previousWidth) * 4u;
                const std::size_t d = c + 4u;

                std::uint8_t* outPixel =
                    destination + (static_cast<std::size_t>(y) * levelWidth + x) * 4u;
                for (int channel = 0; channel < 4; ++channel)
                {
                    outPixel[channel] = static_cast<std::uint8_t>(
                        (source[a + channel] + source[b + channel] +
                         source[c + channel] + source[d + channel] + 2u) / 4u);
                }
            }
        }

        previousOffset = nextOffset;
        previousWidth = levelWidth;
        previousHeight = levelHeight;
        nextOffset += static_cast<std::size_t>(levelWidth) * levelHeight * 4u;
    }

    out.mipCount = depth;
}

}