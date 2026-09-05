#include "engine/graphics/renderer.hpp"

#include "engine/graphics/shader_compiler.hpp"
#include "engine/scene/camera.hpp"
#include "engine/scene/components.hpp"
#include "engine/shaders/shaders_storage.hpp"

#include <d3d11.h>
#include <DirectXMath.h>

namespace ksge {

namespace {

constexpr UINT kZeroOffset = 0u;

DirectX::XMFLOAT3 cameraPosition(flecs::world& world)
{
    const flecs::entity camera = world.entity("editor_camera");
    DirectX::XMFLOAT3 position = {0.0f, 0.0f, 0.0f};
    if (camera.is_alive())
    {
        position = camera.get_mut<Transform>().position;
    }
    return position;
}

struct SceneConstants
{
    DirectX::XMFLOAT4X4 viewProjection;
    DirectX::XMFLOAT4 camPosition;
    DirectX::XMFLOAT4 sunDirection;
    DirectX::XMFLOAT4 sunColor;
    DirectX::XMFLOAT4 skyTop;
    DirectX::XMFLOAT4 skyHorizon;
};

struct ObjectConstants
{
    DirectX::XMFLOAT4X4 world;
    DirectX::XMFLOAT4 baseColorFactor;
    DirectX::XMFLOAT4 mrao;
    DirectX::XMFLOAT4 emissive;
    DirectX::XMFLOAT4 hasTextures;
};

struct SkyConstants
{
    DirectX::XMFLOAT4X4 viewProjection;
    DirectX::XMFLOAT4 camPosition;
    DirectX::XMFLOAT4 sunDirection;
    DirectX::XMFLOAT4 skyTop;
    DirectX::XMFLOAT4 skyHorizon;
};

DirectX::XMFLOAT4X4 makeSkyViewProj(const CameraFrame& frame)
{
    DirectX::XMMATRIX view = math::load(frame.view);
    DirectX::XMFLOAT4X4 stored;
    math::store(stored, view);
    stored._41 = 0.0f;
    stored._42 = 0.0f;
    stored._43 = 0.0f;
    stored._44 = 1.0f;
    const DirectX::XMMATRIX combined =
        math::load(stored) * math::load(frame.projection);
    DirectX::XMFLOAT4X4 result;
    math::store(result, combined);
    return result;
}

}

Renderer::Renderer(GraphicsDevice& device, flecs::world& world)
    : device_(device)
    , d3d_(device.device())
    , context_(device.context())
    , world_(world)
{
    world_.component<MeshRenderer>();
    world_.component<PbrMaterial>();
    world_.component<DirectionalLight>();
    world_.set<DirectionalLight>({});

    createPipeline();
    createStates();
    createDefaultTextures();
    skyMesh_ = uploadMesh(makeCube(1.0f));
}

Renderer::~Renderer()
{
    for (GpuMesh& mesh : meshes_)
    {
        if (mesh.vertices)
        {
            mesh.vertices->Release();
        }
        if (mesh.indices)
        {
            mesh.indices->Release();
        }
    }
    for (GpuTexture& texture : textures_)
    {
        if (texture.srv)
        {
            texture.srv->Release();
        }
        if (texture.texture)
        {
            texture.texture->Release();
        }
    }
    if (defaultEmissiveSrv_)
    {
        defaultEmissiveSrv_->Release();
    }
    if (defaultNormalSrv_)
    {
        defaultNormalSrv_->Release();
    }
    if (defaultMrSrv_)
    {
        defaultMrSrv_->Release();
    }
    if (defaultBaseSrv_)
    {
        defaultBaseSrv_->Release();
    }
    if (opaqueBlendState_)
    {
        opaqueBlendState_->Release();
    }
    if (skyDepthState_)
    {
        skyDepthState_->Release();
    }
    if (depthState_)
    {
        depthState_->Release();
    }
    if (doubleSidedState_)
    {
        doubleSidedState_->Release();
    }
    if (solidState_)
    {
        solidState_->Release();
    }
    if (linearSampler_)
    {
        linearSampler_->Release();
    }
    if (skyBuffer_)
    {
        skyBuffer_->Release();
    }
    if (objectBuffer_)
    {
        objectBuffer_->Release();
    }
    if (sceneBuffer_)
    {
        sceneBuffer_->Release();
    }
    if (inputLayout_)
    {
        inputLayout_->Release();
    }
    if (skyPixelShader_)
    {
        skyPixelShader_->Release();
    }
    if (skyVertexShader_)
    {
        skyVertexShader_->Release();
    }
    if (pbrPixelShader_)
    {
        pbrPixelShader_->Release();
    }
    if (pbrVertexShader_)
    {
        pbrVertexShader_->Release();
    }
}

void Renderer::createPipeline()
{
    std::string error;

    ID3DBlob* vertexBytecode = nullptr;
    if (!createVertexShader(d3d_, shaders::kPbrVertex, pbrVertexShader_, vertexBytecode, error) ||
        !createPixelShader(d3d_, shaders::kPbrPixel, pbrPixelShader_, error))
    {
        pbrVertexShader_ = nullptr;
        pbrPixelShader_ = nullptr;
        if (vertexBytecode)
        {
            vertexBytecode->Release();
        }
        return;
    }

    const D3D11_INPUT_ELEMENT_DESC elements[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0u, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12u, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24u, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"TANGENT", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 32u, D3D11_INPUT_PER_VERTEX_DATA, 0},
    };

