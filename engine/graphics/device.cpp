#define NOMINMAX
#include <windows.h>

#include "engine/graphics/device.hpp"

#include <d3d11.h>
#include <dxgi.h>
#include <dxgi1_2.h>

namespace ksge {

namespace {

constexpr D3D_FEATURE_LEVEL kFeatureLevel = D3D_FEATURE_LEVEL_11_0;

IDXGIDevice* queryDxgiDevice(ID3D11Device* device)
{
    IDXGIDevice* dxgiDevice = nullptr;
    device->QueryInterface(IID_PPV_ARGS(&dxgiDevice));
    return dxgiDevice;
}

IDXGIAdapter* queryAdapter(IDXGIDevice* dxgiDevice)
{
    IDXGIAdapter* adapter = nullptr;
    dxgiDevice->GetAdapter(&adapter);
    return adapter;
}

}

GraphicsDevice::GraphicsDevice(void* nativeHandle, std::int32_t width, std::int32_t height)
    : nativeHandle_(nativeHandle)
    , device_(nullptr)
    , context_(nullptr)
    , swapChain_(nullptr)
    , renderTargetView_(nullptr)
    , depthStencil_(nullptr)
    , depthStencilView_(nullptr)
    , width_(width)
    , height_(height)
{
    createDevice();
    createSwapchain();
    createRenderTargets();
}

GraphicsDevice::~GraphicsDevice()
{
    releaseRenderTargets();
    if (swapChain_)
    {
        swapChain_->Release();
    }
    if (context_)
    {
        context_->Release();
    }
    if (device_)
    {
        device_->Release();
    }
}

ID3D11Device* GraphicsDevice::device() const
{
    return device_;
}

ID3D11DeviceContext* GraphicsDevice::context() const
{
    return context_;
}

void GraphicsDevice::beginFrame(const float clearColor[4])
{
    context_->OMSetRenderTargets(1, &renderTargetView_, depthStencilView_);
    context_->ClearRenderTargetView(renderTargetView_, clearColor);
    context_->ClearDepthStencilView(depthStencilView_, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0u);

    D3D11_VIEWPORT viewport = {};
    viewport.Width = static_cast<float>(width_);
    viewport.Height = static_cast<float>(height_);
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;
    context_->RSSetViewports(1, &viewport);
}

bool GraphicsDevice::present()
{
    const HRESULT result = swapChain_->Present(0, 0);
    if (result == DXGI_ERROR_DEVICE_REMOVED || result == DXGI_ERROR_DEVICE_RESET)
    {
        recreate();
        return false;
    }
    return true;
}

void GraphicsDevice::resize(std::int32_t width, std::int32_t height)
{
    if (width == width_ && height == height_)
    {
        return;
    }
    releaseRenderTargets();
    swapChain_->ResizeBuffers(
        2, static_cast<UINT>(width), static_cast<UINT>(height), DXGI_FORMAT_R8G8B8A8_UNORM, 0u);
    width_ = width;
    height_ = height;
    createRenderTargets();
}

void GraphicsDevice::createDevice()
{
    UINT flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
    HRESULT result = D3D11CreateDevice(
        nullptr,
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        flags,
        &kFeatureLevel,
        1u,
        D3D11_SDK_VERSION,
        &device_,
        nullptr,
        &context_);
    if (FAILED(result))
    {
        result = D3D11CreateDevice(
            nullptr,
            D3D_DRIVER_TYPE_WARP,
            nullptr,
            flags,
            &kFeatureLevel,
            1u,
            D3D11_SDK_VERSION,
            &device_,
            nullptr,
            &context_);
        if (FAILED(result))
        {
            device_ = nullptr;
            context_ = nullptr;
        }
    }
}

void GraphicsDevice::createSwapchain()
{
    IDXGIDevice* dxgiDevice = queryDxgiDevice(device_);
    if (!dxgiDevice)
    {
        return;
    }
    IDXGIAdapter* adapter = queryAdapter(dxgiDevice);
    if (!adapter)
    {
        dxgiDevice->Release();
        return;
    }

    DXGI_SWAP_CHAIN_DESC1 desc = {};
    desc.Width = static_cast<UINT>(width_);
    desc.Height = static_cast<UINT>(height_);
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1u;
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.BufferCount = 2u;
    desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    desc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;

    DXGI_SWAP_CHAIN_FULLSCREEN_DESC fullscreen = {};
    fullscreen.Windowed = TRUE;

    IDXGIFactory2* factory = nullptr;
    adapter->GetParent(IID_PPV_ARGS(&factory));

    HRESULT result = E_FAIL;
    if (factory)
    {
        IDXGISwapChain1* swapChain1 = nullptr;
        result = factory->CreateSwapChainForHwnd(
            device_,
            static_cast<HWND>(nativeHandle_),
            &desc,
            &fullscreen,
            nullptr,
            &swapChain1);
        if (SUCCEEDED(result))
        {
            swapChain_ = swapChain1;
        }
        factory->Release();
    }

    adapter->Release();
    dxgiDevice->Release();

    if (FAILED(result))
    {
        createLegacySwapchain();
    }
}

void GraphicsDevice::createLegacySwapchain()
{
    if (swapChain_)
    {
        swapChain_->Release();
        swapChain_ = nullptr;
    }

    IDXGIDevice* dxgiDevice = queryDxgiDevice(device_);
    if (!dxgiDevice)
    {
        return;
    }
    IDXGIAdapter* adapter = queryAdapter(dxgiDevice);
    if (!adapter)
    {
        dxgiDevice->Release();
        return;
    }

    DXGI_SWAP_CHAIN_DESC desc = {};
    desc.BufferDesc.Width = static_cast<UINT>(width_);
    desc.BufferDesc.Height = static_cast<UINT>(height_);
    desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.BufferDesc.RefreshRate.Numerator = 0u;
    desc.BufferDesc.RefreshRate.Denominator = 1u;
    desc.SampleDesc.Count = 1u;
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.BufferCount = 2u;
    desc.OutputWindow = static_cast<HWND>(nativeHandle_);
    desc.Windowed = TRUE;
    desc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    IDXGIFactory1* factory = nullptr;
    adapter->GetParent(IID_PPV_ARGS(&factory));
    if (factory)
    {
        factory->CreateSwapChain(device_, &desc, &swapChain_);
        factory->Release();
    }

    adapter->Release();
    dxgiDevice->Release();
}

void GraphicsDevice::createRenderTargets()
{
    ID3D11Texture2D* backBuffer = nullptr;
    swapChain_->GetBuffer(0, IID_PPV_ARGS(&backBuffer));
    device_->CreateRenderTargetView(backBuffer, nullptr, &renderTargetView_);
    backBuffer->Release();

    D3D11_TEXTURE2D_DESC depthDesc = {};
    depthDesc.Width = static_cast<UINT>(width_);
    depthDesc.Height = static_cast<UINT>(height_);
    depthDesc.MipLevels = 1u;
    depthDesc.ArraySize = 1u;
    depthDesc.Format = DXGI_FORMAT_D32_FLOAT;
    depthDesc.SampleDesc.Count = 1u;
    depthDesc.Usage = D3D11_USAGE_DEFAULT;
    depthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;

    device_->CreateTexture2D(&depthDesc, nullptr, &depthStencil_);
    device_->CreateDepthStencilView(depthStencil_, nullptr, &depthStencilView_);
}

void GraphicsDevice::releaseRenderTargets()
{
    if (depthStencilView_)
    {
        depthStencilView_->Release();
        depthStencilView_ = nullptr;
    }
    if (depthStencil_)
    {
        depthStencil_->Release();
        depthStencil_ = nullptr;
    }
    if (renderTargetView_)
    {
        renderTargetView_->Release();
        renderTargetView_ = nullptr;
    }
}

void GraphicsDevice::recreate()
{
    releaseRenderTargets();
    if (swapChain_)
    {
        swapChain_->Release();
        swapChain_ = nullptr;
    }
    if (context_)
    {
        context_->Release();
        context_ = nullptr;
    }
    if (device_)
    {
        device_->Release();
        device_ = nullptr;
    }
    createDevice();
    createSwapchain();
    createRenderTargets();
}

}