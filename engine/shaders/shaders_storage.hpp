#pragma once

#include <string>

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

inline constexpr const char* kPostVertex = R"(
struct VSOut
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD;
};

VSOut main(uint vertexId : SV_VertexID)
{
    float2 grid = float2((vertexId << 1u) & 2u, vertexId & 2u);
    VSOut output;
    output.position = float4(grid * float2(2.0, -2.0) + float2(-1.0, 1.0), 0.0, 1.0);
    output.uv = grid;
    return output;
}
)";

inline constexpr const char* kPostPreamble = R"(
struct VSOut
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD;
};

cbuffer PostCB : register(b0)
{
    row_major float4x4 gViewProj;
    row_major float4x4 gInvViewProj;
    float4 gCameraPos;
    float4 gViewport;
    float4 gTargetSize;
    float4 gDebugView;
    float4 gCamNearFar;
    float4 gSun;
    float4 gSunColor;
    float4 gFogParams;
    float4 gSsaoParams;
    float4 gBloomParams;
    float4 gCompositeParams;
};

float3 reconstructWorld(float2 uv, float depth)
{
    float4 ndc = float4(uv.x * 2.0 - 1.0, 1.0 - uv.y * 2.0, depth, 1.0);
    float4 world = mul(ndc, gInvViewProj);
    return world.xyz / world.w;
}

bool finiteValue(float x)
{
    return x * 0.0 == 0.0 && x == x;
}
)";

inline constexpr const char* kSsaoBody = R"(
Texture2D gDepth : register(t0);
Texture2D gNoise : register(t1);
SamplerState gPointSampler : register(s0);

static const float3 gKernel[16] = {
    float3(0.23, -0.71, 0.67),
    float3(-0.49, 0.22, 0.84),
    float3(0.66, 0.50, 0.56),
    float3(0.28, 0.90, 0.34),
    float3(-0.87, -0.17, 0.46),
    float3(0.94, -0.28, 0.20),
    float3(-0.33, 0.61, 0.72),
    float3(0.08, -0.40, 0.91),
    float3(-0.64, -0.45, 0.62),
    float3(0.75, -0.58, 0.31),
    float3(0.44, 0.79, 0.43),
    float3(-0.16, 0.97, 0.18),
    float3(0.12, 0.20, 0.97),
    float3(-0.95, 0.28, 0.13),
    float3(0.55, -0.05, 0.83),
    float3(-0.71, 0.55, 0.44),
};

float4 main(VSOut input) : SV_Target
{
    float depth = gDepth.Sample(gPointSampler, input.uv).r;
    if (depth >= 1.0 || depth <= 0.0)
    {
        return float4(1.0, 1.0, 1.0, 1.0);
    }

    float3 center = reconstructWorld(input.uv, depth);
    if (!finiteValue(center.x) || !finiteValue(center.y) || !finiteValue(center.z))
    {
        return float4(1.0, 1.0, 1.0, 1.0);
    }

    float2 rightUv = input.uv + float2(gViewport.z, 0.0);
    float2 upUv = input.uv + float2(0.0, gViewport.w);
    float3 right = reconstructWorld(rightUv, gDepth.Sample(gPointSampler, rightUv).r);
    float3 up = reconstructWorld(upUv, gDepth.Sample(gPointSampler, upUv).r);

    float3 side = cross(right - center, up - center);
    float3 normal = dot(side, side) > 1e-8 ? normalize(side) : float3(0.0, 1.0, 0.0);

    float3 random = gNoise.Sample(gPointSampler, input.uv * gTargetSize.xy / 4.0).xyz * 2.0 - 1.0;
    float3 tangent = normalize(random - normal * dot(random, normal));
    float3 bitangent = cross(normal, tangent);
    float3x3 tbn = float3x3(tangent, bitangent, normal);

    float occlusion = 0.0;
    for (uint i = 0u; i < 16u; ++i)
    {
        float scale = (float(i) + 1.0) / 16.0;
        float3 sampleDir = mul(gKernel[i], tbn);
        float3 samplePos = center + sampleDir * gSsaoParams.x * scale;

        float4 projected = mul(float4(samplePos, 1.0), gViewProj);
        float2 sampleUv = projected.xy / max(projected.w, 1e-5) * 0.5 + 0.5;
        sampleUv.y = 1.0 - sampleUv.y;
        if (projected.w <= 0.0 || any(sampleUv <= 0.0) || any(sampleUv >= 1.0))
        {
            continue;
        }

        float sampleDepth = gDepth.Sample(gPointSampler, sampleUv).r;
        if (sampleDepth >= 1.0)
        {
            continue;
        }
        float3 sampleWorld = reconstructWorld(sampleUv, sampleDepth);
        float viewSample = length(sampleWorld - gCameraPos.xyz);
        float viewPos = length(samplePos - gCameraPos.xyz);
        float rangeCheck = smoothstep(0.0, 1.0, gSsaoParams.x / (abs(viewSample - viewPos) + 0.02));
        occlusion += (viewSample < viewPos ? 1.0 : 0.0) * rangeCheck;
    }

    float average = occlusion / 16.0;
    float openness = 1.0 - smoothstep(0.0, 0.8, average);
    return float4(openness, openness, openness, 1.0);
}
)";

