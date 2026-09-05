#include "tests/graphics_tests.hpp"

#include "engine/graphics/mesh_upload.hpp"
#include "engine/graphics/postprocess.hpp"
#include "engine/graphics/shader_compiler.hpp"
#include "engine/shaders/shaders_storage.hpp"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
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

void checkNear(float actual, float expected, float tolerance, const char* what)
{
    const float difference = actual - expected;
    const float absolute = difference < 0.0f ? -difference : difference;
    if (absolute > tolerance)
    {
        std::printf("FAIL: %s (actual=%.5f expected=%.5f)\n", what, actual, expected);
        ++failures;
    }
}

void testShaderSourcesCompile()
{
    ID3DBlob* bytecode = nullptr;
    std::string error;

    CHECK(ksge::compileShaderSource(ksge::shaders::kPbrVertex, "main", "vs_5_0", bytecode, error));
    if (bytecode)
    {
        bytecode->Release();
        bytecode = nullptr;
    }
    CHECK(ksge::compileShaderSource(ksge::shaders::kPbrPixel, "main", "ps_5_0", bytecode, error));
    if (bytecode)
    {
        bytecode->Release();
        bytecode = nullptr;
    }
    CHECK(ksge::compileShaderSource(ksge::shaders::kSkyVertex, "main", "vs_5_0", bytecode, error));
    if (bytecode)
    {
        bytecode->Release();
        bytecode = nullptr;
    }
    CHECK(ksge::compileShaderSource(ksge::shaders::kSkyPixel, "main", "ps_5_0", bytecode, error));
    if (bytecode)
    {
        bytecode->Release();
        bytecode = nullptr;
    }

    CHECK(ksge::compileShaderSource(ksge::shaders::kPostVertex, "main", "vs_5_0", bytecode, error));
    if (bytecode)
    {
        bytecode->Release();
        bytecode = nullptr;
    }
    for (const char* body : {
             ksge::shaders::kPostPassthrough,
             ksge::shaders::kSsaoBody,
             ksge::shaders::kSsaoBlurHBody,
             ksge::shaders::kSsaoBlurVBody,
             ksge::shaders::kFogBody,
             ksge::shaders::kBloomExtractBody,
             ksge::shaders::kBloomDownsampleBody,
             ksge::shaders::kBloomBlurHBody,
             ksge::shaders::kBloomBlurVBody,
             ksge::shaders::kBloomUpsampleBody,
             ksge::shaders::kCompositeBody,
         })
    {
        const std::string source = ksge::shaders::postProcessPixelShader(body);
        CHECK(ksge::compileShaderSource(source, "main", "ps_5_0", bytecode, error));
        if (bytecode)
        {
            bytecode->Release();
            bytecode = nullptr;
        }
    }
}

void testPostProcessShadersCompile()
{
    ID3DBlob* bytecode = nullptr;
    std::string error;

    CHECK(ksge::compileShaderSource(ksge::shaders::kPostVertex, "main", "vs_5_0", bytecode, error));
    if (bytecode)
    {
        bytecode->Release();
        bytecode = nullptr;
    }

    const char* bodies[] = {
        ksge::shaders::kSsaoBody,
        ksge::shaders::kSsaoBlurHBody,
        ksge::shaders::kSsaoBlurVBody,
        ksge::shaders::kFogBody,
        ksge::shaders::kBloomExtractBody,
        ksge::shaders::kBloomDownsampleBody,
        ksge::shaders::kBloomBlurHBody,
        ksge::shaders::kBloomBlurVBody,
        ksge::shaders::kBloomUpsampleBody,
        ksge::shaders::kCompositeBody,
    };
    for (const char* body : bodies)
    {
        CHECK(ksge::compileShaderSource(
            ksge::shaders::postProcessPixelShader(body), "main", "ps_5_0", bytecode, error));
        if (bytecode)
        {
            bytecode->Release();
            bytecode = nullptr;
        }
    }
}

void checkLutChannel(
    const std::vector<float>& lut,
    std::uint32_t size,
    std::uint32_t r,
    std::uint32_t g,
    std::uint32_t b,
    std::uint32_t channel,
    float expected,
    float tolerance,
    const char* what)
{
    const std::size_t pixel =
        (static_cast<std::size_t>(b) * size + g) * size + r;
    const float actual = lut[pixel * 4u + channel];
    if (actual < expected - tolerance || actual > expected + tolerance)
    {
        std::printf("FAIL: %s (actual=%.5f expected=%.5f)\n", what, actual, expected);
        ++failures;
    }
}

