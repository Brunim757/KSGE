#pragma once

#include <d3d11.h>

#include <array>
#include <cstdint>
#include <vector>

#include <flecs.h>

#include "engine/assets/mesh_data.hpp"
#include "engine/assets/texture_data.hpp"
#include "engine/graphics/device.hpp"
#include "engine/graphics/mesh_upload.hpp"
#include "engine/graphics/postprocess.hpp"
#include "engine/graphics/shadow_cascade.hpp"
#include "engine/scene/components.hpp"

namespace ksge {

struct GpuMesh
{
    ID3D11Buffer* vertices = nullptr;
    ID3D11Buffer* indices = nullptr;
    std::uint32_t vertexCount = 0u;
    std::uint32_t indexCount = 0u;
    math::Vec3 boundsMin{0.0f, 0.0f, 0.0f};
    math::Vec3 boundsMax{0.0f, 0.0f, 0.0f};
};

struct GpuTexture
{
    ID3D11Texture2D* texture = nullptr;
    ID3D11ShaderResourceView* srv = nullptr;
};

struct DrawEntry
{
    std::uint32_t meshIndex = ~0u;
    std::uint64_t key = 0u;
    const PbrMaterial* material = nullptr;
    DirectX::XMFLOAT4X4 world;
};

class Renderer
{
public:
    Renderer(GraphicsDevice& device, flecs::world& world);
    ~Renderer();

    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    std::uint32_t uploadMesh(const MeshData& mesh);
    std::uint32_t uploadTexture(const TextureData& texture);

    void render();

    void setDebugMode(std::uint32_t mode);

    void setLodChain(
        std::uint32_t meshIndex,
        std::uint32_t mediumMesh,
        std::uint32_t lowMesh);
    void updateGpuTime();

    std::uint32_t frameDraws() const;
    std::uint32_t frameInstances() const;
    std::uint32_t gbufferDraws() const;
    std::uint32_t gbufferInstances() const;
    std::uint32_t shadowDraws() const;
    std::uint32_t shadowInstances() const;
    float frameCpuMs() const;
    float frameGpuMs() const;

private:
    void createPipeline();
    void createStates();
    void createDefaultTextures();
    void buildEntries(const Frustum& frustum);
    void drawGBufferBatches();
    void drawShadowBatches();
    void drawObjectBatchRange(
        const std::vector<DrawEntry>& entries,
        std::size_t first,
        std::size_t after,
        bool shadow);
    void drawGrid();

    void bindMesh(std::uint32_t meshIndex);

    GraphicsDevice& device_;
    ID3D11Device* d3d_;
    ID3D11DeviceContext* context_;
    flecs::world& world_;

    ID3D11VertexShader* gbufferVertexShader_ = nullptr;
    ID3D11PixelShader* gbufferPixelShader_ = nullptr;
    ID3D11VertexShader* gridVertexShader_ = nullptr;
    ID3D11PixelShader* gridPixelShader_ = nullptr;
    ID3D11VertexShader* shadowVertexShader_ = nullptr;
    ID3D11PixelShader* shadowPixelShader_ = nullptr;
    ID3D11InputLayout* inputLayout_ = nullptr;

    ID3D11Buffer* sceneBuffer_ = nullptr;
    ID3D11Buffer* objectBuffer_ = nullptr;
    ID3D11Buffer* shadowBuffer_ = nullptr;
    ID3D11Buffer* instanceBuffer_ = nullptr;
    ID3D11ShaderResourceView* instanceView_ = nullptr;

    ID3D11SamplerState* linearSampler_ = nullptr;
    ID3D11RasterizerState* solidState_ = nullptr;
    ID3D11RasterizerState* doubleSidedState_ = nullptr;
    ID3D11DepthStencilState* depthState_ = nullptr;
    ID3D11BlendState* opaqueBlendState_ = nullptr;

    ID3D11ShaderResourceView* defaultBaseSrv_ = nullptr;
    ID3D11ShaderResourceView* defaultMrSrv_ = nullptr;
    ID3D11ShaderResourceView* defaultNormalSrv_ = nullptr;
    ID3D11ShaderResourceView* defaultEmissiveSrv_ = nullptr;

    std::vector<GpuMesh> meshes_;
    std::vector<GpuTexture> textures_;
    std::vector<DrawEntry> gbufferEntries_;
    std::vector<DrawEntry> shadowEntries_;
    std::vector<DirectX::XMFLOAT4X4> instancedMatrices_;
    std::vector<std::array<std::uint32_t, 3u>> lodChains_;
    std::uint32_t gridMesh_ = ~0u;

    DirectX::XMFLOAT4X4 shadowViewProjection_[kShadowCascades];

    ID3D11Query* disjointQuery_ = nullptr;
    ID3D11Query* timestampStart_ = nullptr;
    ID3D11Query* timestampEnd_ = nullptr;

    std::uint32_t frameDraws_ = 0u;
    std::uint32_t frameInstances_ = 0u;
    std::uint32_t gbufferDraws_ = 0u;
    std::uint32_t gbufferInstances_ = 0u;
    std::uint32_t shadowDraws_ = 0u;
    std::uint32_t shadowInstances_ = 0u;
    float frameCpuMs_ = 0.0f;
    float frameGpuMs_ = 0.0f;

    PostProcess postProcess_;
    std::uint32_t debugMode_ = 0u;
};

}