    if (FAILED(d3d_->CreateInputLayout(
            elements,
            4u,
            vertexBytecode->GetBufferPointer(),
            vertexBytecode->GetBufferSize(),
            &inputLayout_)))
    {
        inputLayout_ = nullptr;
    }
    vertexBytecode->Release();

    if (!createVertexShader(d3d_, shaders::kSkyVertex, skyVertexShader_, vertexBytecode, error) ||
        !createPixelShader(d3d_, shaders::kSkyPixel, skyPixelShader_, error))
    {
        skyVertexShader_ = nullptr;
        skyPixelShader_ = nullptr;
        if (vertexBytecode)
        {
            vertexBytecode->Release();
        }
    }
    if (vertexBytecode)
    {
        vertexBytecode->Release();
    }

     D3D11_BUFFER_DESC sceneDesc = {};
    sceneDesc.ByteWidth = sizeof(SceneConstants);
    sceneDesc.Usage = D3D11_USAGE_DEFAULT;
    sceneDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    d3d_->CreateBuffer(&sceneDesc, nullptr, &sceneBuffer_);

     D3D11_BUFFER_DESC objectDesc = {};
    objectDesc.ByteWidth = sizeof(ObjectConstants);
    objectDesc.Usage = D3D11_USAGE_DEFAULT;
    objectDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    d3d_->CreateBuffer(&objectDesc, nullptr, &objectBuffer_);

     D3D11_BUFFER_DESC skyDesc = {};
    skyDesc.ByteWidth = sizeof(SkyConstants);
    skyDesc.Usage = D3D11_USAGE_DEFAULT;
    skyDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    d3d_->CreateBuffer(&skyDesc, nullptr, &skyBuffer_);
}

