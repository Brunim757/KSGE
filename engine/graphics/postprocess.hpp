#pragma once

#include <d3d11.h>

#include <cstdint>
#include <vector>

#include <DirectXMath.h>

namespace ksge {

struct GradingParams
{
    float exposure = 1.0f;
    float contrast = 1.0f;
    float saturation = 1.0f;
};

struct PostFrameInfo
{
    std::uint32_t width = 0u;
    std::uint32_t height = 0u;
    DirectX::XMFLOAT4X4 viewProjection;
    DirectX::XMFLOAT4X4 inverseViewProjection;
    DirectX::XMFLOAT3 cameraPosition{0.0f, 0.0f, 0.0f};
    float nearPlane = 0.1f;
    float farPlane = 5000.0f;
    DirectX::XMFLOAT3 sunDirection{0.5f, 0.8f, 0.6f};
    float sunIntensity = 3.0f;
    DirectX::XMFLOAT3 sunColor{1.0f, 0.95f, 0.85f};
    float exposure = 1.0f;
    std::uint32_t debugMode = 0u;
};

void generateGradingLut(float* rgbaOut, std::uint32_t size, const GradingParams& params);

class PostProcess
{
public:
    PostProcess() = default;
    ~PostProcess();

    PostProcess(const PostProcess&) = delete;
    PostProcess& operator=(const PostProcess&) = delete;

    void attach(ID3D11Device* device, ID3D11DeviceContext* context);

    void beginScene(std::uint32_t width, std::uint32_t height);
    void endScene();
    void run(const PostFrameInfo& info, ID3D11RenderTargetView* backbuffer);

private:
    struct Target
    {
        ID3D11Texture2D* texture = nullptr;
        ID3D11RenderTargetView* rtv = nullptr;
        ID3D11DepthStencilView* dsv = nullptr;
        ID3D11ShaderResourceView* srv = nullptr;
    };

    void ensureTargets(std::uint32_t width, std::uint32_t height);
    void releaseTargets();
    void createTargets(std::uint32_t width, std::uint32_t height);
    void releaseTarget(Target& target);
    void createTarget(std::uint32_t width, std::uint32_t height, DXGI_FORMAT format, Target& out);
    void createDepthTarget(std::uint32_t width, std::uint32_t height);
    void createNoiseTexture();
    void releaseLut();
    void createLut(std::uint32_t size);
    void compileShaders();
    void releaseShaders();

    void beginPass(
        ID3D11RenderTargetView* rtv,
        float targetWidth,
        float targetHeight,
        bool useDepth,
        ID3D11PixelShader* shader);
    void drawFullscreen();
    void bindSamplers();
    void clearResources();
    void uploadConstants(std::uint32_t texelWidth, std::uint32_t texelHeight);
    void updateGradedLut();

    void applySsao();
    void applySsaoBlur();
    void applyFog();
    void applyBloom();
    void applyComposite(ID3D11RenderTargetView* backbuffer);

    ID3D11Device* d3d_ = nullptr;
    ID3D11DeviceContext* context_ = nullptr;
    ID3D11Device* createdDevice_ = nullptr;
    std::uint32_t width_ = 0u;
    std::uint32_t height_ = 0u;

    Target sceneColor_;
    Target depth_;
    Target ssaoRaw_;
    Target ssaoBlur_;
    Target fog_;
    Target bloomBase_;
    Target bloomMip1_;
    Target bloomMip2_;
    Target bloomAccum_;
    Target bloomTemp_;

    ID3D11VertexShader* fullscreenVertex_ = nullptr;
    ID3D11PixelShader* passthroughShader_ = nullptr;
    ID3D11PixelShader* ssaoShader_ = nullptr;
    ID3D11PixelShader* ssaoBlurHShader_ = nullptr;
    ID3D11PixelShader* ssaoBlurVShader_ = nullptr;
    ID3D11PixelShader* fogShader_ = nullptr;
    ID3D11PixelShader* bloomExtractShader_ = nullptr;
    ID3D11PixelShader* bloomDownsampleShader_ = nullptr;
    ID3D11PixelShader* bloomBlurHShader_ = nullptr;
    ID3D11PixelShader* bloomBlurVShader_ = nullptr;
    ID3D11PixelShader* bloomUpsampleShader_ = nullptr;
    ID3D11PixelShader* compositeShader_ = nullptr;

    ID3D11SamplerState* pointSampler_ = nullptr;
    ID3D11SamplerState* linearSampler_ = nullptr;
    ID3D11Buffer* constants_ = nullptr;

    ID3D11Texture2D* noiseTexture_ = nullptr;
    ID3D11ShaderResourceView* noiseView_ = nullptr;

    ID3D11Texture3D* lutTexture_ = nullptr;
    ID3D11ShaderResourceView* lutView_ = nullptr;
    std::vector<float> lutBuffer_;
    GradingParams lutParams_;
    std::uint32_t lutSize_ = 0u;
    bool lutReady_ = false;

    DirectX::XMFLOAT4X4 postViewProj_;
    DirectX::XMFLOAT4X4 postInvViewProj_;
    DirectX::XMFLOAT3 postCameraPos_{0.0f, 0.0f, 0.0f};
    float postNear_ = 0.1f;
    float postFar_ = 5000.0f;
    DirectX::XMFLOAT3 postSunDir_{0.5f, 0.8f, 0.6f};
    float postSunIntensity_ = 3.0f;
    DirectX::XMFLOAT3 postSunColor_{1.0f, 0.95f, 0.85f};
    float postExposure_ = 1.0f;
    std::uint32_t postDebugMode_ = 0u;
};

}