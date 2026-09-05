#include "engine/graphics/postprocess.hpp"

#include "engine/graphics/shader_compiler.hpp"
#include "engine/shaders/shaders_storage.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace ksge {

namespace {

void writeDiagnostic(const char* what, const char* detail)
{
    std::fprintf(stderr, "KSGE postprocess: %s %s\n", what, detail);
    FILE* file = nullptr;
    if (fopen_s(&file, "gpu.log", "a") == 0 && file != nullptr)
    {
        std::fprintf(file, "postprocess: %s %s\n", what, detail);
        std::fclose(file);
    }
}

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
    DirectX::XMFLOAT4 skyTop;
    DirectX::XMFLOAT4 skyHorizon;
    DirectX::XMFLOAT4 fogParams;
    DirectX::XMFLOAT4 ssaoParams;
    DirectX::XMFLOAT4 bloomParams;
    DirectX::XMFLOAT4 shadowSplits;
    DirectX::XMFLOAT4 shadowParams;
    DirectX::XMFLOAT4 compositeParams;
    DirectX::XMFLOAT4X4 shadowViewProjection[kShadowCascades];
    DirectX::XMFLOAT4X4 previousViewProjection;
};

constexpr UINT kFullscreenVertexCount = 3u;
constexpr std::uint32_t kSsaoSamples = 16u;
constexpr UINT kMaxBinds = 12u;
constexpr std::uint32_t kMinTargetSize = 1u;
constexpr std::uint32_t kShadowMapSize = 1024u;

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
    if (gbufferA_.rtv == nullptr || depth_.dsv == nullptr)
    {
        return;
    }

    ID3D11RenderTargetView* renderTargets[3] = {gbufferA_.rtv, gbufferB_.rtv, gbufferC_.rtv};
    context_->OMSetRenderTargets(3u, renderTargets, depth_.dsv);
    const float clearColor[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    for (std::uint32_t i = 0u; i < 3u; ++i)
    {
        context_->ClearRenderTargetView(renderTargets[i], clearColor);
    }
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
    ID3D11RenderTargetView* nullRenderTargets[3] = {};
    context_->OMSetRenderTargets(3u, nullRenderTargets, nullptr);
    clearResources();
}

void PostProcess::beginShadowMap(std::uint32_t cascade)
{
    if (context_ == nullptr || cascade >= kShadowCascades || shadowMaps_[cascade].dsv == nullptr)
    {
        return;
    }
    context_->OMSetRenderTargets(0u, nullptr, shadowMaps_[cascade].dsv);
    D3D11_VIEWPORT viewport = {};
    defaultViewport(viewport);
    viewport.Width = static_cast<float>(kShadowMapSize);
    viewport.Height = static_cast<float>(kShadowMapSize);
    context_->RSSetViewports(1u, &viewport);
}

void PostProcess::endShadowMap()
{
    if (context_ == nullptr)
    {
        return;
    }
    context_->OMSetRenderTargets(0u, nullptr, nullptr);
}