void Renderer::createStates()
{
     D3D11_SAMPLER_DESC samplerDesc = {};
    samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
    samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
    samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
    samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;
    d3d_->CreateSamplerState(&samplerDesc, &linearSampler_);

     D3D11_RASTERIZER_DESC solidDesc = {};
    solidDesc.FillMode = D3D11_FILL_SOLID;
    solidDesc.CullMode = D3D11_CULL_BACK;
    solidDesc.FrontCounterClockwise = FALSE;
    solidDesc.DepthClipEnable = TRUE;
    solidDesc.MultisampleEnable = TRUE;
    d3d_->CreateRasterizerState(&solidDesc, &solidState_);

     D3D11_RASTERIZER_DESC doubleSidedDesc = solidDesc;
    doubleSidedDesc.CullMode = D3D11_CULL_NONE;
    d3d_->CreateRasterizerState(&doubleSidedDesc, &doubleSidedState_);

     D3D11_DEPTH_STENCIL_DESC depthDesc = {};
    depthDesc.DepthEnable = TRUE;
    depthDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
    depthDesc.DepthFunc = D3D11_COMPARISON_LESS;
    d3d_->CreateDepthStencilState(&depthDesc, &depthState_);

     D3D11_DEPTH_STENCIL_DESC skyDepthDesc = {};
    skyDepthDesc.DepthEnable = TRUE;
    skyDepthDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
    skyDepthDesc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
    d3d_->CreateDepthStencilState(&skyDepthDesc, &skyDepthState_);

     D3D11_BLEND_DESC blendDesc = {};
    blendDesc.RenderTarget[0].BlendEnable = FALSE;
    blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    d3d_->CreateBlendState(&blendDesc, &opaqueBlendState_);
}

void Renderer::createDefaultTextures()
{
    const std::uint8_t white[4] = {255u, 255u, 255u, 255u};
    const std::uint8_t mr[4] = {0u, 255u, 0u, 255u};
    const std::uint8_t normal[4] = {128u, 128u, 255u, 255u};

    const std::uint8_t* definitions[3] = {white, mr, normal};
    ID3D11ShaderResourceView** targets[3] = {&defaultBaseSrv_, &defaultMrSrv_, &defaultNormalSrv_};

    for (int index = 0; index < 3; ++index)
    {
        D3D11_TEXTURE2D_DESC desc = {};
        desc.Width = 1u;
        desc.Height = 1u;
        desc.MipLevels = 1u;
        desc.ArraySize = 1u;
        desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.SampleDesc.Count = 1u;
        desc.Usage = D3D11_USAGE_IMMUTABLE;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

        D3D11_SUBRESOURCE_DATA data = {};
        data.pSysMem = definitions[index];
        data.SysMemPitch = 4u;

        ID3D11Texture2D* texture = nullptr;
        d3d_->CreateTexture2D(&desc, &data, &texture);
        d3d_->CreateShaderResourceView(texture, nullptr, targets[index]);
        texture->Release();
    }

    D3D11_TEXTURE2D_DESC emissiveDesc = {};
    emissiveDesc.Width = 1u;
    emissiveDesc.Height = 1u;
    emissiveDesc.MipLevels = 1u;
    emissiveDesc.ArraySize = 1u;
    emissiveDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    emissiveDesc.SampleDesc.Count = 1u;
    emissiveDesc.Usage = D3D11_USAGE_IMMUTABLE;
    emissiveDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA emissiveData = {};
    emissiveData.pSysMem = white;
    emissiveData.SysMemPitch = 4u;

    ID3D11Texture2D* emissiveTexture = nullptr;
    d3d_->CreateTexture2D(&emissiveDesc, &emissiveData, &emissiveTexture);
    d3d_->CreateShaderResourceView(emissiveTexture, nullptr, &defaultEmissiveSrv_);
    emissiveTexture->Release();
}

std::uint32_t Renderer::uploadMesh(const MeshData& source)
{
    PreparedMesh prepared;
    prepareMesh(source, prepared);

    GpuMesh gpuMesh;
    gpuMesh.vertexCount = prepared.vertexCount;
    gpuMesh.indexCount = prepared.indexCount;
    gpuMesh.boundsMin = source.boundsMin;
    gpuMesh.boundsMax = source.boundsMax;

    D3D11_BUFFER_DESC vertexDesc = {};
    vertexDesc.ByteWidth = static_cast<UINT>(prepared.vertices.size());
    vertexDesc.Usage = D3D11_USAGE_IMMUTABLE;
    vertexDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    D3D11_SUBRESOURCE_DATA vertexData = {};
    vertexData.pSysMem = prepared.vertices.data();
    d3d_->CreateBuffer(&vertexDesc, &vertexData, &gpuMesh.vertices);

    D3D11_BUFFER_DESC indexDesc = {};
    indexDesc.ByteWidth = static_cast<UINT>(prepared.indices.size() * sizeof(std::uint32_t));
    indexDesc.Usage = D3D11_USAGE_IMMUTABLE;
    indexDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
    D3D11_SUBRESOURCE_DATA indexData = {};
    indexData.pSysMem = prepared.indices.data();
    d3d_->CreateBuffer(&indexDesc, &indexData, &gpuMesh.indices);

    meshes_.push_back(gpuMesh);
    return static_cast<std::uint32_t>(meshes_.size() - 1u);
}

