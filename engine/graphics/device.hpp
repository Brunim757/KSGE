#pragma once

#include <cstdint>

struct ID3D11Device;
struct ID3D11DeviceContext;
struct IDXGISwapChain;
struct ID3D11RenderTargetView;
struct ID3D11Texture2D;
struct ID3D11DepthStencilView;

namespace ksge {

class GraphicsDevice
{
public:
    GraphicsDevice(void* nativeHandle, std::int32_t width, std::int32_t height);
    ~GraphicsDevice();

    GraphicsDevice(const GraphicsDevice&) = delete;
    GraphicsDevice& operator=(const GraphicsDevice&) = delete;

    void beginFrame(const float clearColor[4]);
    bool present();
    void resize(std::int32_t width, std::int32_t height);

    ID3D11Device* device() const;
    ID3D11DeviceContext* context() const;
    ID3D11RenderTargetView* renderTarget() const;
    std::int32_t width() const;
    std::int32_t height() const;

private:
    void createDevice();
    void createSwapchain();
    void createLegacySwapchain();
    void createRenderTargets();
    void releaseRenderTargets();
    void recreate();

    void* nativeHandle_;
    ID3D11Device* device_;
    ID3D11DeviceContext* context_;
    IDXGISwapChain* swapChain_;
    ID3D11RenderTargetView* renderTargetView_;
    ID3D11Texture2D* depthStencil_;
    ID3D11DepthStencilView* depthStencilView_;
    std::int32_t width_;
    std::int32_t height_;
};

}