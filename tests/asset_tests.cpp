#define NOMINMAX
#include <windows.h>

#include "tests/asset_tests.hpp"

#include "engine/assets/registry.hpp"
#include "engine/assets/texture.hpp"
#include "engine/assets/texture_data.hpp"

#include <wincodec.h>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <vector>

namespace {

int failures = 0;

void expect(bool condition, const char* what)
{
    if (!condition)
    {
        std::printf("FAIL: %s\n", what);
        ++failures;
    }
}

#define CHECK(...) expect((__VA_ARGS__), #__VA_ARGS__)

float absf(float value)
{
    return value < 0.0f ? -value : value;
}

void checkNear(float actual, float expected, float tolerance, const char* what)
{
    if (absf(actual - expected) > tolerance)
    {
        std::printf("FAIL: %s (actual=%.5f expected=%.5f)\n", what, actual, expected);
        ++failures;
    }
}

void writeU32le(std::vector<std::uint8_t>& bytes, std::size_t offset, std::uint32_t value)
{
    bytes[offset + 0u] = static_cast<std::uint8_t>(value & 0xFFu);
    bytes[offset + 1u] = static_cast<std::uint8_t>((value >> 8u) & 0xFFu);
    bytes[offset + 2u] = static_cast<std::uint8_t>((value >> 16u) & 0xFFu);
    bytes[offset + 3u] = static_cast<std::uint8_t>((value >> 24u) & 0xFFu);
}

void writeU16le(std::vector<std::uint8_t>& bytes, std::size_t offset, std::uint16_t value)
{
    bytes[offset + 0u] = static_cast<std::uint8_t>(value & 0xFFu);
    bytes[offset + 1u] = static_cast<std::uint8_t>((value >> 8u) & 0xFFu);
}

std::vector<std::uint8_t> makeDdsHeader(std::uint32_t width, std::uint32_t height, std::uint32_t fourCC)
{
    std::vector<std::uint8_t> bytes(128u, 0u);
    std::memcpy(bytes.data(), "DDS ", 4u);
    writeU32le(bytes, 4u, 124u);
    writeU32le(bytes, 8u, 0x00001007u);
    writeU32le(bytes, 12u, height);
    writeU32le(bytes, 16u, width);
    writeU32le(bytes, 20u, ((width + 3u) / 4u) * ((height + 3u) / 4u) * (fourCC == 0x31545844u ? 8u : 16u));
    writeU32le(bytes, 28u, 1u);
    writeU32le(bytes, 76u, 32u);
    writeU32le(bytes, 80u, 0x00000004u);
    writeU32le(bytes, 84u, fourCC);
    writeU32le(bytes, 108u, 0x00001000u);
    return bytes;
}

void testGltfTriangle()
{
    const std::filesystem::path directory =
        std::filesystem::temp_directory_path() / "ksge_test_assets";
    std::filesystem::create_directories(directory);
    const std::filesystem::path path = directory / "triangle.gltf";

    const char* gltf = R"({
  "asset": {"version": "2.0", "generator": "KSGE Test"},
  "scene": 0,
  "scenes": [{"nodes": [0]}],
  "nodes": [{"mesh": 0}],
  "materials": [{
    "pbrMetallicRoughness": {
      "baseColorFactor": [1.0, 0.0, 0.0, 1.0],
      "metallicFactor": 0.5,
      "roughnessFactor": 0.25
    },
    "doubleSided": true
  }],
  "meshes": [{"primitives": [
    {"attributes": {"POSITION": 0}, "material": 0, "mode": 4}
  ]}],
  "bufferViews": [{"buffer": 0, "byteOffset": 0, "byteLength": 36, "target": 34962}],
  "accessors": [{
    "bufferView": 0, "componentType": 5126, "count": 3, "type": "VEC3",
    "min": [0.0, 0.0, 0.0], "max": [1.0, 1.0, 0.0]
  }],
  "buffers": [{
    "byteLength": 36,
    "uri": "data:application/octet-stream;base64,AAAAAAAAAAAAAAAAAACAPwAAAAAAAAAAAAAAAAAAgD8AAAAA"
  }]
})";

    std::ofstream writer(path, std::ios::binary);
    writer.write(gltf, static_cast<std::streamsize>(std::strlen(gltf)));
    writer.close();

    ksge::AssetRegistry registry;

    const std::uint32_t meshIndex = registry.loadMesh(path);
    CHECK(meshIndex != ksge::AssetRegistry::kInvalidIndex);
    CHECK(registry.lastError().empty());

    const ksge::MeshData& mesh = registry.mesh(meshIndex);
    CHECK(mesh.vertexCount == 3u);
    CHECK(mesh.vertexStride == 12u);
    CHECK(mesh.vertexMask == ksge::VertexPosition);
    CHECK(mesh.indices.size() == 3u);
    CHECK(mesh.indices[0] == 0u && mesh.indices[1] == 1u && mesh.indices[2] == 2u);

    float v0[3] = {};
    std::memcpy(v0, mesh.vertices.data(), sizeof(float) * 3u);
    checkNear(v0[0], 0.0f, 1.0e-5f, "triangle v0 x");
    checkNear(v0[1], 0.0f, 1.0e-5f, "triangle v0 y");

    float v1[3] = {};
    std::memcpy(v1, mesh.vertices.data() + mesh.vertexStride, sizeof(float) * 3u);
    checkNear(v1[0], 1.0f, 1.0e-5f, "triangle v1 x");

    float v2[3] = {};
    std::memcpy(v2, mesh.vertices.data() + 2u * mesh.vertexStride, sizeof(float) * 3u);
    checkNear(v2[1], 1.0f, 1.0e-5f, "triangle v2 y");

    checkNear(mesh.boundsMin.x, 0.0f, 1.0e-5f, "triangle bounds min x");
    checkNear(mesh.boundsMax.x, 1.0f, 1.0e-5f, "triangle bounds max x");
    checkNear(mesh.boundsMax.y, 1.0f, 1.0e-5f, "triangle bounds max y");

    const std::uint32_t materialIndex = registry.loadMaterial(path, 0u);
    CHECK(materialIndex != ksge::AssetRegistry::kInvalidIndex);
    const ksge::MaterialData& material = registry.material(materialIndex);
    checkNear(material.baseColorFactor.x, 1.0f, 1.0e-5f, "material base color r");
    checkNear(material.baseColorFactor.y, 0.0f, 1.0e-5f, "material base color g");
    checkNear(material.metallicFactor, 0.5f, 1.0e-5f, "material metallic");
    checkNear(material.roughnessFactor, 0.25f, 1.0e-5f, "material roughness");
    CHECK(material.doubleSided);

    const std::uint32_t cachedIndex = registry.loadMesh(path);
    CHECK(cachedIndex == meshIndex);
    CHECK(registry.meshCount() == 1u);
}