std::uint32_t Renderer::uploadTexture(const TextureData& source)
{
    GpuTexture gpuTexture;

    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width = source.width;
    desc.Height = source.height;
    desc.MipLevels = source.mipCount;
    desc.ArraySize = 1u;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1u;
    desc.Usage = D3D11_USAGE_IMMUTABLE;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    std::vector<D3D11_SUBRESOURCE_DATA> levels(source.mipCount);
    std::size_t offset = 0u;
    for (std::uint32_t level = 0u; level < source.mipCount; ++level)
    {
        const std::uint32_t levelWidth = textureMipDimension(source.width, level);
        const std::uint32_t levelHeight = textureMipDimension(source.height, level);
        levels[level].pSysMem = source.pixels.data() + offset;
        levels[level].SysMemPitch = levelWidth * 4u;
        offset += static_cast<std::size_t>(levelWidth) * levelHeight * 4u;
    }

    d3d_->CreateTexture2D(&desc, levels.data(), &gpuTexture.texture);
    d3d_->CreateShaderResourceView(gpuTexture.texture, nullptr, &gpuTexture.srv);

    textures_.push_back(gpuTexture);
    return static_cast<std::uint32_t>(textures_.size() - 1u);
}

void Renderer::render()
{
    if (pbrVertexShader_ == nullptr || pbrPixelShader_ == nullptr)
    {
        return;
    }

    const CameraFrame& frame = world_.get<CameraFrame>();
    const DirectionalLight& light = world_.get<DirectionalLight>();

    SceneConstants scene = {};
    scene.viewProjection = frame.viewProjection;
    const DirectX::XMFLOAT3 camPosition = cameraPosition(world_);
    scene.camPosition = {camPosition.x, camPosition.y, camPosition.z, 1.0f};
    scene.sunDirection = {light.direction.x, light.direction.y, light.direction.z, 0.0f};
    scene.sunColor = {light.color.x, light.color.y, light.color.z, light.intensity};
    scene.skyTop = {0.22f, 0.42f, 0.72f, 1.0f};
    scene.skyHorizon = {0.55f, 0.63f, 0.70f, 1.0f};

    context_->UpdateSubresource(sceneBuffer_, 0u, nullptr, &scene, 0u, 0u);
    context_->VSSetConstantBuffers(0u, 1u, &sceneBuffer_);
    context_->PSSetConstantBuffers(0u, 1u, &sceneBuffer_);
    context_->VSSetShader(pbrVertexShader_, nullptr, 0u);
    context_->PSSetShader(pbrPixelShader_, nullptr, 0u);
    context_->IASetInputLayout(inputLayout_);
    context_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    context_->PSSetSamplers(0u, 1u, &linearSampler_);
    context_->OMSetBlendState(opaqueBlendState_, nullptr, 0xFFFFFFFFu);
    context_->OMSetDepthStencilState(depthState_, 0u);

    world_.each([&](flecs::entity entity, Transform& transform, MeshRenderer& renderer,
                    PbrMaterial& material)
    {
        const DirectX::XMMATRIX world = worldMatrix(transform);
        drawMesh(renderer.meshAsset, world, material);
    });

    drawSky();
}

