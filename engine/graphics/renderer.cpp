#include "engine/graphics/renderer.hpp"

#include "engine/graphics/shader_compiler.hpp"
#include "engine/scene/camera.hpp"
#include "engine/scene/components.hpp"
#include "engine/shaders/shaders_storage.hpp"

#include <algorithm>
#include <chrono>
#include <cstring>

#include <d3d11.h>
#include <DirectXMath.h>

namespace ksge {

namespace {

constexpr UINT kZeroOffset = 0u;
constexpr std::uint32_t kMaxInstancesPerBatch = 1024u;

float haltonSequence(std::uint32_t base, std::uint32_t index)
{
    float result = 0.0f;
    float fraction = 1.0f;
    while (index > 0u)
    {
        fraction /= static_cast<float>(base);
        result += fraction * static_cast<float>(index % base);
        index /= base;
    }
    return result;
}

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

struct ShadowConstants
{
    DirectX::XMFLOAT4X4 lightViewProjection;
};

std::uint64_t makeInstanceKey(const PbrMaterial& material, std::uint32_t meshIndex)
{
    std::uint64_t key = 0xcbf29ce484222325ull;
    const auto mix = [&](std::uint32_t value)
    {
        key ^= value;
        key *= 0x100000001b3ull;
    };
    const auto mixFloat = [&](float value)
    {
        std::uint32_t bits = 0u;
        std::memcpy(&bits, &value, sizeof(bits));
        mix(bits);
    };

    mixFloat(material.baseColorFactor.x);
    mixFloat(material.baseColorFactor.y);
    mixFloat(material.baseColorFactor.z);
    mixFloat(material.baseColorFactor.w);
    mixFloat(material.emissiveFactor.x);
    mixFloat(material.emissiveFactor.y);
    mixFloat(material.emissiveFactor.z);
    mixFloat(material.metallicFactor);
    mixFloat(material.roughnessFactor);
    mixFloat(material.aoFactor);
    mix(static_cast<std::uint32_t>(material.baseColorTexture));
    mix(static_cast<std::uint32_t>(material.metallicRoughnessTexture));
    mix(static_cast<std::uint32_t>(material.normalTexture));
    mix(static_cast<std::uint32_t>(material.occlusionTexture));
    mix(material.doubleSided ? 1u : 0u);
    mix(meshIndex);
    return key;
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

    postProcess_.attach(d3d_, context_);
    createPipeline();
    createStates();
    createDefaultTextures();

    D3D11_BUFFER_DESC instanceDesc = {};
    instanceDesc.ByteWidth = static_cast<UINT>(kMaxInstancesPerBatch * sizeof(DirectX::XMFLOAT4X4));
    instanceDesc.Usage = D3D11_USAGE_DEFAULT;
    instanceDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    instanceDesc.StructureByteStride = sizeof(DirectX::XMFLOAT4X4);
    d3d_->CreateBuffer(&instanceDesc, nullptr, &instanceBuffer_);

    D3D11_SHADER_RESOURCE_VIEW_DESC instanceViewDesc = {};
    instanceViewDesc.Format = DXGI_FORMAT_UNKNOWN;
    instanceViewDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
    instanceViewDesc.Buffer.FirstElement = 0u;
    instanceViewDesc.Buffer.NumElements = kMaxInstancesPerBatch;
    d3d_->CreateShaderResourceView(instanceBuffer_, &instanceViewDesc, &instanceView_);

    instancedMatrices_.resize(kMaxInstancesPerBatch);
    gridMesh_ = uploadMesh(makeQuad(40.0f, 40.0f));

    D3D11_QUERY_DESC disjointDesc = {};
    disjointDesc.Query = D3D11_QUERY_TIMESTAMP_DISJOINT;
    d3d_->CreateQuery(&disjointDesc, &disjointQuery_);

    D3D11_QUERY_DESC timestampDesc = {};
    timestampDesc.Query = D3D11_QUERY_TIMESTAMP;
    d3d_->CreateQuery(&timestampDesc, &timestampStart_);
    d3d_->CreateQuery(&timestampDesc, &timestampEnd_);
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
    if (timestampEnd_)
    {
        timestampEnd_->Release();
    }
    if (timestampStart_)
    {
        timestampStart_->Release();
    }
    if (disjointQuery_)
    {
        disjointQuery_->Release();
    }
    if (instanceView_)
    {
        instanceView_->Release();
    }
    if (instanceBuffer_)
    {
        instanceBuffer_->Release();
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
    if (shadowBuffer_)
    {
        shadowBuffer_->Release();
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
    if (shadowPixelShader_)
    {
        shadowPixelShader_->Release();
    }
    if (shadowVertexShader_)
    {
        shadowVertexShader_->Release();
    }
    if (gridPixelShader_)
    {
        gridPixelShader_->Release();
    }
    if (gridVertexShader_)
    {
        gridVertexShader_->Release();
    }
    if (gbufferPixelShader_)
    {
        gbufferPixelShader_->Release();
    }
    if (gbufferVertexShader_)
    {
        gbufferVertexShader_->Release();
    }
}

void Renderer::createPipeline()
{
    std::string error;

    ID3DBlob* vertexBytecode = nullptr;
    if (!createVertexShader(d3d_, shaders::kGBufferInstancedVertex, gbufferVertexShader_, vertexBytecode, error) ||
        !createPixelShader(d3d_, shaders::kGBufferPixel, gbufferPixelShader_, error))
    {
        gbufferVertexShader_ = nullptr;
        gbufferPixelShader_ = nullptr;
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

    ID3DBlob* gridBytecode = nullptr;
    if (!createVertexShader(d3d_, shaders::kPbrVertex, gridVertexShader_, gridBytecode, error) ||
        !createPixelShader(d3d_, shaders::kGridPixel, gridPixelShader_, error))
    {
        gridVertexShader_ = nullptr;
        gridPixelShader_ = nullptr;
    }
    if (gridBytecode)
    {
        gridBytecode->Release();
    }

    ID3DBlob* shadowBytecode = nullptr;
    if (!createVertexShader(d3d_, shaders::kShadowInstancedVertex, shadowVertexShader_, shadowBytecode, error) ||
        !createPixelShader(d3d_, shaders::kShadowPixel, shadowPixelShader_, error))
    {
        shadowVertexShader_ = nullptr;
        shadowPixelShader_ = nullptr;
    }
    if (shadowBytecode)
    {
        shadowBytecode->Release();
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

    D3D11_BUFFER_DESC shadowDesc = {};
    shadowDesc.ByteWidth = sizeof(ShadowConstants);
    shadowDesc.Usage = D3D11_USAGE_DEFAULT;
    shadowDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    d3d_->CreateBuffer(&shadowDesc, nullptr, &shadowBuffer_);
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
    solidDesc.CullMode = D3D11_CULL_NONE;
    solidDesc.FrontCounterClockwise = TRUE;
    solidDesc.DepthClipEnable = TRUE;
    solidDesc.MultisampleEnable = TRUE;
    d3d_->CreateRasterizerState(&solidDesc, &solidState_);

    const D3D11_RASTERIZER_DESC doubleSidedDesc = solidDesc;
    d3d_->CreateRasterizerState(&doubleSidedDesc, &doubleSidedState_);

    D3D11_DEPTH_STENCIL_DESC depthDesc = {};
    depthDesc.DepthEnable = TRUE;
    depthDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
    depthDesc.DepthFunc = D3D11_COMPARISON_LESS;
    d3d_->CreateDepthStencilState(&depthDesc, &depthState_);

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

void Renderer::bindMesh(std::uint32_t meshIndex)
{
    const GpuMesh& mesh = meshes_[meshIndex];
    context_->IASetVertexBuffers(0u, 1u, &mesh.vertices, &kPreparedStride, &kZeroOffset);
    context_->IASetIndexBuffer(mesh.indices, DXGI_FORMAT_R32_UINT, 0u);
}

void Renderer::buildEntries(const Frustum& frustum)
{
    gbufferEntries_.clear();
    shadowEntries_.clear();

    const DirectX::XMFLOAT3 cameraFloat = cameraPosition(world_);
    const DirectX::XMVECTOR camera = DirectX::XMLoadFloat3(&cameraFloat);

    world_.each([&](Transform& transform, MeshRenderer& meshRenderer, PbrMaterial& material)
    {
        ++frameGathered_;
        if (meshRenderer.meshAsset >= meshes_.size())
        {
            return;
        }
        const DirectX::XMMATRIX world = worldMatrix(transform);
        const GpuMesh& mesh = meshes_[meshRenderer.meshAsset];

        const DirectX::XMVECTOR localMin = DirectX::XMLoadFloat3(&mesh.boundsMin);
        const DirectX::XMVECTOR localMax = DirectX::XMLoadFloat3(&mesh.boundsMax);
        const DirectX::XMVECTOR localCenter = DirectX::XMVectorMultiply(
            DirectX::XMVectorAdd(localMin, localMax), DirectX::XMVectorReplicate(0.5f));
        const DirectX::XMVECTOR halfExtents = DirectX::XMVectorMultiply(
            DirectX::XMVectorSubtract(localMax, localMin), DirectX::XMVectorReplicate(0.5f));
        const DirectX::XMVECTOR worldPosition = DirectX::XMVector3TransformCoord(localCenter, world);

        std::uint32_t drawMesh = meshRenderer.meshAsset;
        if (!lodChains_.empty() && meshRenderer.meshAsset < lodChains_.size())
        {
            const DirectX::XMVECTOR distanceVector = DirectX::XMVectorSubtract(worldPosition, camera);
            const float distance = DirectX::XMVectorGetX(DirectX::XMVector3LengthEst(distanceVector));
            const std::uint32_t lod = distance > 160.0f ? 2u : (distance > 80.0f ? 1u : 0u);
            if (lod > 0u)
            {
                drawMesh = lodChains_[meshRenderer.meshAsset][lod];
            }
        }

        math::Vec3 center;
        DirectX::XMStoreFloat3(&center, worldPosition);
        const math::Vec3 boundsHalf = {
            DirectX::XMVectorGetX(halfExtents),
            DirectX::XMVectorGetY(halfExtents),
            DirectX::XMVectorGetZ(halfExtents),
        };

        if (ksge::intersects(frustum, center, boundsHalf))
        {
            DrawEntry entry;
            entry.meshIndex = drawMesh;
            entry.key = makeInstanceKey(material, drawMesh);
            entry.material = &material;
            math::store(entry.world, world);
            gbufferEntries_.push_back(entry);
            ++frameGbufferPushed_;
        }

        DrawEntry shadowEntry;
        shadowEntry.meshIndex = drawMesh;
        shadowEntry.key = drawMesh;
        shadowEntry.material = nullptr;
        math::store(shadowEntry.world, world);
        shadowEntries_.push_back(shadowEntry);
        ++frameShadowPushed_;
    });

    std::sort(gbufferEntries_.begin(), gbufferEntries_.end(),
        [](const DrawEntry& a, const DrawEntry& b) { return a.key < b.key; });
    std::sort(shadowEntries_.begin(), shadowEntries_.end(),
        [](const DrawEntry& a, const DrawEntry& b) { return a.key < b.key; });
}

void Renderer::drawObjectBatchRange(
    const std::vector<DrawEntry>& entries,
    std::size_t first,
    std::size_t after,
    bool shadow)
{
    std::size_t groupStart = first;
    while (groupStart < after)
    {
        const std::size_t groupEnd = groupStart + 1u;
        const std::uint64_t groupKey = entries[groupStart].key;
        std::size_t cursor = groupStart + 1u;
        while (cursor < after && entries[cursor].key == groupKey)
        {
            ++cursor;
        }

        if (!shadow)
        {
            const PbrMaterial& material = *entries[groupStart].material;
            ObjectConstants object = {};
            math::store(object.world, DirectX::XMMatrixIdentity());
            object.baseColorFactor = material.baseColorFactor;
            object.mrao = {material.metallicFactor, material.roughnessFactor, material.aoFactor, 0.0f};
            object.emissive = {
                material.emissiveFactor.x, material.emissiveFactor.y, material.emissiveFactor.z, 1.0f};
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
                material.baseColorTexture >= 0 &&
                        static_cast<std::uint32_t>(material.baseColorTexture) < textures_.size()
                    ? textures_[static_cast<std::size_t>(material.baseColorTexture)].srv
                    : defaultBaseSrv_,
                material.metallicRoughnessTexture >= 0 &&
                        static_cast<std::uint32_t>(material.metallicRoughnessTexture) < textures_.size()
                    ? textures_[static_cast<std::size_t>(material.metallicRoughnessTexture)].srv
                    : defaultMrSrv_,
                material.normalTexture >= 0 &&
                        static_cast<std::uint32_t>(material.normalTexture) < textures_.size()
                    ? textures_[static_cast<std::size_t>(material.normalTexture)].srv
                    : defaultNormalSrv_,
                material.occlusionTexture >= 0 &&
                        static_cast<std::uint32_t>(material.occlusionTexture) < textures_.size()
                    ? textures_[static_cast<std::size_t>(material.occlusionTexture)].srv
                    : defaultBaseSrv_,
                defaultEmissiveSrv_,
            };
            context_->PSSetShaderResources(0u, 5u, srvs);

            context_->RSSetState(material.doubleSided ? doubleSidedState_ : solidState_);
        }
        else
        {
            context_->RSSetState(solidState_);
        }

        std::size_t base = groupStart;
        while (base < groupEnd)
        {
            const std::size_t remaining = groupEnd - base;
            const std::uint32_t instanceCount =
                static_cast<std::uint32_t>(std::min<std::size_t>(kMaxInstancesPerBatch, remaining));
            for (std::uint32_t index = 0u; index < instanceCount; ++index)
            {
                instancedMatrices_[index] = entries[base + index].world;
            }
            context_->UpdateSubresource(instanceBuffer_, 0u, nullptr, instancedMatrices_.data(), 0u, 0u);

            const GpuMesh& mesh = meshes_[entries[base].meshIndex];
            bindMesh(entries[base].meshIndex);
            context_->DrawIndexedInstanced(mesh.indexCount, instanceCount, 0u, 0, 0);
            ++frameDraws_;
            frameInstances_ += instanceCount;
            if (shadow)
            {
                ++shadowDraws_;
                shadowInstances_ += instanceCount;
            }
            else
            {
                ++gbufferDraws_;
                gbufferInstances_ += instanceCount;
            }
            base += instanceCount;
        }

        groupStart = cursor;
    }
}

void Renderer::drawGBufferBatches()
{
    context_->VSSetShader(gbufferVertexShader_, nullptr, 0u);
    context_->PSSetShader(gbufferPixelShader_, nullptr, 0u);
    context_->IASetInputLayout(inputLayout_);
    context_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    context_->VSSetShaderResources(0u, 1u, &instanceView_);
    drawObjectBatchRange(gbufferEntries_, 0u, gbufferEntries_.size(), false);
}

void Renderer::drawShadowBatches()
{
    context_->VSSetShader(shadowVertexShader_, nullptr, 0u);
    context_->PSSetShader(shadowPixelShader_, nullptr, 0u);
    context_->IASetInputLayout(inputLayout_);
    context_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    context_->VSSetShaderResources(0u, 1u, &instanceView_);
    drawObjectBatchRange(shadowEntries_, 0u, shadowEntries_.size(), true);
}

void Renderer::drawGrid(const DirectX::XMFLOAT4X4& baseViewProjection)
{
    if (gridVertexShader_ == nullptr || gridPixelShader_ == nullptr || gridMesh_ >= meshes_.size())
    {
        return;
    }

    SceneConstants gridScene = {};
    gridScene.viewProjection = baseViewProjection;
    const DirectX::XMFLOAT3 camPosition = cameraPosition(world_);
    gridScene.camPosition = {camPosition.x, camPosition.y, camPosition.z, 1.0f};
    gridScene.sunDirection = {0.5f, 0.8f, 0.6f, 0.0f};
    gridScene.sunColor = {1.0f, 0.95f, 0.85f, 1.0f};
    gridScene.skyTop = {0.22f, 0.42f, 0.72f, 1.0f};
    gridScene.skyHorizon = {0.55f, 0.63f, 0.70f, 1.0f};
    context_->UpdateSubresource(sceneBuffer_, 0u, nullptr, &gridScene, 0u, 0u);

    ObjectConstants object = {};
    math::store(object.world, DirectX::XMMatrixTranslation(0.0f, -0.5f, 0.0f));
    context_->UpdateSubresource(objectBuffer_, 0u, nullptr, &object, 0u, 0u);
    context_->VSSetConstantBuffers(1u, 1u, &objectBuffer_);
    context_->PSSetConstantBuffers(1u, 1u, &objectBuffer_);

    ID3D11ShaderResourceView* nullResources[5] = {};
    context_->PSSetShaderResources(0u, 5u, nullResources);

    context_->VSSetShader(gridVertexShader_, nullptr, 0u);
    context_->PSSetShader(gridPixelShader_, nullptr, 0u);
    context_->RSSetState(solidState_);

    bindMesh(gridMesh_);
    const GpuMesh& mesh = meshes_[gridMesh_];
    context_->DrawIndexed(mesh.indexCount, 0u, 0);
    ++frameDraws_;
}

void Renderer::render()
{
    const auto frameStart = std::chrono::steady_clock::now();
    d3d_ = device_.device();
    context_ = device_.context();
    if (gbufferVertexShader_ == nullptr || gbufferPixelShader_ == nullptr)
    {
        return;
    }
    postProcess_.attach(d3d_, context_);
    frameDraws_ = 0u;
    frameInstances_ = 0u;
    gbufferDraws_ = 0u;
    gbufferInstances_ = 0u;
    shadowDraws_ = 0u;
    shadowInstances_ = 0u;
    frameGathered_ = 0u;
    frameShadowPushed_ = 0u;
    frameGbufferPushed_ = 0u;

    if (disjointQuery_)
    {
        context_->Begin(disjointQuery_);
    }

    const CameraFrame& frame = world_.get<CameraFrame>();
    const DirectionalLight& light = world_.get<DirectionalLight>();
    const flecs::entity cameraEntity = world_.entity("editor_camera");
    const bool hasCamera = cameraEntity.has<Camera>();
    const float nearPlane = hasCamera ? cameraEntity.get<Camera>().nearPlane : 0.1f;
    const float farPlane = hasCamera ? cameraEntity.get<Camera>().farPlane : 5000.0f;
    const DirectX::XMFLOAT3 camPosition = cameraPosition(world_);

    float cascadeSplits[kShadowCascades + 1u];
    computeCascadeSplits(nearPlane, farPlane, cascadeSplits);
    const DirectX::XMFLOAT3 lightDirection = {light.direction.x, light.direction.y, light.direction.z};
    computeCascadeMatrices(frame, lightDirection, nearPlane, farPlane, cascadeSplits, shadowViewProjection_);

    Frustum frustum;
    extractFrustum(frame, frustum);
    buildEntries(frustum);

    if (timestampStart_)
    {
        context_->End(timestampStart_);
    }

    for (std::uint32_t cascade = 0u; cascade < kShadowCascades; ++cascade)
    {
        postProcess_.beginShadowMap(cascade);
        ShadowConstants shadow = {};
        shadow.lightViewProjection = shadowViewProjection_[cascade];
        context_->UpdateSubresource(shadowBuffer_, 0u, nullptr, &shadow, 0u, 0u);
        context_->VSSetConstantBuffers(0u, 1u, &shadowBuffer_);
        context_->OMSetDepthStencilState(depthState_, 0u);
        context_->OMSetBlendState(opaqueBlendState_, nullptr, 0xFFFFFFFFu);
        drawShadowBatches();
    }
    postProcess_.endShadowMap();

    postProcess_.beginScene(device_.width(), device_.height());

    const DirectX::XMMATRIX baseViewProjection = math::load(frame.viewProjection);
    DirectX::XMFLOAT4X4 baseViewProjectionStored;
    math::store(baseViewProjectionStored, baseViewProjection);
    DirectX::XMMATRIX displayViewProjection = baseViewProjection;
    if (debugMode_ == 0u)
    {
        const float jitterX = (haltonSequence(2u, frameIndex_) - 0.5f) * 2.0f /
            static_cast<float>(device_.width());
        const float jitterY = (haltonSequence(3u, frameIndex_) - 0.5f) * 2.0f /
            static_cast<float>(device_.height());
        const DirectX::XMMATRIX jitterMatrix = DirectX::XMMatrixSet(
            1.0f, 0.0f, 0.0f, 0.0f,
            0.0f, 1.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 1.0f, 0.0f,
            jitterX, jitterY, 0.0f, 1.0f);
        displayViewProjection = DirectX::XMMatrixMultiply(baseViewProjection, jitterMatrix);
    }
    ++frameIndex_;

    SceneConstants scene = {};
    math::store(scene.viewProjection, displayViewProjection);
    scene.camPosition = {camPosition.x, camPosition.y, camPosition.z, 1.0f};
    scene.sunDirection = {light.direction.x, light.direction.y, light.direction.z, 0.0f};
    scene.sunColor = {light.color.x, light.color.y, light.color.z, light.intensity};
    scene.skyTop = {0.22f, 0.42f, 0.72f, 1.0f};
    scene.skyHorizon = {0.55f, 0.63f, 0.70f, 1.0f};

    context_->UpdateSubresource(sceneBuffer_, 0u, nullptr, &scene, 0u, 0u);
    context_->VSSetConstantBuffers(0u, 1u, &sceneBuffer_);
    context_->PSSetConstantBuffers(0u, 1u, &sceneBuffer_);
    context_->PSSetSamplers(0u, 1u, &linearSampler_);
    context_->OMSetBlendState(opaqueBlendState_, nullptr, 0xFFFFFFFFu);
    context_->OMSetDepthStencilState(depthState_, 0u);

    drawGBufferBatches();
    drawGrid(baseViewProjectionStored);
    postProcess_.endScene();

    PostFrameInfo info = {};
    info.width = device_.width();
    info.height = device_.height();
    math::store(info.viewProjection, displayViewProjection);
    DirectX::XMVECTOR determinant = DirectX::XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f);
    const DirectX::XMMATRIX inverse = DirectX::XMMatrixInverse(&determinant, displayViewProjection);
    math::store(info.inverseViewProjection, inverse);
    math::store(info.previousViewProjection, math::load(previousViewProjection_));
    math::store(previousViewProjection_, displayViewProjection);
    info.cameraPosition = {camPosition.x, camPosition.y, camPosition.z};
    info.nearPlane = nearPlane;
    info.farPlane = farPlane;
    info.sunDirection = {light.direction.x, light.direction.y, light.direction.z};
    info.sunIntensity = light.intensity;
    info.sunColor = light.color;
    for (std::uint32_t cascade = 0u; cascade < kShadowCascades; ++cascade)
    {
        info.shadowViewProjection[cascade] = shadowViewProjection_[cascade];
    }
    info.cascadeSplits[0] = cascadeSplits[1];
    info.cascadeSplits[1] = cascadeSplits[2];
    info.cascadeSplits[2] = cascadeSplits[3];
    info.shadowMapSize = 1024.0f;
    info.shadowBlendWidth = 20.0f;
    info.shadowDepthBias = 0.004f;
    info.exposure = 1.0f;
    info.debugMode = debugMode_;

    postProcess_.run(info, device_.renderTarget());

    if (disjointQuery_ && timestampStart_ && timestampEnd_)
    {
        context_->End(timestampEnd_);
        context_->End(disjointQuery_);
    }

    const auto frameEnd = std::chrono::steady_clock::now();
    frameCpuMs_ = std::chrono::duration<float, std::milli>(frameEnd - frameStart).count();
}

void Renderer::updateGpuTime()
{
    if (disjointQuery_ == nullptr || timestampStart_ == nullptr || timestampEnd_ == nullptr)
    {
        return;
    }
    context_->Flush();
    D3D11_QUERY_DATA_TIMESTAMP_DISJOINT disjoint = {};
    if (context_->GetData(disjointQuery_, &disjoint, sizeof(disjoint), 0u) != S_OK ||
        disjoint.Disjoint)
    {
        return;
    }
    std::uint64_t startTicks = 0u;
    std::uint64_t endTicks = 0u;
    if (context_->GetData(timestampStart_, &startTicks, sizeof(startTicks), 0u) != S_OK ||
        context_->GetData(timestampEnd_, &endTicks, sizeof(endTicks), 0u) != S_OK)
    {
        return;
    }
    const double frequency = static_cast<double>(disjoint.Frequency);
    frameGpuMs_ = static_cast<float>((static_cast<double>(endTicks - startTicks) / frequency) * 1000.0);
}

void Renderer::setLodChain(std::uint32_t meshIndex, std::uint32_t mediumMesh, std::uint32_t lowMesh)
{
    if (meshIndex >= meshes_.size() || mediumMesh >= meshes_.size() || lowMesh >= meshes_.size())
    {
        return;
    }
    lodChains_.resize(meshes_.size());
    lodChains_[meshIndex] = {meshIndex, mediumMesh, lowMesh};
}

void Renderer::setDebugMode(std::uint32_t mode)
{
    debugMode_ = mode & 0x7u;
}

std::uint32_t Renderer::frameDraws() const
{
    return frameDraws_;
}

std::uint32_t Renderer::frameInstances() const
{
    return frameInstances_;
}

std::uint32_t Renderer::gbufferDraws() const
{
    return gbufferDraws_;
}

std::uint32_t Renderer::gbufferInstances() const
{
    return gbufferInstances_;
}

std::uint32_t Renderer::shadowDraws() const
{
    return shadowDraws_;
}

std::uint32_t Renderer::shadowInstances() const
{
    return shadowInstances_;
}

float Renderer::frameCpuMs() const
{
    return frameCpuMs_;
}

float Renderer::frameGpuMs() const
{
    return frameGpuMs_;
}

std::uint32_t Renderer::frameGathered() const
{
    return frameGathered_;
}

std::uint32_t Renderer::framePushed() const
{
    return frameShadowPushed_;
}

std::uint32_t Renderer::gbufferPushed() const
{
    return frameGbufferPushed_;
}

}