void testPngRoundTrip()
{
    const std::filesystem::path directory =
        std::filesystem::temp_directory_path() / "ksge_test_assets";
    std::filesystem::create_directories(directory);
    const std::filesystem::path path = directory / "red.png";

    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    IWICImagingFactory* factory = nullptr;
    CHECK(SUCCEEDED(CoCreateInstance(
        CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory))));

    IStream* stream = nullptr;
    CHECK(SUCCEEDED(CreateStreamOnHGlobal(nullptr, TRUE, &stream)));

    IWICBitmapEncoder* encoder = nullptr;
    CHECK(SUCCEEDED(factory->CreateEncoder(GUID_ContainerFormatPng, nullptr, &encoder)));
    CHECK(SUCCEEDED(encoder->Initialize(stream, WICBitmapEncoderNoCache)));

    IWICBitmapFrameEncode* frame = nullptr;
    IPropertyBag2* bag = nullptr;
    CHECK(SUCCEEDED(encoder->CreateNewFrame(&frame, &bag)));
    CHECK(SUCCEEDED(frame->Initialize(bag)));

    std::uint8_t sourcePixel[4] = {0u, 0u, 255u, 255u};
    IWICBitmap* bitmap = nullptr;
    CHECK(SUCCEEDED(factory->CreateBitmapFromMemory(
        1u,
        1u,
        GUID_WICPixelFormat32bppPBGRA,
        4u,
        4u,
        sourcePixel,
        &bitmap)));
    CHECK(SUCCEEDED(frame->WriteSource(bitmap, nullptr)));
    CHECK(SUCCEEDED(frame->Commit()));
    CHECK(SUCCEEDED(encoder->Commit()));

    HGLOBAL global = nullptr;
    CHECK(SUCCEEDED(GetHGlobalFromStream(stream, &global)));
    const std::size_t size = GlobalSize(global);
    const void* locked = GlobalLock(global);
    std::vector<std::uint8_t> fileBytes(static_cast<const std::uint8_t*>(locked),
        static_cast<const std::uint8_t*>(locked) + size);
    GlobalUnlock(global);

    bitmap->Release();
    bag->Release();
    frame->Release();
    encoder->Release();
    stream->Release();
    factory->Release();

    std::ofstream writer(path, std::ios::binary);
    writer.write(reinterpret_cast<const char*>(fileBytes.data()),
        static_cast<std::streamsize>(fileBytes.size()));
    writer.close();

    ksge::AssetRegistry registry;
    const std::uint32_t textureIndex = registry.loadTexture(path);
    CHECK(textureIndex != ksge::AssetRegistry::kInvalidIndex);

    const ksge::TextureData& texture = registry.texture(textureIndex);
    CHECK(texture.width == 1u);
    CHECK(texture.height == 1u);
    CHECK(texture.pixels.size() == 4u);
    CHECK(texture.pixels[0] == 255u);
    CHECK(texture.pixels[1] == 0u);
    CHECK(texture.pixels[2] == 0u);
    CHECK(texture.pixels[3] == 255u);
}

