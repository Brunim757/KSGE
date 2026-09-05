#pragma once

namespace ksge {
namespace shaders {

inline constexpr const char* kPbrVertex = R"(
struct VSIn
{
    float3 position : POSITION;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD;
    float4 tangent : TANGENT;
};

struct VSOut
{
    float4 position : SV_Position;
    float3 world : WORLD;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD;
    float4 tangent : TANGENT;
};

cbuffer SceneCB : register(b0)
{
    row_major float4x4 gViewProj;
    float4 gCamPos;
    float4 gSunDir;
    float4 gSunColor;
    float4 gSkyTop;
    float4 gSkyHorizon;
};

cbuffer ObjectCB : register(b1)
{
    row_major float4x4 gWorld;
    float4 gBaseColorFactor;
    float4 gMRAO;
    float4 gEmissive;
    float4 gHasTextures;
};

VSOut main(VSIn input)
{
    float4 world = mul(float4(input.position, 1.0), gWorld);

    VSOut output;
    output.position = mul(world, gViewProj);
    output.world = world.xyz;
    output.normal = mul(float4(input.normal, 0.0), gWorld).xyz;
    output.uv = input.uv;
    output.tangent = normalize(float4(mul(float4(input.tangent.xyz, 0.0), gWorld).xyz, input.tangent.w));
    return output;
}
)";

inline constexpr const char* kPbrPixel = R"(
Texture2D gBaseTexture : register(t0);
Texture2D gMRTexture : register(t1);
Texture2D gNormalTexture : register(t2);
Texture2D gOccTexture : register(t3);
Texture2D gEmissiveTexture : register(t4);
SamplerState gSampler : register(s0);

struct VSOut
{
    float4 position : SV_Position;
    float3 world : WORLD;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD;
    float4 tangent : TANGENT;
};

cbuffer SceneCB : register(b0)
{
    row_major float4x4 gViewProj;
    float4 gCamPos;
    float4 gSunDir;
    float4 gSunColor;
    float4 gSkyTop;
    float4 gSkyHorizon;
};

cbuffer ObjectCB : register(b1)
{
    row_major float4x4 gWorld;
    float4 gBaseColorFactor;
    float4 gMRAO;
    float4 gEmissive;
    float4 gHasTextures;
};

float distributionGGX(float nDotH, float roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float denom = nDotH * nDotH * (a2 - 1.0) + 1.0;
    return a2 / max(3.14159265 * denom * denom, 1e-5);
}

float geometrySchlickGGX(float nDotV, float roughness)
{
    float r = roughness + 1.0;
    float k = r * r / 8.0;
    return nDotV / max(nDotV * (1.0 - k) + k, 1e-5);
}

float geometrySmith(float nDotV, float nDotL, float roughness)
{
    return geometrySchlickGGX(nDotV, roughness) * geometrySchlickGGX(nDotL, roughness);
}

float3 fresnelSchlick(float cosTheta, float3 f0)
{
    return f0 + (1.0 - f0) * pow(1.0 - saturate(cosTheta), 5.0);
}

float4 main(VSOut input) : SV_Target
{
    float hasBase = gHasTextures.x;
    float hasMR = gHasTextures.y;
    float hasNormal = gHasTextures.z;
    float hasOcc = gHasTextures.w;

    float4 baseColorSample = gBaseTexture.Sample(gSampler, input.uv);
    float4 albedoSample = lerp(float4(1.0, 1.0, 1.0, 1.0), baseColorSample, hasBase);
    float4 baseColor = gBaseColorFactor * albedoSample;

    float mraoSampleB = gMRTexture.Sample(gSampler, input.uv).b;
    float mraoSampleG = gMRTexture.Sample(gSampler, input.uv).g;
    float metallic = lerp(gMRAO.x, mraoSampleB, hasMR);
    float roughness = lerp(gMRAO.y, mraoSampleG, hasMR);
    float ao = lerp(gMRAO.z, gOccTexture.Sample(gSampler, input.uv).r, hasOcc);

    float3 normal = normalize(input.normal);
    if (hasNormal > 0.5)
    {
        float4 tangent = input.tangent;
        float3 binormal = normalize(cross(normal, tangent.xyz) * tangent.w);
        float3x3 tbn = float3x3(tangent.xyz, binormal, normal);
        float3 mapped = gNormalTexture.Sample(gSampler, input.uv).xyz * 2.0 - 1.0;
        normal = normalize(mul(mapped, tbn));
    }

    float4 emissiveSample = gEmissiveTexture.Sample(gSampler, input.uv);
    float3 emissive = gEmissive.xyz * lerp(float3(1.0, 1.0, 1.0), emissiveSample.xyz, 1.0);

    float3 viewDir = normalize(gCamPos.xyz - input.world);
    float3 lightDir = normalize(gSunDir.xyz);

    float3 f0 = lerp(float3(0.04, 0.04, 0.04), baseColor.xyz, metallic);
    float3 halfDir = normalize(lightDir + viewDir);

    float nDotL = saturate(dot(normal, lightDir));
    float nDotV = saturate(dot(normal, viewDir));
    float nDotH = saturate(dot(normal, halfDir));

    float ndf = distributionGGX(nDotH, roughness);
    float geometry = geometrySmith(nDotV, nDotL, roughness);
    float3 fresnel = fresnelSchlick(nDotH, f0);

    float3 specular = ndf * geometry * fresnel / max(4.0 * nDotV * nDotL, 1e-4);
    float3 diffuse = (1.0 - fresnel) * (1.0 - metallic) * baseColor.xyz / 3.14159265;

    float3 direct = (diffuse + specular) * gSunColor.xyz * nDotL * gSunColor.w;

    float3 ambient = lerp(gSkyHorizon.xyz, gSkyTop.xyz, saturate(normal.y * 0.5 + 0.5));
    float3 indirect = ambient * baseColor.xyz * ao;

    float3 result = direct + indirect + emissive;
    return float4(result, baseColor.a);
}
)";

