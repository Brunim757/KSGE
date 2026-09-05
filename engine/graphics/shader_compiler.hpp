#pragma once

#include <d3d11.h>

#include <string>
#include <string_view>

namespace ksge {

bool compileShaderSource(
    std::string_view source,
    std::string_view entry,
    std::string_view target,
    ID3DBlob*& bytecodeOut,
    std::string& errorOut);

bool createVertexShader(
    ID3D11Device* device,
    std::string_view source,
    ID3D11VertexShader*& shaderOut,
    ID3DBlob*& bytecodeOut,
    std::string& errorOut);

bool createPixelShader(
    ID3D11Device* device,
    std::string_view source,
    ID3D11PixelShader*& shaderOut,
    std::string& errorOut);

}