inline constexpr const char* kSsaoBlurHBody = R"(
Texture2D gSsao : register(t0);
Texture2D gDepth : register(t1);
SamplerState gPointSampler : register(s0);

static const float gBlurWeights[5] = {0.227027, 0.1945946, 0.1216216, 0.054054, 0.016216};

float4 main(VSOut input) : SV_Target
{
    float depth = gDepth.Sample(gPointSampler, input.uv).r;
    float total = 0.0;
    float weightSum = 0.0;

    for (int tap = -2; tap <= 2; ++tap)
    {
        float2 offset = float2(float(tap) * gTargetSize.z, 0.0);
        float sampleDepth = gDepth.Sample(gPointSampler, input.uv + offset).r;
        float depthWeight = 1.0 - smoothstep(0.0, gSsaoParams.x * 0.05, abs(sampleDepth - depth));
        float weight = depthWeight * gBlurWeights[tap + 2];
        float sampleAo = gSsao.Sample(gPointSampler, input.uv + offset).r;
        total += sampleAo * weight;
        weightSum += weight;
    }

    float output = weightSum > 0.0 ? total / weightSum : 1.0;
    return float4(output, output, output, 1.0);
}
)";

inline constexpr const char* kSsaoBlurVBody = R"(
Texture2D gSsao : register(t0);
Texture2D gDepth : register(t1);
SamplerState gPointSampler : register(s0);

static const float gBlurWeights[5] = {0.227027, 0.1945946, 0.1216216, 0.054054, 0.016216};

float4 main(VSOut input) : SV_Target
{
    float depth = gDepth.Sample(gPointSampler, input.uv).r;
    float total = 0.0;
    float weightSum = 0.0;

    for (int tap = -2; tap <= 2; ++tap)
    {
        float2 offset = float2(0.0, float(tap) * gTargetSize.w);
        float sampleDepth = gDepth.Sample(gPointSampler, input.uv + offset).r;
        float depthWeight = 1.0 - smoothstep(0.0, gSsaoParams.x * 0.05, abs(sampleDepth - depth));
        float weight = depthWeight * gBlurWeights[tap + 2];
        float sampleAo = gSsao.Sample(gPointSampler, input.uv + offset).r;
        total += sampleAo * weight;
        weightSum += weight;
    }

    float output = weightSum > 0.0 ? total / weightSum : 1.0;
    return float4(output, output, output, 1.0);
}
)";

inline constexpr const char* kFogBody = R"(
Texture2D gDepth : register(t0);
SamplerState gPointSampler : register(s0);

float henyeyGreenstein(float cosTheta)
{
    const float g = 0.3;
    const float g2 = g * g;
    float denom = 1.0 + g2 - 2.0 * g * cosTheta;
    return (1.0 - g2) / (4.0 * 3.14159265 * pow(denom, 1.5));
}

