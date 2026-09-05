#pragma once

#include <d3d11.h>

#include <cstdint>
#include <vector>

#include <flecs.h>

#include "engine/assets/mesh_data.hpp"
#include "engine/assets/texture_data.hpp"
#include "engine/graphics/device.hpp"
#include "engine/graphics/mesh_upload.hpp"
#include "engine/graphics/postprocess.hpp"
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

private:
    void createPipeline();
    void createStates();
    void createDefaultTextures();
    void drawMesh(
        std::uint32_t meshIndex,
        const DirectX::XMMATRIX& world,
        const PbrMaterial& material);
    void drawSky();

    GraphicsDevice& device_;
    ID3D11Device* d3d_;
    ID3D11DeviceContext* context_;
    flecs::world& world_;

    ID3D11VertexShader* pbrVertexShader_ = nullptr;
    ID3D11PixelShader* pbrPixelShader_ = nullptr;
    ID3D11VertexShader* skyVertexShader_ = nullptr;
    ID3D11PixelShader* skyPixelShader_ = nullptr;
    ID3D11InputLayout* inputLayout_ = nullptr;

    ID3D11Buffer* sceneBuffer_ = nullptr;
    ID3D11Buffer* objectBuffer_ = nullptr;
    ID3D11Buffer* skyBuffer_ = nullptr;

    ID3D11SamplerState* linearSampler_ = nullptr;
    ID3D11RasterizerState* solidState_ = nullptr;
    ID3D11RasterizerState* doubleSidedState_ = nullptr;
    ID3D11DepthStencilState* depthState_ = nullptr;
    ID3D11DepthStencilState* skyDepthState_ = nullptr;
    ID3D11BlendState* opaqueBlendState_ = nullptr;

    ID3D11ShaderResourceView* defaultBaseSrv_ = nullptr;
    ID3D11ShaderResourceView* defaultMrSrv_ = nullptr;
    ID3D11ShaderResourceView* defaultNormalSrv_ = nullptr;
    ID3D11ShaderResourceView* defaultEmissiveSrv_ = nullptr;

    std::vector<GpuMesh> meshes_;
    std::vector<GpuTexture> textures_;
    std::uint32_t skyMesh_ = ~0u;

    PostProcess postProcess_;
    std::uint32_t debugMode_ = 0u;
};

}