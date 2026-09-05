#include "engine/graphics/postprocess.hpp"

#include "engine/graphics/shader_compiler.hpp"
#include "engine/shaders/shaders_storage.hpp"

#include <algorithm>
#include <cmath>

namespace ksge {

namespace {

struct PostConstants
{
    DirectX::XMFLOAT4X4 viewProjection;
    DirectX::XMFLOAT4X4 inverseViewProjection;
    DirectX::XMFLOAT4 cameraPosition;
    DirectX::XMFLOAT4 viewport;
    DirectX::XMFLOAT4 targetSize;
    DirectX::XMFLOAT4 debugView;
    DirectX::XMFLOAT4 cameraNearFar;
    DirectX::XMFLOAT4 sun;
    DirectX::XMFLOAT4 sunColor;
    DirectX::XMFLOAT4 fogParams;
    DirectX::XMFLOAT4 ssaoParams;
    DirectX::XMFLOAT4 bloomParams;
    DirectX::XMFLOAT4 compositeParams;
};

constexpr UINT kFullscreenVertexCount = 3u;
constexpr std::uint32_t kSsaoSamples = 16u;
constexpr UINT kMaxBinds = 8u;
constexpr std::uint32_t kMinTargetSize = 1u;

std::uint32_t halve(std::uint32_t value)
{
    return std::max(kMinTargetSize, (value + 1u) / 2u);
}

std::uint32_t quarter(std::uint32_t value)
{
    return std::max(kMinTargetSize, (value + 3u) / 4u);
}

std::uint32_t eighth(std::uint32_t value)
{
    return std::max(kMinTargetSize, (value + 7u) / 8u);
}

std::uint32_t sixteenth(std::uint32_t value)
{
    return std::max(kMinTargetSize, (value + 15u) / 16u);
}

float saturateFloat(float value)
{
    return value < 0.0f ? 0.0f : (value > 1.0f ? 1.0f : value);
}

void defaultViewport(D3D11_VIEWPORT& viewport)
{
    viewport.TopLeftX = 0.0f;
    viewport.TopLeftY = 0.0f;
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;
}

}

void generateGradingLut(float* rgbaOut, std::uint32_t size, const GradingParams& params)
{
    if (rgbaOut == nullptr || size < 2u)
    {
        return;
    }
    const float maxIndex = static_cast<float>(size - 1u);
    for (std::uint32_t b = 0u; b < size; ++b)
    {
        for (std::uint32_t g = 0u; g < size; ++g)
        {
            for (std::uint32_t r = 0u; r < size; ++r)
            {
                const float u = static_cast<float>(r) / maxIndex;
                const float v = static_cast<float>(g) / maxIndex;
                const float w = static_cast<float>(b) / maxIndex;

                float color[3] = {u, v, w};
                for (int channel = 0; channel < 3; ++channel)
                {
                    color[channel] *= params.exposure;
                    color[channel] = (color[channel] - 0.5f) * params.contrast + 0.5f;
                    color[channel] = saturateFloat(color[channel]);
                }

                const float luma =
                    color[0] * 0.2126f + color[1] * 0.7152f + color[2] * 0.0722f;
                for (int channel = 0; channel < 3; ++channel)
                {
                    color[channel] = luma + (color[channel] - luma) * params.saturation;
                }

                float* pixel = rgbaOut + (static_cast<std::size_t>(b) * size + g) * size * 4u + r * 4u;
                pixel[0] = color[0];
                pixel[1] = color[1];
                pixel[2] = color[2];
                pixel[3] = 1.0f;
            }
        }
    }
}

void PostProcess::attach(ID3D11Device* device, ID3D11DeviceContext* context)
{
    d3d_ = device;
    context_ = context;
    createdDevice_ = device;
}

PostProcess::~PostProcess()
{
    releaseTargets();
}

void PostProcess::beginScene(std::uint32_t width, std::uint32_t height)
{
    if (d3d_ == nullptr || context_ == nullptr)
    {
        return;
    }
    ensureTargets(width, height);
    if (sceneColor_.rtv == nullptr || depth_.dsv == nullptr)
    {
        return;
    }

    ID3D11RenderTargetView* renderTarget = sceneColor_.rtv;
    context_->OMSetRenderTargets(1u, &renderTarget, depth_.dsv);
    const float clearColor[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    context_->ClearRenderTargetView(sceneColor_.rtv, clearColor);
    context_->ClearDepthStencilView(depth_.dsv, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0u);

    D3D11_VIEWPORT viewport = {};
    defaultViewport(viewport);
    viewport.Width = static_cast<float>(width);
    viewport.Height = static_cast<float>(height);
    context_->RSSetViewports(1u, &viewport);
}

void PostProcess::endScene()
{
    if (context_ == nullptr)
    {
        return;
    }
    ID3D11RenderTargetView* nullRenderTarget = nullptr;
    context_->OMSetRenderTargets(1u, &nullRenderTarget, nullptr);
    clearResources();
}

void PostProcess::run(const PostFrameInfo& info, ID3D11RenderTargetView* backbuffer)
{
    if (d3d_ == nullptr || context_ == nullptr || backbuffer == nullptr)
    {
        return;
    }
    ensureTargets(info.width, info.height);
    if (sceneColor_.srv == nullptr)
    {
        return;
    }
    if (compositeShader_ == nullptr || lutView_ == nullptr ||
        ssaoRaw_.srv == nullptr || fog_.srv == nullptr ||
        bloomBase_.srv == nullptr || depth_.srv == nullptr)
    {
        if (passthroughShader_ != nullptr && sceneColor_.srv != nullptr)
        {
            beginPass(backbuffer, static_cast<float>(width_), static_cast<float>(height_), false, passthroughShader_);
            ID3D11ShaderResourceView* sceneResource[1] = {sceneColor_.srv};
            context_->PSSetShaderResources(0u, 1u, sceneResource);
            drawFullscreen();
            clearResources();
        }
        return;
    }

    postViewProj_ = info.viewProjection;
    postInvViewProj_ = info.inverseViewProjection;
    postCameraPos_ = info.cameraPosition;
    postNear_ = info.nearPlane;
    postFar_ = info.farPlane;
    postSunDir_ = info.sunDirection;
    postSunIntensity_ = info.sunIntensity;
    postSunColor_ = info.sunColor;
    postExposure_ = info.exposure;
    postDebugMode_ = info.debugMode;

    updateGradedLut();

    if (ssaoShader_ == nullptr)
    {
        const float neutral[4] = {1.0f, 1.0f, 1.0f, 1.0f};
        context_->ClearRenderTargetView(ssaoRaw_.rtv, neutral);
        context_->ClearRenderTargetView(ssaoBlur_.rtv, neutral);
    }
    if (fogShader_ == nullptr && fog_.rtv != nullptr)
    {
        const float neutral[4] = {0.0f, 0.0f, 0.0f, 1.0f};
        context_->ClearRenderTargetView(fog_.rtv, neutral);
    }
    if (bloomExtractShader_ == nullptr && bloomBase_.rtv != nullptr)
    {
        const float neutral[4] = {0.0f, 0.0f, 0.0f, 1.0f};
        context_->ClearRenderTargetView(bloomBase_.rtv, neutral);
        context_->ClearRenderTargetView(bloomMip1_.rtv, neutral);
        context_->ClearRenderTargetView(bloomMip2_.rtv, neutral);
        context_->ClearRenderTargetView(bloomAccum_.rtv, neutral);
        context_->ClearRenderTargetView(bloomTemp_.rtv, neutral);
    }

    applySsao();
    applySsaoBlur();
    applyFog();
    applyBloom();
    applyComposite(backbuffer);
}

void PostProcess::applySsao()
{
    if (ssaoShader_ == nullptr || ssaoRaw_.rtv == nullptr)
    {
        return;
    }
    const std::uint32_t halfWidth = halve(width_);
    const std::uint32_t halfHeight = halve(height_);

    beginPass(ssaoRaw_.rtv, static_cast<float>(halfWidth), static_cast<float>(halfHeight), false, ssaoShader_);
    uploadConstants(halfWidth, halfHeight);
    ID3D11ShaderResourceView* resources[2] = {depth_.srv, noiseView_};
    context_->PSSetShaderResources(0u, 2u, resources);
    drawFullscreen();
    clearResources();
}

void PostProcess::applySsaoBlur()
{
    if (ssaoBlurHShader_ == nullptr || ssaoBlurVShader_ == nullptr ||
        ssaoRaw_.rtv == nullptr || ssaoBlur_.rtv == nullptr)
    {
        return;
    }
    const std::uint32_t halfWidth = halve(width_);
    const std::uint32_t halfHeight = halve(height_);

    beginPass(ssaoBlur_.rtv, static_cast<float>(halfWidth), static_cast<float>(halfHeight), false, ssaoBlurHShader_);
    uploadConstants(halfWidth, halfHeight);
    ID3D11ShaderResourceView* horizontalResource[2] = {ssaoRaw_.srv, depth_.srv};
    context_->PSSetShaderResources(0u, 2u, horizontalResource);
    drawFullscreen();
    clearResources();

    beginPass(ssaoRaw_.rtv, static_cast<float>(halfWidth), static_cast<float>(halfHeight), false, ssaoBlurVShader_);
    uploadConstants(halfWidth, halfHeight);
    ID3D11ShaderResourceView* verticalResource[2] = {ssaoBlur_.srv, depth_.srv};
    context_->PSSetShaderResources(0u, 2u, verticalResource);
    drawFullscreen();
    clearResources();
}

void PostProcess::applyFog()
{
    if (fogShader_ == nullptr || fog_.rtv == nullptr)
    {
        return;
    }
    const std::uint32_t halfWidth = halve(width_);
    const std::uint32_t halfHeight = halve(height_);

    beginPass(fog_.rtv, static_cast<float>(halfWidth), static_cast<float>(halfHeight), false, fogShader_);
    uploadConstants(halfWidth, halfHeight);

    ID3D11ShaderResourceView* resources[1] = {depth_.srv};
    context_->PSSetShaderResources(0u, 1u, resources);
    drawFullscreen();
    clearResources();
}

void PostProcess::applyBloom()
{
    if (bloomExtractShader_ == nullptr || bloomBase_.rtv == nullptr)
    {
        return;
    }
    const std::uint32_t quarterWidth = quarter(width_);
    const std::uint32_t quarterHeight = quarter(height_);
    const std::uint32_t eighthWidth = eighth(width_);
    const std::uint32_t eighthHeight = eighth(height_);
    const std::uint32_t sixteenthWidth = sixteenth(width_);
    const std::uint32_t sixteenthHeight = sixteenth(height_);

    beginPass(bloomBase_.rtv, static_cast<float>(quarterWidth), static_cast<float>(quarterHeight), false, bloomExtractShader_);
    uploadConstants(quarterWidth, quarterHeight);
    ID3D11ShaderResourceView* extractResource[1] = {sceneColor_.srv};
    context_->PSSetShaderResources(0u, 1u, extractResource);
    drawFullscreen();
    clearResources();

    beginPass(bloomMip1_.rtv, static_cast<float>(eighthWidth), static_cast<float>(eighthHeight), false, bloomDownsampleShader_);
    uploadConstants(quarterWidth, quarterHeight);
    ID3D11ShaderResourceView* downResource[1] = {bloomBase_.srv};
    context_->PSSetShaderResources(0u, 1u, downResource);
    drawFullscreen();
    clearResources();

beginPass(bloomMip2_.rtv, static_cast<float>(sixteenthWidth), static_cast<float>(sixteenthHeight), false, bloomDownsampleShader_);
    uploadConstants(eighthWidth, eighthHeight);
    ID3D11ShaderResourceView* downResource2[1] = {bloomMip1_.srv};
    context_->PSSetShaderResources(0u, 1u, downResource2);
    drawFullscreen();
    clearResources();

    if (bloomBlurHShader_ != nullptr && bloomBlurVShader_ != nullptr && bloomTemp_.rtv != nullptr)
    {
        beginPass(bloomTemp_.rtv, static_cast<float>(sixteenthWidth), static_cast<float>(sixteenthHeight), false, bloomBlurHShader_);
        uploadConstants(sixteenthWidth, sixteenthHeight);
        ID3D11ShaderResourceView* blurResource[1] = {bloomMip2_.srv};
        context_->PSSetShaderResources(0u, 1u, blurResource);
        drawFullscreen();
        clearResources();

beginPass(bloomMip2_.rtv, static_cast<float>(sixteenthWidth), static_cast<float>(sixteenthHeight), false, bloomBlurVShader_);
        uploadConstants(sixteenthWidth, sixteenthHeight);
        ID3D11ShaderResourceView* blurResource2[1] = {bloomTemp_.srv};
        context_->PSSetShaderResources(0u, 1u, blurResource2);
        drawFullscreen();
        clearResources();
    }

    if (bloomUpsampleShader_ != nullptr && bloomAccum_.rtv != nullptr)
    {
        beginPass(bloomAccum_.rtv, static_cast<float>(eighthWidth), static_cast<float>(eighthHeight), false, bloomUpsampleShader_);
        uploadConstants(eighthWidth, eighthHeight);
        ID3D11ShaderResourceView* upsampledResources[2] = {bloomMip1_.srv, bloomMip2_.srv};
        context_->PSSetShaderResources(0u, 2u, upsampledResources);
        drawFullscreen();
        clearResources();
    }
}

void PostProcess::applyComposite(ID3D11RenderTargetView* backbuffer)
{
    if (compositeShader_ == nullptr || backbuffer == nullptr ||
        sceneColor_.srv == nullptr || ssaoRaw_.srv == nullptr || fog_.srv == nullptr ||
        bloomBase_.srv == nullptr || depth_.srv == nullptr)
    {
        return;
    }

    beginPass(backbuffer, static_cast<float>(width_), static_cast<float>(height_), false, compositeShader_);
    uploadConstants(width_, height_);

    ID3D11ShaderResourceView* resources[kMaxBinds] = {
        sceneColor_.srv,
        ssaoRaw_.srv,
        fog_.srv,
        bloomBase_.srv,
        bloomAccum_.srv,
        lutView_,
        depth_.srv,
        nullptr,
    };
    context_->PSSetShaderResources(0u, 7u, resources);
    drawFullscreen();
    clearResources();
}

void PostProcess::beginPass(
    ID3D11RenderTargetView* rtv,
    float targetWidth,
    float targetHeight,
    bool useDepth,
    ID3D11PixelShader* shader)
{
    context_->OMSetRenderTargets(1u, &rtv, useDepth ? depth_.dsv : nullptr);

    D3D11_VIEWPORT viewport = {};
    defaultViewport(viewport);
    viewport.Width = targetWidth;
    viewport.Height = targetHeight;
    context_->RSSetViewports(1u, &viewport);

    context_->IASetInputLayout(nullptr);
    context_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    context_->VSSetShader(fullscreenVertex_, nullptr, 0u);
    context_->PSSetShader(shader, nullptr, 0u);
    bindSamplers();
}

void PostProcess::drawFullscreen()
{
    context_->Draw(kFullscreenVertexCount, 0u);
}

void PostProcess::bindSamplers()
{
    context_->PSSetSamplers(0u, 1u, &pointSampler_);
    context_->PSSetSamplers(1u, 1u, &linearSampler_);
}

void PostProcess::clearResources()
{
    ID3D11ShaderResourceView* nullResources[kMaxBinds] = {};
    context_->PSSetShaderResources(0u, kMaxBinds, nullResources);
}

void PostProcess::uploadConstants(std::uint32_t texelWidth, std::uint32_t texelHeight)
{
    if (constants_ == nullptr)
    {
        return;
    }
    PostConstants constants = {};
    constants.viewProjection = postViewProj_;
    constants.inverseViewProjection = postInvViewProj_;
    constants.cameraPosition = {
        postCameraPos_.x,
        postCameraPos_.y,
        postCameraPos_.z,
        1.0f,
    };
    constants.viewport = {
        static_cast<float>(texelWidth),
        static_cast<float>(texelHeight),
        1.0f / static_cast<float>(texelWidth),
        1.0f / static_cast<float>(texelHeight),
    };
    constants.targetSize = {
        static_cast<float>(texelWidth),
        static_cast<float>(texelHeight),
        1.0f / static_cast<float>(texelWidth),
        1.0f / static_cast<float>(texelHeight),
    };
    constants.debugView = {
        static_cast<float>(postDebugMode_),
        0.0f,
        0.0f,
        0.0f,
    };
    constants.cameraNearFar = {postNear_, postFar_, 0.0f, 0.0f};
    constants.sun = {postSunDir_.x, postSunDir_.y, postSunDir_.z, postSunIntensity_};
    constants.sunColor = {postSunColor_.x, postSunColor_.y, postSunColor_.z, 0.0f};
    constants.fogParams = {0.0002f, 0.02f, 0.0f, 0.0f};
    constants.ssaoParams = {1.5f, 0.8f, static_cast<float>(kSsaoSamples), 0.0f};
    constants.bloomParams = {1.0f, 0.35f, 0.0f, 0.0f};
    constants.compositeParams = {0.8f, 0.8f, 0.5f, postExposure_};

    context_->UpdateSubresource(constants_, 0u, nullptr, &constants, 0u, 0u);
    context_->VSSetConstantBuffers(0u, 1u, &constants_);
    context_->PSSetConstantBuffers(0u, 1u, &constants_);
}

void PostProcess::updateGradedLut()
{
    if (lutTexture_ == nullptr || lutView_ == nullptr || lutBuffer_.empty())
    {
        return;
    }
    GradingParams params = {};
    params.exposure = postExposure_;
    if (lutReady_ && params.exposure == lutParams_.exposure &&
        params.contrast == lutParams_.contrast && params.saturation == lutParams_.saturation)
    {
        return;
    }

    generateGradingLut(lutBuffer_.data(), lutSize_, params);
    context_->UpdateSubresource(
        lutTexture_,
        0u,
        nullptr,
        lutBuffer_.data(),
        lutSize_ * 4u * sizeof(float),
        lutSize_ * lutSize_ * 4u * sizeof(float));
    lutParams_ = params;
    lutReady_ = true;
}

void PostProcess::ensureTargets(std::uint32_t width, std::uint32_t height)
{
    if (width_ == width && height_ == height && createdDevice_ == d3d_ &&
        sceneColor_.texture != nullptr)
    {
        return;
    }
    releaseTargets();
    createTargets(width, height);
    width_ = width;
    height_ = height;
    createdDevice_ = d3d_;
}

void PostProcess::releaseTargets()
{
    releaseTarget(sceneColor_);
    releaseTarget(depth_);
    releaseTarget(ssaoRaw_);
    releaseTarget(ssaoBlur_);
    releaseTarget(fog_);
    releaseTarget(bloomBase_);
    releaseTarget(bloomMip1_);
    releaseTarget(bloomMip2_);
    releaseTarget(bloomAccum_);
    releaseTarget(bloomTemp_);

    if (noiseView_)
    {
        noiseView_->Release();
        noiseView_ = nullptr;
    }
    if (noiseTexture_)
    {
        noiseTexture_->Release();
        noiseTexture_ = nullptr;
    }
    releaseLut();
    if (constants_)
    {
        constants_->Release();
        constants_ = nullptr;
    }
    if (linearSampler_)
    {
        linearSampler_->Release();
        linearSampler_ = nullptr;
    }
    if (pointSampler_)
    {
        pointSampler_->Release();
        pointSampler_ = nullptr;
    }
    releaseShaders();
}

void PostProcess::releaseTarget(Target& target)
{
    if (target.srv)
    {
        target.srv->Release();
        target.srv = nullptr;
    }
    if (target.dsv)
    {
        target.dsv->Release();
        target.dsv = nullptr;
    }
    if (target.rtv)
    {
        target.rtv->Release();
        target.rtv = nullptr;
    }
    if (target.texture)
    {
        target.texture->Release();
        target.texture = nullptr;
    }
}

void PostProcess::createTargets(std::uint32_t width, std::uint32_t height)
{
    D3D11_SAMPLER_DESC pointDesc = {};
    pointDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
    pointDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    pointDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    pointDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    d3d_->CreateSamplerState(&pointDesc, &pointSampler_);

    D3D11_SAMPLER_DESC linearDesc = {};
    linearDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    linearDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    linearDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    linearDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    d3d_->CreateSamplerState(&linearDesc, &linearSampler_);

    createTarget(width, height, DXGI_FORMAT_R16G16B16A16_FLOAT, sceneColor_);
    createDepthTarget(width, height);
    createTarget(halve(width), halve(height), DXGI_FORMAT_R8_UNORM, ssaoRaw_);
    createTarget(halve(width), halve(height), DXGI_FORMAT_R8_UNORM, ssaoBlur_);
    createTarget(halve(width), halve(height), DXGI_FORMAT_R16G16B16A16_FLOAT, fog_);
    createTarget(quarter(width), quarter(height), DXGI_FORMAT_R16G16B16A16_FLOAT, bloomBase_);
    createTarget(eighth(width), eighth(height), DXGI_FORMAT_R16G16B16A16_FLOAT, bloomMip1_);
    createTarget(sixteenth(width), sixteenth(height), DXGI_FORMAT_R16G16B16A16_FLOAT, bloomMip2_);
    createTarget(eighth(width), eighth(height), DXGI_FORMAT_R16G16B16A16_FLOAT, bloomAccum_);
    createTarget(sixteenth(width), sixteenth(height), DXGI_FORMAT_R16G16B16A16_FLOAT, bloomTemp_);

    createNoiseTexture();
    createLut(33u);

    D3D11_BUFFER_DESC constantDesc = {};
    constantDesc.ByteWidth = static_cast<UINT>(sizeof(PostConstants));
    constantDesc.Usage = D3D11_USAGE_DEFAULT;
    constantDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    d3d_->CreateBuffer(&constantDesc, nullptr, &constants_);

    compileShaders();
}

void PostProcess::createTarget(
    std::uint32_t width,
    std::uint32_t height,
    DXGI_FORMAT format,
    Target& out)
{
    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width = width;
    desc.Height = height;
    desc.MipLevels = 1u;
    desc.ArraySize = 1u;
    desc.Format = format;
    desc.SampleDesc.Count = 1u;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

    d3d_->CreateTexture2D(&desc, nullptr, &out.texture);
    d3d_->CreateRenderTargetView(out.texture, nullptr, &out.rtv);
    d3d_->CreateShaderResourceView(out.texture, nullptr, &out.srv);
}

void PostProcess::createDepthTarget(std::uint32_t width, std::uint32_t height)
{
    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width = width;
    desc.Height = height;
    desc.MipLevels = 1u;
    desc.ArraySize = 1u;
    desc.Format = DXGI_FORMAT_R32_TYPELESS;
    desc.SampleDesc.Count = 1u;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;

    d3d_->CreateTexture2D(&desc, nullptr, &depth_.texture);

    D3D11_DEPTH_STENCIL_VIEW_DESC depthViewDesc = {};
    depthViewDesc.Format = DXGI_FORMAT_D32_FLOAT;
    depthViewDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
    d3d_->CreateDepthStencilView(depth_.texture, &depthViewDesc, &depth_.dsv);

    D3D11_SHADER_RESOURCE_VIEW_DESC resourceDesc = {};
    resourceDesc.Format = DXGI_FORMAT_R32_FLOAT;
    resourceDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    resourceDesc.Texture2D.MipLevels = 1u;
    d3d_->CreateShaderResourceView(depth_.texture, &resourceDesc, &depth_.srv);
}

void PostProcess::createNoiseTexture()
{
    constexpr std::uint32_t size = 4u;
    std::uint8_t pixels[size * size * 4u];
    std::uint32_t state = 0x9E3779B9u;
    for (std::uint8_t& pixel : pixels)
    {
        state = 1664525u * state + 1013904223u;
        pixel = static_cast<std::uint8_t>((state >> 25u) & 0x7Fu);
    }

    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width = size;
    desc.Height = size;
    desc.MipLevels = 1u;
    desc.ArraySize = 1u;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1u;
    desc.Usage = D3D11_USAGE_IMMUTABLE;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA initialData = {};
    initialData.pSysMem = pixels;
    initialData.SysMemPitch = size * 4u;

    d3d_->CreateTexture2D(&desc, &initialData, &noiseTexture_);
    d3d_->CreateShaderResourceView(noiseTexture_, nullptr, &noiseView_);
}

void PostProcess::releaseLut()
{
    if (lutView_)
    {
        lutView_->Release();
        lutView_ = nullptr;
    }
    if (lutTexture_)
    {
        lutTexture_->Release();
        lutTexture_ = nullptr;
    }
    lutBuffer_.clear();
    lutBuffer_.shrink_to_fit();
    lutSize_ = 0u;
    lutReady_ = false;
}

void PostProcess::createLut(std::uint32_t size)
{
    D3D11_TEXTURE3D_DESC desc = {};
    desc.Width = size;
    desc.Height = size;
    desc.Depth = size;
    desc.MipLevels = 1u;
    desc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    d3d_->CreateTexture3D(&desc, nullptr, &lutTexture_);
    if (lutTexture_ == nullptr)
    {
        return;
    }

    D3D11_SHADER_RESOURCE_VIEW_DESC resourceDesc = {};
    resourceDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
    resourceDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE3D;
    resourceDesc.Texture3D.MipLevels = 1u;
    d3d_->CreateShaderResourceView(lutTexture_, &resourceDesc, &lutView_);

    lutSize_ = size;
    lutBuffer_.resize(static_cast<std::size_t>(size) * size * size * 4u);
    lutReady_ = false;
}

void PostProcess::compileShaders()
{
    std::string error;
    ID3DBlob* bytecode = nullptr;

    createVertexShader(d3d_, shaders::kPostVertex, fullscreenVertex_, bytecode, error);
    if (bytecode)
    {
        bytecode->Release();
        bytecode = nullptr;
    }
    createPixelShader(d3d_, shaders::postProcessPixelShader(shaders::kPostPassthrough), passthroughShader_, error);
    createPixelShader(d3d_, shaders::postProcessPixelShader(shaders::kSsaoBody), ssaoShader_, error);
    createPixelShader(d3d_, shaders::postProcessPixelShader(shaders::kSsaoBlurHBody), ssaoBlurHShader_, error);
    createPixelShader(d3d_, shaders::postProcessPixelShader(shaders::kSsaoBlurVBody), ssaoBlurVShader_, error);
    createPixelShader(d3d_, shaders::postProcessPixelShader(shaders::kFogBody), fogShader_, error);
    createPixelShader(d3d_, shaders::postProcessPixelShader(shaders::kBloomExtractBody), bloomExtractShader_, error);
    createPixelShader(d3d_, shaders::postProcessPixelShader(shaders::kBloomDownsampleBody), bloomDownsampleShader_, error);
    createPixelShader(d3d_, shaders::postProcessPixelShader(shaders::kBloomBlurHBody), bloomBlurHShader_, error);
    createPixelShader(d3d_, shaders::postProcessPixelShader(shaders::kBloomBlurVBody), bloomBlurVShader_, error);
    createPixelShader(d3d_, shaders::postProcessPixelShader(shaders::kBloomUpsampleBody), bloomUpsampleShader_, error);
    createPixelShader(d3d_, shaders::postProcessPixelShader(shaders::kCompositeBody), compositeShader_, error);
}

void PostProcess::releaseShaders()
{
    if (compositeShader_)
    {
        compositeShader_->Release();
        compositeShader_ = nullptr;
    }
    if (bloomUpsampleShader_)
    {
        bloomUpsampleShader_->Release();
        bloomUpsampleShader_ = nullptr;
    }
    if (bloomBlurVShader_)
    {
        bloomBlurVShader_->Release();
        bloomBlurVShader_ = nullptr;
    }
    if (bloomBlurHShader_)
    {
        bloomBlurHShader_->Release();
        bloomBlurHShader_ = nullptr;
    }
    if (bloomDownsampleShader_)
    {
        bloomDownsampleShader_->Release();
        bloomDownsampleShader_ = nullptr;
    }
    if (bloomExtractShader_)
    {
        bloomExtractShader_->Release();
        bloomExtractShader_ = nullptr;
    }
    if (fogShader_)
    {
        fogShader_->Release();
        fogShader_ = nullptr;
    }
    if (ssaoBlurVShader_)
    {
        ssaoBlurVShader_->Release();
        ssaoBlurVShader_ = nullptr;
    }
    if (ssaoBlurHShader_)
    {
        ssaoBlurHShader_->Release();
        ssaoBlurHShader_ = nullptr;
    }
    if (ssaoShader_)
    {
        ssaoShader_->Release();
        ssaoShader_ = nullptr;
    }
    if (passthroughShader_)
    {
        passthroughShader_->Release();
        passthroughShader_ = nullptr;
    }
    if (fullscreenVertex_)
    {
        fullscreenVertex_->Release();
        fullscreenVertex_ = nullptr;
    }
}

}