void PostProcess::run(const PostFrameInfo& info, ID3D11RenderTargetView* backbuffer)
{
    if (d3d_ == nullptr || context_ == nullptr)
    {
        return;
    }
    ensureTargets(info.width, info.height);
    if (compositeShader_ == nullptr || copyShader_ == nullptr)
    {
        drawSceneFallback(backbuffer);
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
    postSkyTop_ = info.skyTop;
    postSkyHorizon_ = info.skyHorizon;
    for (std::uint32_t cascade = 0u; cascade < kShadowCascades; ++cascade)
    {
        postShadowViewProj_[cascade] = info.shadowViewProjection[cascade];
    }
    postCascadeSplits_[0] = info.cascadeSplits[0];
    postCascadeSplits_[1] = info.cascadeSplits[1];
    postCascadeSplits_[2] = info.cascadeSplits[2];
    postShadowMapSize_ = info.shadowMapSize;
    postShadowBlendWidth_ = info.shadowBlendWidth;
    postShadowDepthBias_ = info.shadowDepthBias;
    postExposure_ = info.exposure;
    postDebugMode_ = info.debugMode;
    postPrevViewProj_ = info.previousViewProjection;

    if (postDebugMode_ != lastDebugMode_)
    {
        taaValid_ = false;
        lastDebugMode_ = postDebugMode_;
    }

    if (postDebugMode_ == 0u)
    {
        if (sceneBlendShader_ == nullptr || taaShader_ == nullptr || finalShader_ == nullptr)
        {
            clearTarget(taaHistory_.rtv, 0.0f, 0.0f, 0.0f, 1.0f);
            clearTarget(taaResult_.rtv, 0.0f, 0.0f, 0.0f, 1.0f);
        }
    }

    if (ssaoShader_ == nullptr)
    {
        clearTarget(ssaoRaw_.rtv, 1.0f, 1.0f, 1.0f, 1.0f);
        clearTarget(ssaoBlur_.rtv, 1.0f, 1.0f, 1.0f, 1.0f);
    }
    if (fogShader_ == nullptr)
    {
        clearTarget(fog_.rtv, 0.0f, 0.0f, 0.0f, 1.0f);
    }
    if (bloomExtractShader_ == nullptr)
    {
        clearTarget(bloomBase_.rtv, 0.0f, 0.0f, 0.0f, 1.0f);
        clearTarget(bloomMip1_.rtv, 0.0f, 0.0f, 0.0f, 1.0f);
        clearTarget(bloomMip2_.rtv, 0.0f, 0.0f, 0.0f, 1.0f);
        clearTarget(bloomAccum_.rtv, 0.0f, 0.0f, 0.0f, 1.0f);
        clearTarget(bloomTemp_.rtv, 0.0f, 0.0f, 0.0f, 1.0f);
    }
    if (deferredLightShader_ == nullptr || skyPostShader_ == nullptr)
    {
        clearTarget(sceneColor_.rtv, 0.0f, 0.0f, 0.0f, 1.0f);
    }
    if (ssrShader_ == nullptr)
    {
        clearTarget(ssr_.rtv, 0.0f, 0.0f, 0.0f, 1.0f);
    }
    if (ssgiShader_ == nullptr)
    {
        clearTarget(ssgi_.rtv, 0.0f, 0.0f, 0.0f, 1.0f);
    }

    updateGradedLut();
    applySsao();
    applySsaoBlur();
    applyDeferredLight();
    applySsr();
    applySsgi();
    applySky();
    applyFog();
    applyBloom();
    if (postDebugMode_ == 0u)
    {
        applySceneBlend();
        applyTaa();
        applyFinal(backbuffer);
    }
    else
    {
        applyDebugComposite(backbuffer);
    }
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

void PostProcess::applyDeferredLight()
{
    if (deferredLightShader_ == nullptr || sceneColor_.rtv == nullptr ||
        gbufferA_.srv == nullptr || gbufferB_.srv == nullptr || gbufferC_.srv == nullptr ||
        depth_.srv == nullptr || ssaoRaw_.srv == nullptr)
    {
        return;
    }
    beginPass(sceneColor_.rtv, static_cast<float>(width_), static_cast<float>(height_), false, deferredLightShader_);
    uploadConstants(width_, height_);

    ID3D11ShaderResourceView* resources[8] = {
        gbufferA_.srv,
        gbufferB_.srv,
        gbufferC_.srv,
        depth_.srv,
        ssaoRaw_.srv,
        shadowMaps_[0].srv,
        shadowMaps_[1].srv,
        shadowMaps_[2].srv,
    };
    context_->PSSetShaderResources(0u, 8u, resources);
    drawFullscreen();
    clearResources();
}

void PostProcess::applySsr()
{
    if (ssrShader_ == nullptr || ssr_.rtv == nullptr ||
        sceneColor_.srv == nullptr || gbufferA_.srv == nullptr ||
        gbufferB_.srv == nullptr || depth_.srv == nullptr)
    {
        return;
    }
    const std::uint32_t halfWidth = halve(width_);
    const std::uint32_t halfHeight = halve(height_);

    beginPass(ssr_.rtv, static_cast<float>(halfWidth), static_cast<float>(halfHeight), false, ssrShader_);
    uploadConstants(halfWidth, halfHeight);
    ID3D11ShaderResourceView* resources[4] = {sceneColor_.srv, gbufferA_.srv, gbufferB_.srv, depth_.srv};
    context_->PSSetShaderResources(0u, 4u, resources);
    drawFullscreen();
    clearResources();
}

void PostProcess::applySsgi()
{
    if (ssgiShader_ == nullptr || ssgi_.rtv == nullptr ||
        sceneColor_.srv == nullptr || gbufferA_.srv == nullptr ||
        gbufferB_.srv == nullptr || depth_.srv == nullptr)
    {
        return;
    }
    const std::uint32_t halfWidth = halve(width_);
    const std::uint32_t halfHeight = halve(height_);

    beginPass(ssgi_.rtv, static_cast<float>(halfWidth), static_cast<float>(halfHeight), false, ssgiShader_);
    uploadConstants(halfWidth, halfHeight);
    ID3D11ShaderResourceView* resources[4] = {sceneColor_.srv, gbufferA_.srv, gbufferB_.srv, depth_.srv};
    context_->PSSetShaderResources(0u, 4u, resources);
    drawFullscreen();
    clearResources();
}

void PostProcess::applySky()
{
    if (skyPostShader_ == nullptr || sceneColor_.rtv == nullptr || depth_.srv == nullptr)
    {
        return;
    }
    beginPass(sceneColor_.rtv, static_cast<float>(width_), static_cast<float>(height_), false, skyPostShader_);
    uploadConstants(width_, height_);
    ID3D11ShaderResourceView* resources[1] = {depth_.srv};
    context_->PSSetShaderResources(0u, 1u, resources);
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
    uploadConstants(width_, height_);
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

void PostProcess::applySceneBlend()
{
    if (sceneBlendShader_ == nullptr || blend_.rtv == nullptr ||
        sceneColor_.srv == nullptr || ssr_.srv == nullptr || ssgi_.srv == nullptr ||
        fog_.srv == nullptr || bloomBase_.srv == nullptr)
    {
        return;
    }
    beginPass(blend_.rtv, static_cast<float>(width_), static_cast<float>(height_), false, sceneBlendShader_);
    uploadConstants(width_, height_);
    ID3D11ShaderResourceView* resources[6] = {
        sceneColor_.srv,
        ssr_.srv,
        ssgi_.srv,
        fog_.srv,
        bloomBase_.srv,
        bloomAccum_.srv,
    };
    context_->PSSetShaderResources(0u, 6u, resources);
    drawFullscreen();
    clearResources();
}

void PostProcess::applyTaa()
{
    if (taaShader_ == nullptr || taaResult_.rtv == nullptr ||
        blend_.srv == nullptr || taaHistory_.srv == nullptr || depth_.srv == nullptr)
    {
        return;
    }
    beginPass(taaResult_.rtv, static_cast<float>(width_), static_cast<float>(height_), false, taaShader_);
    uploadConstants(width_, height_);
    ID3D11ShaderResourceView* resources[3] = {blend_.srv, taaHistory_.srv, depth_.srv};
    context_->PSSetShaderResources(0u, 3u, resources);
    drawFullscreen();
    clearResources();

    ID3D11RenderTargetView* historyTarget = taaHistory_.rtv;
    context_->OMSetRenderTargets(1u, &historyTarget, nullptr);
    D3D11_VIEWPORT viewport = {};
    defaultViewport(viewport);
    viewport.Width = static_cast<float>(width_);
    viewport.Height = static_cast<float>(height_);
    context_->RSSetViewports(1u, &viewport);
    context_->IASetInputLayout(nullptr);
    context_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    context_->VSSetShader(fullscreenVertex_, nullptr, 0u);
    context_->PSSetShader(copyShader_, nullptr, 0u);
    bindSamplers();
    ID3D11ShaderResourceView* copyResources[1] = {taaResult_.srv};
    context_->PSSetShaderResources(0u, 1u, copyResources);
    drawFullscreen();
    clearResources();
    taaValid_ = true;
}

void PostProcess::applyFinal(ID3D11RenderTargetView* backbuffer)
{
    if (finalShader_ == nullptr || backbuffer == nullptr || taaResult_.srv == nullptr ||
        lutView_ == nullptr)
    {
        drawSceneFallback(backbuffer);
        return;
    }
    beginPass(backbuffer, static_cast<float>(width_), static_cast<float>(height_), false, finalShader_);
    uploadConstants(width_, height_);
    ID3D11ShaderResourceView* resources[2] = {taaResult_.srv, lutView_};
    context_->PSSetShaderResources(0u, 2u, resources);
    drawFullscreen();
    clearResources();
}

void PostProcess::applyDebugComposite(ID3D11RenderTargetView* backbuffer)
{
    if (backbuffer == nullptr)
    {
        return;
    }
    if (compositeShader_ == nullptr || sceneColor_.srv == nullptr ||
        ssaoRaw_.srv == nullptr || fog_.srv == nullptr || bloomBase_.srv == nullptr ||
        lutView_ == nullptr || depth_.srv == nullptr || copyShader_ == nullptr ||
        ssr_.srv == nullptr || ssgi_.srv == nullptr)
    {
        drawSceneFallback(backbuffer);
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
        ssr_.srv,
        ssgi_.srv,
    };
    context_->PSSetShaderResources(0u, 9u, resources);
    drawFullscreen();
    clearResources();
}

void PostProcess::drawSceneFallback(ID3D11RenderTargetView* backbuffer)
{
    if (backbuffer == nullptr || copyShader_ == nullptr || sceneColor_.srv == nullptr)
    {
        return;
    }
    beginPass(backbuffer, static_cast<float>(width_), static_cast<float>(height_), false, copyShader_);
    ID3D11ShaderResourceView* resources[1] = {sceneColor_.srv};
    context_->PSSetShaderResources(0u, 1u, resources);
    drawFullscreen();
    clearResources();
}

void PostProcess::clearTarget(ID3D11RenderTargetView* rtv, float r, float g, float b, float a)
{
    if (rtv == nullptr)
    {
        return;
    }
    const float color[4] = {r, g, b, a};
    context_->ClearRenderTargetView(rtv, color);
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
    context_->PSSetSamplers(2u, 1u, &shadowSampler_);
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
        static_cast<float>(width_),
        static_cast<float>(height_),
        1.0f / static_cast<float>(width_),
        1.0f / static_cast<float>(height_),
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
        taaValid_ ? 1.0f : 0.0f,
    };
    constants.cameraNearFar = {postNear_, postFar_, 0.0f, 0.0f};
    constants.sun = {postSunDir_.x, postSunDir_.y, postSunDir_.z, postSunIntensity_};
    constants.sunColor = {postSunColor_.x, postSunColor_.y, postSunColor_.z, 0.0f};
    constants.skyTop = {postSkyTop_.x, postSkyTop_.y, postSkyTop_.z, 1.0f};
    constants.skyHorizon = {postSkyHorizon_.x, postSkyHorizon_.y, postSkyHorizon_.z, 1.0f};
    constants.fogParams = {0.002f, 0.05f, 0.0f, 0.0f};
    constants.ssaoParams = {1.2f, 0.8f, static_cast<float>(kSsaoSamples), 0.0f};
    constants.bloomParams = {1.0f, 0.35f, 0.8f, 0.0f};
    constants.shadowSplits = {
        postCascadeSplits_[0],
        postCascadeSplits_[1],
        postCascadeSplits_[2],
        0.0f,
    };
    constants.shadowParams = {
        1.0f / postShadowMapSize_,
        1.0f / postShadowMapSize_,
        postShadowDepthBias_,
        postShadowBlendWidth_,
    };
    constants.compositeParams = {0.5f, 0.9f, 1.0f, postExposure_};
    for (std::uint32_t cascade = 0u; cascade < kShadowCascades; ++cascade)
    {
        constants.shadowViewProjection[cascade] = postShadowViewProj_[cascade];
    }
    constants.previousViewProjection = postPrevViewProj_;

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
    releaseTarget(gbufferA_);
    releaseTarget(gbufferB_);
    releaseTarget(gbufferC_);
    for (std::uint32_t cascade = 0u; cascade < kShadowCascades; ++cascade)
    {
        releaseTarget(shadowMaps_[cascade]);
    }
    releaseTarget(ssaoRaw_);
    releaseTarget(ssaoBlur_);
    releaseTarget(ssr_);
    releaseTarget(ssgi_);
    releaseTarget(fog_);
    releaseTarget(bloomBase_);
    releaseTarget(bloomMip1_);
    releaseTarget(bloomMip2_);
    releaseTarget(bloomAccum_);
    releaseTarget(bloomTemp_);
    releaseTarget(blend_);
    releaseTarget(taaHistory_);
    releaseTarget(taaResult_);

    taaValid_ = false;

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
    if (shadowSampler_)
    {
        shadowSampler_->Release();
        shadowSampler_ = nullptr;
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
    if (FAILED(d3d_->CreateSamplerState(&pointDesc, &pointSampler_)))
    {
        writeDiagnostic("point sampler creation failed", "post sampler");
    }

    D3D11_SAMPLER_DESC linearDesc = {};
    linearDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    linearDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    linearDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    linearDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    if (FAILED(d3d_->CreateSamplerState(&linearDesc, &linearSampler_)))
    {
        writeDiagnostic("linear sampler creation failed", "post sampler");
    }

    D3D11_SAMPLER_DESC shadowDesc = {};
    shadowDesc.Filter = D3D11_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
    shadowDesc.AddressU = D3D11_TEXTURE_ADDRESS_BORDER;
    shadowDesc.AddressV = D3D11_TEXTURE_ADDRESS_BORDER;
    shadowDesc.AddressW = D3D11_TEXTURE_ADDRESS_BORDER;
    shadowDesc.BorderColor[0] = 1.0f;
    shadowDesc.BorderColor[1] = 1.0f;
    shadowDesc.BorderColor[2] = 1.0f;
    shadowDesc.BorderColor[3] = 1.0f;
    shadowDesc.ComparisonFunc = D3D11_COMPARISON_LESS;
    if (FAILED(d3d_->CreateSamplerState(&shadowDesc, &shadowSampler_)))
    {
        writeDiagnostic("shadow sampler creation failed", "post sampler");
    }

    createTarget(width, height, DXGI_FORMAT_R8G8B8A8_UNORM, gbufferA_);
    createTarget(width, height, DXGI_FORMAT_R16G16B16A16_FLOAT, gbufferB_);
    createTarget(width, height, DXGI_FORMAT_R8G8B8A8_UNORM, gbufferC_);
    createDepthTarget(width, height);
    createTarget(width, height, DXGI_FORMAT_R16G16B16A16_FLOAT, sceneColor_);
    createTarget(halve(width), halve(height), DXGI_FORMAT_R8_UNORM, ssaoRaw_);
    createTarget(halve(width), halve(height), DXGI_FORMAT_R8_UNORM, ssaoBlur_);
    createTarget(halve(width), halve(height), DXGI_FORMAT_R16G16B16A16_FLOAT, ssr_);
    createTarget(halve(width), halve(height), DXGI_FORMAT_R16G16B16A16_FLOAT, ssgi_);
    createTarget(halve(width), halve(height), DXGI_FORMAT_R16G16B16A16_FLOAT, fog_);
    createTarget(quarter(width), quarter(height), DXGI_FORMAT_R16G16B16A16_FLOAT, bloomBase_);
    createTarget(eighth(width), eighth(height), DXGI_FORMAT_R16G16B16A16_FLOAT, bloomMip1_);
    createTarget(sixteenth(width), sixteenth(height), DXGI_FORMAT_R16G16B16A16_FLOAT, bloomMip2_);
    createTarget(eighth(width), eighth(height), DXGI_FORMAT_R16G16B16A16_FLOAT, bloomAccum_);
    createTarget(sixteenth(width), sixteenth(height), DXGI_FORMAT_R16G16B16A16_FLOAT, bloomTemp_);
    createTarget(width, height, DXGI_FORMAT_R16G16B16A16_FLOAT, blend_);
    createTarget(width, height, DXGI_FORMAT_R16G16B16A16_FLOAT, taaHistory_);
    createTarget(width, height, DXGI_FORMAT_R16G16B16A16_FLOAT, taaResult_);
    if (taaHistory_.rtv != nullptr)
    {
        const float zeroColor[4] = {0.0f, 0.0f, 0.0f, 1.0f};
        context_->ClearRenderTargetView(taaHistory_.rtv, zeroColor);
    }

    for (std::uint32_t cascade = 0u; cascade < kShadowCascades; ++cascade)
    {
        createShadowTarget(cascade);
    }

    createNoiseTexture();
    createLut(33u);

    D3D11_BUFFER_DESC constantDesc = {};
    constantDesc.ByteWidth = static_cast<UINT>(sizeof(PostConstants));
    constantDesc.Usage = D3D11_USAGE_DEFAULT;
    constantDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    if (FAILED(d3d_->CreateBuffer(&constantDesc, nullptr, &constants_)))
    {
        writeDiagnostic("post constant buffer creation failed", "post cb");
    }

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

    if (FAILED(d3d_->CreateTexture2D(&desc, nullptr, &out.texture)))
    {
        writeDiagnostic("render target texture creation failed", "post target");
        return;
    }
    if (FAILED(d3d_->CreateRenderTargetView(out.texture, nullptr, &out.rtv)))
    {
        writeDiagnostic("render target view creation failed", "post target");
        return;
    }
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

    if (FAILED(d3d_->CreateTexture2D(&desc, nullptr, &depth_.texture)))
    {
        writeDiagnostic("depth texture creation failed", "post depth");
        return;
    }

    D3D11_DEPTH_STENCIL_VIEW_DESC depthViewDesc = {};
    depthViewDesc.Format = DXGI_FORMAT_D32_FLOAT;
    depthViewDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
    if (FAILED(d3d_->CreateDepthStencilView(depth_.texture, &depthViewDesc, &depth_.dsv)))
    {
        writeDiagnostic("depth stencil view creation failed", "post depth");
        return;
    }

    D3D11_SHADER_RESOURCE_VIEW_DESC resourceDesc = {};
    resourceDesc.Format = DXGI_FORMAT_R32_FLOAT;
    resourceDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    resourceDesc.Texture2D.MipLevels = 1u;
    d3d_->CreateShaderResourceView(depth_.texture, &resourceDesc, &depth_.srv);
}

void PostProcess::createShadowTarget(std::uint32_t cascade)
{
    Target& target = shadowMaps_[cascade];

    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width = kShadowMapSize;
    desc.Height = kShadowMapSize;
    desc.MipLevels = 1u;
    desc.ArraySize = 1u;
    desc.Format = DXGI_FORMAT_R32_TYPELESS;
    desc.SampleDesc.Count = 1u;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;

    if (FAILED(d3d_->CreateTexture2D(&desc, nullptr, &target.texture)))
    {
        writeDiagnostic("shadow texture creation failed", "shadow map");
        return;
    }

    D3D11_DEPTH_STENCIL_VIEW_DESC depthViewDesc = {};
    depthViewDesc.Format = DXGI_FORMAT_D32_FLOAT;
    depthViewDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
    if (FAILED(d3d_->CreateDepthStencilView(target.texture, &depthViewDesc, &target.dsv)))
    {
        writeDiagnostic("shadow depth view creation failed", "shadow map");
        return;
    }

    D3D11_SHADER_RESOURCE_VIEW_DESC resourceDesc = {};
    resourceDesc.Format = DXGI_FORMAT_R32_FLOAT;
    resourceDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    resourceDesc.Texture2D.MipLevels = 1u;
    d3d_->CreateShaderResourceView(target.texture, &resourceDesc, &target.srv);
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

    if (FAILED(d3d_->CreateTexture2D(&desc, &initialData, &noiseTexture_)))
    {
        writeDiagnostic("noise texture creation failed", "ssao noise");
        return;
    }
    if (FAILED(d3d_->CreateShaderResourceView(noiseTexture_, nullptr, &noiseView_)))
    {
        writeDiagnostic("noise view creation failed", "ssao noise");
    }
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

    if (FAILED(d3d_->CreateTexture3D(&desc, nullptr, &lutTexture_)))
    {
        writeDiagnostic("lut texture creation failed", "3d lut");
        return;
    }

    D3D11_SHADER_RESOURCE_VIEW_DESC resourceDesc = {};
    resourceDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
    resourceDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE3D;
    resourceDesc.Texture3D.MipLevels = 1u;
    if (FAILED(d3d_->CreateShaderResourceView(lutTexture_, &resourceDesc, &lutView_)))
    {
        writeDiagnostic("lut view creation failed", "3d lut");
        return;
    }

    lutSize_ = size;
    lutBuffer_.resize(static_cast<std::size_t>(size) * size * size * 4u);
    lutReady_ = false;
}

void PostProcess::compileShaders()
{
    std::string error;
    ID3DBlob* bytecode = nullptr;

    if (!createVertexShader(d3d_, shaders::kPostVertex, fullscreenVertex_, bytecode, error))
    {
        writeDiagnostic("fullscreen vertex shader failed", error.c_str());
    }
    if (bytecode)
    {
        bytecode->Release();
        bytecode = nullptr;
    }

    auto compile = [&](const char* name, ID3D11PixelShader*& target, const std::string& source)
    {
        std::string shaderError;
        if (!createPixelShader(d3d_, source, target, shaderError))
        {
            writeDiagnostic(name, shaderError.c_str());
        }
    };

    compile("copy shader", copyShader_, shaders::postProcessPixelShader(shaders::kPostCopyBody));
    compile("deferred light", deferredLightShader_, shaders::postProcessPixelShader(shaders::kDeferredLightBody));
    compile("ssr", ssrShader_, shaders::postProcessPixelShader(shaders::kSsrBody));
    compile("ssgi", ssgiShader_, shaders::postProcessPixelShader(shaders::kSsgiBody));
    compile("sky post", skyPostShader_, shaders::postProcessPixelShader(shaders::kSkyPostBody));
    compile("ssao", ssaoShader_, shaders::postProcessPixelShader(shaders::kSsaoBody));
    compile("ssao blur h", ssaoBlurHShader_, shaders::postProcessPixelShader(shaders::kSsaoBlurHBody));
    compile("ssao blur v", ssaoBlurVShader_, shaders::postProcessPixelShader(shaders::kSsaoBlurVBody));
    compile("fog", fogShader_, shaders::postProcessPixelShader(shaders::kFogBody));
    compile("bloom extract", bloomExtractShader_, shaders::postProcessPixelShader(shaders::kBloomExtractBody));
    compile("bloom downsample", bloomDownsampleShader_, shaders::postProcessPixelShader(shaders::kBloomDownsampleBody));
    compile("bloom blur h", bloomBlurHShader_, shaders::postProcessPixelShader(shaders::kBloomBlurHBody));
    compile("bloom blur v", bloomBlurVShader_, shaders::postProcessPixelShader(shaders::kBloomBlurVBody));
    compile("bloom upsample", bloomUpsampleShader_, shaders::postProcessPixelShader(shaders::kBloomUpsampleBody));
    compile("scene blend", sceneBlendShader_, shaders::postProcessPixelShader(shaders::kSceneBlendBody));
    compile("taa", taaShader_, shaders::postProcessPixelShader(shaders::kTaaBody));
    compile("final", finalShader_, shaders::postProcessPixelShader(shaders::kFinalBody));
    compile("composite", compositeShader_, shaders::postProcessPixelShader(shaders::kCompositeBody));
}

void PostProcess::releaseShaders()
{
    if (finalShader_)
    {
        finalShader_->Release();
        finalShader_ = nullptr;
    }
    if (taaShader_)
    {
        taaShader_->Release();
        taaShader_ = nullptr;
    }
    if (sceneBlendShader_)
    {
        sceneBlendShader_->Release();
        sceneBlendShader_ = nullptr;
    }
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
    if (skyPostShader_)
    {
        skyPostShader_->Release();
        skyPostShader_ = nullptr;
    }
    if (ssgiShader_)
    {
        ssgiShader_->Release();
        ssgiShader_ = nullptr;
    }
    if (ssrShader_)
    {
        ssrShader_->Release();
        ssrShader_ = nullptr;
    }
    if (deferredLightShader_)
    {
        deferredLightShader_->Release();
        deferredLightShader_ = nullptr;
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
    if (copyShader_)
    {
        copyShader_->Release();
        copyShader_ = nullptr;
    }
    if (fullscreenVertex_)
    {
        fullscreenVertex_->Release();
        fullscreenVertex_ = nullptr;
    }
}

}