inline constexpr const char* kSkyVertex = R"(
struct VSIn
{
    float3 position : POSITION;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD;
    float4 tangent : TANGENT;
};

struct VSOut
{
    float4 position : SV_Position;
    float3 world : WORLD;
};

cbuffer SkyCB : register(b0)
{
    row_major float4x4 gViewProj;
    float4 gCamPos;
    float4 gSunDir;
    float4 gSkyTop;
    float4 gSkyHorizon;
};

VSOut main(VSIn input)
{
    float3 world = input.position;

    VSOut output;
    output.position = mul(float4(world, 1.0), gViewProj).xyww;
    output.world = input.position;
    return output;
}
)";

inline constexpr const char* kSkyPixel = R"(
struct VSOut
{
    float4 position : SV_Position;
    float3 world : WORLD;
};

cbuffer SkyCB : register(b0)
{
    row_major float4x4 gViewProj;
    float4 gCamPos;
    float4 gSunDir;
    float4 gSkyTop;
    float4 gSkyHorizon;
};

float4 main(VSOut input) : SV_Target
{
    float3 direction = normalize(input.world);
    float up = saturate(direction.y);
    float skyMix = smoothstep(-0.2, 0.5, direction.y);
    float3 sky = lerp(gSkyHorizon.xyz, gSkyTop.xyz, skyMix);

    float sunAmount = pow(max(dot(direction, normalize(gSunDir.xyz)), 0.0), 96.0);
    float3 sunGlow = gSkyTop.xyz * sunAmount * 2.0;
    sunGlow += pow(max(dot(direction, normalize(gSunDir.xyz)), 0.0), 8.0) * gSkyHorizon.xyz * 0.15;

    float horizonGlow = pow(1.0 - min(abs(direction.y), 1.0), 3.0);
    sky += (gSkyHorizon.xyz * 0.35) * horizonGlow;

    return float4(sky + sunGlow, 1.0);
}
)";

}
}