void Renderer::drawMesh(
    std::uint32_t meshIndex,
    const DirectX::XMMATRIX& world,
    const PbrMaterial& material)
{
    if (meshIndex >= meshes_.size())
    {
        return;
    }
    const GpuMesh& mesh = meshes_[meshIndex];

    ObjectConstants object = {};
    math::store(object.world, world);
    object.baseColorFactor = material.baseColorFactor;
    object.mrao = {material.metallicFactor, material.roughnessFactor, material.aoFactor, 0.0f};
    object.emissive = {material.emissiveFactor.x, material.emissiveFactor.y, material.emissiveFactor.z, 1.0f};
    object.hasTextures = {
        material.baseColorTexture >= 0 ? 1.0f : 0.0f,
        material.metallicRoughnessTexture >= 0 ? 1.0f : 0.0f,
        material.normalTexture >= 0 ? 1.0f : 0.0f,
        material.occlusionTexture >= 0 ? 1.0f : 0.0f,
    };

    context_->UpdateSubresource(objectBuffer_, 0u, nullptr, &object, 0u, 0u);
    context_->VSSetConstantBuffers(1u, 1u, &objectBuffer_);
    context_->PSSetConstantBuffers(1u, 1u, &objectBuffer_);

    ID3D11ShaderResourceView* srvs[5] = {
        material.baseColorTexture >= 0 ? textures_[static_cast<std::size_t>(material.baseColorTexture)].srv : defaultBaseSrv_,
        material.metallicRoughnessTexture >= 0 ? textures_[static_cast<std::size_t>(material.metallicRoughnessTexture)].srv : defaultMrSrv_,
        material.normalTexture >= 0 ? textures_[static_cast<std::size_t>(material.normalTexture)].srv : defaultNormalSrv_,
        material.occlusionTexture >= 0 ? textures_[static_cast<std::size_t>(material.occlusionTexture)].srv : defaultBaseSrv_,
        defaultEmissiveSrv_,
    };
    context_->PSSetShaderResources(0u, 5u, srvs);

    if (material.doubleSided)
    {
        context_->RSSetState(doubleSidedState_);
    }
    else
    {
        context_->RSSetState(solidState_);
    }

    context_->IASetVertexBuffers(0u, 1u, &mesh.vertices, &kPreparedStride, &kZeroOffset);
    context_->IASetIndexBuffer(mesh.indices, DXGI_FORMAT_R32_UINT, 0u);
    context_->DrawIndexed(mesh.indexCount, 0u, 0);
}

void Renderer::drawSky()
{
    if (skyMesh_ >= meshes_.size() || skyVertexShader_ == nullptr)
    {
        return;
    }

    const CameraFrame& frame = world_.get<CameraFrame>();
    const DirectionalLight& light = world_.get<DirectionalLight>();

    SkyConstants sky = {};
    sky.viewProjection = makeSkyViewProj(frame);
    const DirectX::XMFLOAT3 camPosition = cameraPosition(world_);
    sky.camPosition = {camPosition.x, camPosition.y, camPosition.z, 1.0f};
    sky.sunDirection = {light.direction.x, light.direction.y, light.direction.z, 0.0f};
    sky.skyTop = {0.22f, 0.42f, 0.72f, 1.0f};
    sky.skyHorizon = {0.55f, 0.63f, 0.70f, 1.0f};

    context_->UpdateSubresource(skyBuffer_, 0u, nullptr, &sky, 0u, 0u);
    context_->VSSetConstantBuffers(0u, 1u, &skyBuffer_);
    context_->PSSetConstantBuffers(0u, 1u, &skyBuffer_);

    context_->VSSetShader(skyVertexShader_, nullptr, 0u);
    context_->PSSetShader(skyPixelShader_, nullptr, 0u);
    context_->OMSetDepthStencilState(skyDepthState_, 0u);
    context_->RSSetState(solidState_);

    const GpuMesh& mesh = meshes_[skyMesh_];
    ID3D11Buffer* vertexBuffers[1] = {mesh.vertices};
    context_->IASetVertexBuffers(0u, 1u, vertexBuffers, &kPreparedStride, &kZeroOffset);
    context_->IASetIndexBuffer(mesh.indices, DXGI_FORMAT_R32_UINT, 0u);
    context_->DrawIndexed(mesh.indexCount, 0u, 0);
}

}