float4 main(VSOut input) : SV_Target
{
    float depth = gDepth.Sample(gPointSampler, input.uv).r;

    float3 end = reconstructWorld(input.uv, depth);
    if (!finiteValue(end.x) || !finiteValue(end.y) || !finiteValue(end.z))
    {
        return float4(0.0, 0.0, 0.0, 1.0);
    }
    float3 viewDir = normalize(end - gCameraPos.xyz);
    float distance = length(end - gCameraPos.xyz);
    if (distance <= 0.0001)
    {
        return float4(0.0, 0.0, 0.0, 1.0);
    }

    float stepSize = distance / 16.0;
    float3 scattering = float3(0.0, 0.0, 0.0);
    float transmittance = 1.0;
    float3 sunDir = normalize(gSun.xyz);

    for (uint i = 0u; i < 16u; ++i)
    {
        float3 samplePoint = gCameraPos.xyz + viewDir * (float(i) + 0.5) * stepSize;
        float height = max(samplePoint.y - gFogParams.z, 0.0);
        float density = gFogParams.x * exp(-height * gFogParams.y);
        float fogAmount = 1.0 - exp(-density * stepSize);

        float phase = henyeyGreenstein(dot(viewDir, sunDir));
        scattering += gSunColor.xyz * gSun.w * phase * fogAmount * transmittance;
        transmittance *= 1.0 - fogAmount;
    }

    return float4(scattering, transmittance);
}
)";

inline constexpr const char* kBloomExtractBody = R"(
Texture2D gScene : register(t0);
SamplerState gLinearSampler : register(s1);

float4 main(VSOut input) : SV_Target
{
    float4 color = gScene.Sample(gLinearSampler, input.uv);
    float brightness = max(color.r, max(color.g, color.b));
    float knee = gBloomParams.y;
    float soft = max(brightness - gBloomParams.x + knee, 0.0);
    soft = soft * soft / max(4.0 * knee, 1e-4);
    float contribution = max(soft, brightness - gBloomParams.x);
    contribution = contribution / max(brightness, 1e-4);
    return float4(color.xyz * saturate(contribution), 1.0);
}
)";

inline constexpr const char* kBloomDownsampleBody = R"(
Texture2D gScene : register(t0);
SamplerState gLinearSampler : register(s1);

float4 main(VSOut input) : SV_Target
{
    float3 accumulated =
        gScene.Sample(gLinearSampler, input.uv + float2(-0.5, -0.5) * gTargetSize.zw).xyz +
        gScene.Sample(gLinearSampler, input.uv + float2(0.5, -0.5) * gTargetSize.zw).xyz +
        gScene.Sample(gLinearSampler, input.uv + float2(-0.5, 0.5) * gTargetSize.zw).xyz +
        gScene.Sample(gLinearSampler, input.uv + float2(0.5, 0.5) * gTargetSize.zw).xyz;
    return float4(accumulated * 0.25, 1.0);
}
)";

inline constexpr const char* kBloomBlurHBody = R"(
Texture2D gScene : register(t0);
SamplerState gLinearSampler : register(s1);

static const float gBloomWeights[9] = {
    0.01621622, 0.05405405, 0.12162162, 0.19459459, 0.22702703,
    0.19459459, 0.12162162, 0.05405405, 0.01621622,
};

float4 main(VSOut input) : SV_Target
{
    float3 accumulated = float3(0.0, 0.0, 0.0);
    float weightSum = 0.0;

    for (int tap = -4; tap <= 4; ++tap)
    {
        float2 offset = float2(float(tap) * gTargetSize.z, 0.0);
        float weight = gBloomWeights[tap + 4];
        accumulated += gScene.Sample(gLinearSampler, input.uv + offset).xyz * weight;
        weightSum += weight;
    }

    return float4(accumulated / weightSum, 1.0);
}
)";