void testDdsBc1()
{
    const std::filesystem::path directory =
        std::filesystem::temp_directory_path() / "ksge_test_assets";
    std::filesystem::create_directories(directory);
    const std::filesystem::path path = directory / "white.dds";

    std::vector<std::uint8_t> fileBytes = makeDdsHeader(4u, 4u, 0x31545844u);
    const std::uint8_t block[8] = {0xFFu, 0xFFu, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u};
    fileBytes.insert(fileBytes.end(), block, block + 8u);

    std::ofstream writer(path, std::ios::binary);
    writer.write(reinterpret_cast<const char*>(fileBytes.data()),
        static_cast<std::streamsize>(fileBytes.size()));
    writer.close();

    ksge::AssetRegistry registry;
    const std::uint32_t textureIndex = registry.loadTexture(path);
    CHECK(textureIndex != ksge::AssetRegistry::kInvalidIndex);

    const ksge::TextureData& texture = registry.texture(textureIndex);
    CHECK(texture.width == 4u);
    CHECK(texture.height == 4u);
    CHECK(texture.pixels.size() == 64u);
    for (std::size_t i = 0u; i < texture.pixels.size(); i += 4u)
    {
        CHECK(texture.pixels[i] == 255u);
        CHECK(texture.pixels[i + 3] == 255u);
    }
}

void testDdsUncompressed()
{
    const std::filesystem::path directory =
        std::filesystem::temp_directory_path() / "ksge_test_assets";
    std::filesystem::create_directories(directory);
    const std::filesystem::path path = directory / "blue.dds";

    std::vector<std::uint8_t> fileBytes(128u, 0u);
    std::memcpy(fileBytes.data(), "DDS ", 4u);
    writeU32le(fileBytes, 4u, 124u);
    writeU32le(fileBytes, 8u, 0x00001007u);
    writeU32le(fileBytes, 12u, 2u);
    writeU32le(fileBytes, 16u, 2u);
    writeU32le(fileBytes, 20u, 16u);
    writeU32le(fileBytes, 28u, 1u);
    writeU32le(fileBytes, 76u, 32u);
    writeU32le(fileBytes, 80u, 0x00000040u);
    writeU32le(fileBytes, 88u, 32u);
    writeU32le(fileBytes, 92u, 0x00FF0000u);
    writeU32le(fileBytes, 96u, 0x0000FF00u);
    writeU32le(fileBytes, 100u, 0x000000FFu);
    writeU32le(fileBytes, 104u, 0xFF000000u);
    writeU32le(fileBytes, 108u, 0x00001000u);

    const std::uint8_t pixels[16] = {
        0xFFu, 0x00u, 0x00u, 0xFFu,
        0x00u, 0xFFu, 0x00u, 0xFFu,
        0x00u, 0x00u, 0xFFu, 0xFFu,
        0x80u, 0x80u, 0x80u, 0xFFu,
    };
    fileBytes.insert(fileBytes.end(), pixels, pixels + 16u);

    std::ofstream writer(path, std::ios::binary);
    writer.write(reinterpret_cast<const char*>(fileBytes.data()),
        static_cast<std::streamsize>(fileBytes.size()));
    writer.close();

    ksge::AssetRegistry registry;
    const std::uint32_t textureIndex = registry.loadTexture(path);
    CHECK(textureIndex != ksge::AssetRegistry::kInvalidIndex);

    const ksge::TextureData& texture = registry.texture(textureIndex);
    CHECK(texture.width == 2u);
    CHECK(texture.height == 2u);
    CHECK(texture.pixels.size() == 16u);
    CHECK(texture.pixels[0] == 255u && texture.pixels[1] == 0u && texture.pixels[2] == 0u);
    CHECK(texture.pixels[4] == 0u && texture.pixels[5] == 255u && texture.pixels[6] == 0u);
    CHECK(texture.pixels[8] == 0u && texture.pixels[9] == 0u && texture.pixels[10] == 255u);

    ksge::TextureData mips = registry.texture(textureIndex);
    ksge::generateMipChain(mips, 0u);
    CHECK(mips.mipCount == 2u);
    CHECK(ksge::textureMipDimension(mips.width, 1u) == 1u);
    CHECK(mips.pixels.size() == 20u);
}

}

int runAssetTests()
{
    testGltfTriangle();
    testPngRoundTrip();
    testDdsBc1();
    testDdsUncompressed();

    std::printf("KSGE asset tests: %d failure(s)\n", failures);
    return failures;
}