void testGradingLutIdentity()
{
    constexpr std::uint32_t size = 33u;
    std::vector<float> lut(static_cast<std::size_t>(size) * size * size * 4u);
    ksge::GradingParams params = {};
    params.exposure = 1.0f;
    params.contrast = 1.0f;
    params.saturation = 1.0f;
    ksge::generateGradingLut(lut.data(), size, params);

    checkLutChannel(lut, size, 8u, 16u, 17u, 0u, 8.0f / 32.0f, 1.0e-4f, "lut identity r");
    checkLutChannel(lut, size, 8u, 16u, 17u, 1u, 16.0f / 32.0f, 1.0e-4f, "lut identity g");
    checkLutChannel(lut, size, 8u, 16u, 17u, 2u, 17.0f / 32.0f, 1.0e-4f, "lut identity b");
}

void testGradingLutContrast()
{
    constexpr std::uint32_t size = 33u;
    std::vector<float> lut(static_cast<std::size_t>(size) * size * size * 4u);
    ksge::GradingParams params = {};
    params.exposure = 1.0f;
    params.contrast = 2.0f;
    params.saturation = 1.0f;
    ksge::generateGradingLut(lut.data(), size, params);

    checkLutChannel(lut, size, 8u, 8u, 8u, 0u, 0.0f, 1.0e-4f, "lut contrast dark zero");
    checkLutChannel(lut, size, 24u, 24u, 24u, 0u, 1.0f, 1.0e-4f, "lut contrast bright one");
}

void testGradingLutSaturation()
{
    constexpr std::uint32_t size = 33u;
    std::vector<float> lut(static_cast<std::size_t>(size) * size * size * 4u);
    ksge::GradingParams params = {};
    params.exposure = 1.0f;
    params.contrast = 1.0f;
    params.saturation = 0.0f;
    ksge::generateGradingLut(lut.data(), size, params);

    const std::size_t pixel =
        (static_cast<std::size_t>(8u) * size + 8u) * size + 24u;
    const float r = lut[pixel * 4u];
    const float g = lut[pixel * 4u + 1u];
    const float b = lut[pixel * 4u + 2u];
    CHECK(r > g - 1.0e-4f && r < g + 1.0e-4f);
    CHECK(g > b - 1.0e-4f && g < b + 1.0e-4f);
}

void testPrepareTriangle()
{
    ksge::MeshData triangle;
    triangle.vertexCount = 3u;
    triangle.vertexMask = ksge::VertexPosition;
    triangle.vertexStride = 12u;

    const float positions[9] = {
        0.0f, 0.0f, 0.0f,
        1.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f,
    };
    triangle.vertices.resize(36u);
    std::memcpy(triangle.vertices.data(), positions, sizeof(positions));
    triangle.indices = {0u, 1u, 2u};
    triangle.boundsMin = {0.0f, 0.0f, 0.0f};
    triangle.boundsMax = {1.0f, 1.0f, 0.0f};

    ksge::PreparedMesh prepared;
    prepareMesh(triangle, prepared);

    CHECK(prepared.vertexCount == 3u);
    CHECK(prepared.indexCount == 3u);
    CHECK(prepared.vertices.size() == 3u * ksge::kPreparedStride);

    for (std::uint32_t vertex = 0u; vertex < 3u; ++vertex)
    {
        const std::uint8_t* data = prepared.vertices.data() + static_cast<std::size_t>(vertex) * ksge::kPreparedStride;
        float normal[3];
        std::memcpy(normal, data + 12u, 12u);
        const float length = std::sqrt(normal[0] * normal[0] + normal[1] * normal[1] + normal[2] * normal[2]);
        checkNear(length, 1.0f, 1.0e-4f, "triangle normal unit length");

        float uv[2];
        std::memcpy(uv, data + 24u, 8u);
        checkNear(uv[0], 0.0f, 1.0e-5f, "triangle uv zero");
        checkNear(uv[1], 0.0f, 1.0e-5f, "triangle uv zero");
    }
}

void testCubeStructure()
{
    const ksge::MeshData cube = ksge::makeCube(1.0f);
    CHECK(cube.vertexCount == 24u);
    CHECK(cube.indices.size() == 36u);
    CHECK((cube.vertexMask & (ksge::VertexPosition | ksge::VertexNormal | ksge::VertexUv | ksge::VertexTangent)) != 0u);

    ksge::PreparedMesh prepared;
    prepareMesh(cube, prepared);
    CHECK(prepared.vertexCount == 24u);

    const std::uint8_t* first = prepared.vertices.data();
    float position[3];
    std::memcpy(position, first, 12u);
    checkNear(position[0], 0.5f, 1.0e-5f, "cube first vertex x");
}

void testSphereNormals()
{
    const ksge::MeshData sphere = ksge::makeSphere(16u, 8u, 0.5f);
    ksge::PreparedMesh prepared;
    prepareMesh(sphere, prepared);
    CHECK(prepared.vertexCount == sphere.vertexCount);
}

}

int runGraphicsTests()
{
    testShaderSourcesCompile();
    testPostProcessShadersCompile();
    testGradingLutIdentity();
    testGradingLutContrast();
    testGradingLutSaturation();
    testPrepareTriangle();
    testCubeStructure();
    testSphereNormals();

    std::printf("KSGE graphics tests: %d failure(s)\n", failures);
    return failures;
}