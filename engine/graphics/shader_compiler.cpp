#include "engine/graphics/shader_compiler.hpp"

#include <d3dcompiler.h>

namespace ksge {

namespace {

constexpr UINT compilerFlags()
{
#ifdef _DEBUG
    return D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
    return D3DCOMPILE_OPTIMIZATION_LEVEL3;
#endif
}

}

bool compileShaderSource(
    std::string_view source,
    std::string_view entry,
    std::string_view target,
    ID3DBlob*& bytecodeOut,
    std::string& errorOut)
{
    ID3DBlob* errorBlob = nullptr;
    const HRESULT result = D3DCompile(
        source.data(),
        source.size(),
        nullptr,
        nullptr,
        nullptr,
        entry.data(),
        target.data(),
        compilerFlags(),
        0u,
        &bytecodeOut,
        &errorBlob);

    if (FAILED(result))
    {
        if (errorBlob)
        {
            const char* message = static_cast<const char*>(errorBlob->GetBufferPointer());
            errorOut = std::string(message, errorBlob->GetBufferSize());
            errorBlob->Release();
        }
        else
        {
            errorOut = "shader compile failed without message";
        }
        return false;
    }

    if (errorBlob)
    {
        errorBlob->Release();
    }
    return true;
}

bool createVertexShader(
    ID3D11Device* device,
    std::string_view source,
    ID3D11VertexShader*& shaderOut,
    ID3DBlob*& bytecodeOut,
    std::string& errorOut)
{
    if (!compileShaderSource(source, "main", "vs_5_0", bytecodeOut, errorOut))
    {
        return false;
    }
    const HRESULT result = device->CreateVertexShader(
        bytecodeOut->GetBufferPointer(), bytecodeOut->GetBufferSize(), nullptr, &shaderOut);
    if (FAILED(result))
    {
        errorOut = "vertex shader object creation failed";
        return false;
    }
    return true;
}

bool createPixelShader(
    ID3D11Device* device,
    std::string_view source,
    ID3D11PixelShader*& shaderOut,
    std::string& errorOut)
{
    ID3DBlob* bytecode = nullptr;
    if (!compileShaderSource(source, "main", "ps_5_0", bytecode, errorOut))
    {
        return false;
    }
    const HRESULT result = device->CreatePixelShader(
        bytecode->GetBufferPointer(), bytecode->GetBufferSize(), nullptr, &shaderOut);
    bytecode->Release();
    if (FAILED(result))
    {
        errorOut = "pixel shader object creation failed";
        return false;
    }
    return true;
}

}