inline constexpr const char* kBloomBlurVBody = R"(
Texture2D gScene : register(t0);
SamplerState gLinearSampler : register(s1);

static const float gBloomWeights[9] = {
    0.01621622, 0.05405405, 0.12162162, 0.19459459, 0.22702703,
    0.19459459, 0.12162162, 0.05405405, 0.01621622,
};

float4 main(VSOut input) : SV_Target
{
    float3 accumulated = float3(0.0, 0.0, 0.0);
    float weightSum = 0.0;

    for (int tap = -4; tap <= 4; ++tap)
    {
        float2 offset = float2(0.0, float(tap) * gTargetSize.w);
        float weight = gBloomWeights[tap + 4];
        accumulated += gScene.Sample(gLinearSampler, input.uv + offset).xyz * weight;
        weightSum += weight;
    }

    return float4(accumulated / weightSum, 1.0);
}
)";

inline constexpr const char* kBloomUpsampleBody = R"(
Texture2D gCurrent : register(t0);
Texture2D gUpper : register(t1);
SamplerState gLinearSampler : register(s1);

float4 main(VSOut input) : SV_Target
{
    float3 current = gCurrent.Sample(gLinearSampler, input.uv).xyz;
    float3 upper = gUpper.Sample(gLinearSampler, input.uv).xyz;
    return float4(current + upper, 1.0);
}
)";

inline constexpr const char* kCompositeBody = R"(
Texture2D gScene : register(t0);
Texture2D gSsao : register(t1);
Texture2D gFog : register(t2);
Texture2D gBloomBase : register(t3);
Texture2D gBloomAccum : register(t4);
Texture3D gLut : register(t5);
Texture2D gDepth : register(t6);
SamplerState gPointSampler : register(s0);
SamplerState gLinearSampler : register(s1);

float3 acesToneMap(float3 color)
{
    return saturate((color * (2.51 * color + 0.03)) / (color * (2.43 * color + 0.59) + 0.14));
}

float4 main(VSOut input) : SV_Target
{
    float3 scene = gScene.Sample(gLinearSampler, input.uv).xyz;
    float ao = gSsao.Sample(gLinearSampler, input.uv).r;
    float4 fog = gFog.Sample(gLinearSampler, input.uv);
    float3 bloom = gBloomBase.Sample(gLinearSampler, input.uv).xyz +
                   gBloomAccum.Sample(gLinearSampler, input.uv).xyz;
    float depth = gDepth.Sample(gPointSampler, input.uv).r;

    uint mode = uint(gDebugView.x);
    if (mode == 1u)
    {
        return float4(scene, 1.0);
    }
    if (mode == 2u)
    {
        return float4(ao.xxx, 1.0);
    }
    if (mode == 3u)
    {
        return float4(fog.rgb + (1.0 - fog.a), 1.0);
    }
    if (mode == 4u)
    {
        return float4(bloom * 2.0, 1.0);
    }
    if (mode == 5u)
    {
        return float4((1.0 - depth).xxx, 1.0);
    }
    if (mode == 6u)
    {
        return float4(acesToneMap(scene), 1.0);
    }

    float3 color = scene;
    color *= 1.0 - (1.0 - ao) * saturate(gCompositeParams.x);
    color = color * (1.0 - (1.0 - fog.a) * gCompositeParams.z) + fog.rgb * gCompositeParams.z;
    color += bloom * gCompositeParams.y;
    color = acesToneMap(color);
    color = gLut.Sample(gLinearSampler, saturate(color)).xyz;
    color = pow(color, 1.0 / 2.2);
    return float4(color, 1.0);
}
)";

inline constexpr const char* kPostCopyBody = R"(
Texture2D gScene : register(t0);
SamplerState gLinearSampler : register(s1);

float4 main(VSOut input) : SV_Target
{
    return float4(gScene.Sample(gLinearSampler, input.uv).rgb, 1.0);
}
)";

inline std::string postProcessPixelShader(const char* body)
{
    std::string source(kPostPreamble);
    source.append(body);
    return source;
}

}
}