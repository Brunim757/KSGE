#include "tests/graphics_tests.hpp"

#include "engine/graphics/mesh_upload.hpp"
#include "engine/graphics/shader_compiler.hpp"
#include "engine/shaders/shaders_storage.hpp"

#include <cmath>
#include <cstdio>
#include <cstring>

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
    checkNear(position[0], 0.5f, 1.0e-5f, "cube corner x");
    checkNear(position[1], 0.5f, 1.0e-5f, "cube corner y");
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
    testPrepareTriangle();
    testCubeStructure();
    testSphereNormals();

    std::printf("KSGE graphics tests: %d failure(s)\n", failures);
    return failures;
}