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

inline constexpr const char* kGBufferPixel = R"(
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

cbuffer ObjectCB : register(b1)
{
    row_major float4x4 gWorld;
    float4 gBaseColorFactor;
    float4 gMRAO;
    float4 gEmissive;
    float4 gHasTextures;
};

struct GBufferOut
{
    float4 color : SV_Target0;
    float4 normal : SV_Target1;
    float4 data : SV_Target2;
};

GBufferOut main(VSOut input)
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

    GBufferOut output;
    output.color = float4(baseColor.xyz, metallic);
    output.normal = float4(normal * 0.5 + 0.5, roughness);
    output.data = float4(emissive, ao);
    return output;
}
)";

inline constexpr const char* kGBufferInstancedVertex = R"(
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

StructuredBuffer<float4x4> gInstances : register(t0);

cbuffer SceneCB : register(b0)
{
    row_major float4x4 gViewProj;
    float4 gCamPos;
    float4 gSunDir;
    float4 gSunColor;
    float4 gSkyTop;
    float4 gSkyHorizon;
};

VSOut main(VSIn input, uint instanceId : SV_InstanceID)
{
    float4x4 world = gInstances[instanceId];
    float4 worldPosition = mul(float4(input.position, 1.0), world);

    VSOut output;
    output.position = mul(worldPosition, gViewProj);
    output.world = worldPosition.xyz;
    output.normal = mul(float4(input.normal, 0.0), world).xyz;
    output.uv = input.uv;
    output.tangent = normalize(float4(mul(float4(input.tangent.xyz, 0.0), world).xyz, input.tangent.w));
    return output;
}
)";

inline constexpr const char* kShadowInstancedVertex = R"(
struct VSIn
{
    float3 position : POSITION;
};

StructuredBuffer<float4x4> gInstances : register(t0);

cbuffer ShadowCB : register(b0)
{
    row_major float4x4 gLightViewProj;
};

float4 main(VSIn input, uint instanceId : SV_InstanceID) : SV_Position
{
    float4x4 world = gInstances[instanceId];
    return mul(mul(float4(input.position, 1.0), world), gLightViewProj);
}
)";

inline constexpr const char* kGridPixel = R"(
struct VSOut
{
    float4 position : SV_Position;
    float3 world : WORLD;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD;
    float4 tangent : TANGENT;
};

struct GBufferOut
{
    float4 color : SV_Target0;
    float4 normal : SV_Target1;
    float4 data : SV_Target2;
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

float edgeCoverage(float2 coord)
{
    float2 d = abs(frac(coord) - 0.0);
    d = min(d, 1.0 - d);
    float2 width = max(fwidth(coord) * 0.5, 0.0005);
    float2 coverage = 1.0 - smoothstep(0.0, width, d);
    return max(coverage.x, coverage.y);
}

GBufferOut main(VSOut input)
{
    float2 gridCoord = input.world.xz;
    float minorCoverage = edgeCoverage(gridCoord);
    float majorCoverage = edgeCoverage(gridCoord / 10.0);
    float originCoverage = edgeCoverage(gridCoord / 4.0);

    float distanceToCamera = length(input.world.xyz - gCamPos.xyz);
    float distanceFade = 1.0 - smoothstep(30.0, 120.0, distanceToCamera);
    float minorVisibility = max(minorCoverage * (1.0 - majorCoverage), 0.0) * distanceFade;
    float majorVisibility = majorCoverage * distanceFade;
    float originVisibility = originCoverage * distanceFade;

    float visibility = max(max(minorVisibility, majorVisibility), originVisibility);
    if (visibility <= 0.004)
    {
        discard;
    }

    float3 minorColor = float3(0.42, 0.46, 0.52);
    float3 majorColor = float3(0.72, 0.78, 0.86);
    float3 originColor = float3(0.95, 0.97, 1.0);
    float3 lineColor = minorColor;
    lineColor = lerp(lineColor, majorColor, saturate(majorVisibility / max(visibility, 1e-4)));
    lineColor = lerp(lineColor, originColor, saturate(originVisibility / max(visibility, 1e-4)));

    GBufferOut output;
    output.color = float4(lineColor * min(visibility * 2.0, 1.0), 0.0);
    output.normal = float4(0.5, 1.0, 0.5, 1.0);
    output.data = float4(0.0, 0.0, 0.0, 1.0);
    return output;
}
)";

inline constexpr const char* kShadowPixel = R"(
float4 main() : SV_Target
{
    return float4(0.0, 0.0, 0.0, 1.0);
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
    float4 gSkyTop;
    float4 gSkyHorizon;
    float4 gFogParams;
    float4 gSsaoParams;
    float4 gBloomParams;
    float4 gShadowSplits;
    float4 gShadowParams;
    float4 gCompositeParams;
    row_major float4x4 gShadowViewProj[3];
    row_major float4x4 gPrevViewProj;
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

float linearViewDepth(float depth)
{
    return (gCamNearFar.x * gCamNearFar.y) /
        max(gCamNearFar.y - depth * (gCamNearFar.y - gCamNearFar.x), 1e-5);
}

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

float3 skyGradientColor(float3 direction)
{
    float up = saturate(direction.y);
    float skyMix = smoothstep(-0.2, 0.5, direction.y);
    float3 sky = lerp(gSkyHorizon.xyz, gSkyTop.xyz, skyMix);

    float sunAmount = pow(max(dot(direction, normalize(gSun.xyz)), 0.0), 96.0);
    float3 sunGlow = gSkyTop.xyz * sunAmount * 2.0;
    sunGlow += pow(max(dot(direction, normalize(gSun.xyz)), 0.0), 8.0) * gSkyHorizon.xyz * 0.15;

    float horizonGlow = pow(1.0 - min(abs(direction.y), 1.0), 3.0);
    sky += (gSkyHorizon.xyz * 0.35) * horizonGlow;

    return sky + sunGlow;
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
    if (dot(normal, gCameraPos.xyz - center) < 0.0)
    {
        normal = -normal;
    }

    float3 random = gNoise.Sample(gPointSampler, input.uv * gTargetSize.xy / 4.0).xyz * 2.0 - 1.0;
    float3 tangent = normalize(random - normal * dot(random, normal));
    float3 bitangent = cross(normal, tangent);
    float3x3 tbn = float3x3(tangent, bitangent, normal);

    float centerDistance = max(length(center - gCameraPos.xyz), 1e-4);
    float distanceScale = saturate(centerDistance / 25.0);
    distanceScale = max(distanceScale, 0.15);

    float occlusion = 0.0;
    for (uint i = 0u; i < 16u; ++i)
    {
        float scale = (float(i) + 1.0) / 16.0;
        float3 sampleDir = mul(gKernel[i], tbn);
        float3 samplePos = center + sampleDir * gSsaoParams.x * scale * distanceScale;

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

inline constexpr const char* kDeferredLightBody = R"(
Texture2D gBufA : register(t0);
Texture2D gBufB : register(t1);
Texture2D gBufC : register(t2);
Texture2D gDepth : register(t3);
Texture2D gSsao : register(t4);
Texture2D gShadowMap0 : register(t5);
Texture2D gShadowMap1 : register(t6);
Texture2D gShadowMap2 : register(t7);
SamplerState gPointSampler : register(s0);
SamplerState gLinearSampler : register(s1);
SamplerComparisonState gShadowSampler : register(s2);

float sampleShadowCascade(uint cascade, float3 worldPos)
{
    float4 projected = mul(float4(worldPos, 1.0), gShadowViewProj[cascade]);
    projected.xy /= max(projected.w, 1e-5);
    if (any(projected.xy <= -1.0) || any(projected.xy >= 1.0) || projected.w <= 0.0)
    {
        return 1.0;
    }
    float2 uv = projected.xy * 0.5 + 0.5;
    uv.y = 1.0 - uv.y;
    float reference = projected.z - gShadowParams.z;
    float shadowed = 0.0;
    [unroll]
    for (int row = -2; row <= 2; ++row)
    {
        [unroll]
        for (int column = -2; column <= 2; ++column)
        {
            float2 offset = (float2(float(column), float(row)) + 0.5) * gShadowParams.xy;
            if (cascade == 0u)
            {
                shadowed += gShadowMap0.SampleCmpLevelZero(gShadowSampler, uv + offset, reference);
            }
            else if (cascade == 1u)
            {
                shadowed += gShadowMap1.SampleCmpLevelZero(gShadowSampler, uv + offset, reference);
            }
            else
            {
                shadowed += gShadowMap2.SampleCmpLevelZero(gShadowSampler, uv + offset, reference);
            }
        }
    }
    return 1.0 - shadowed / 25.0;
}

float4 main(VSOut input) : SV_Target
{
    float depth = gDepth.Sample(gPointSampler, input.uv).r;
    if (depth >= 1.0)
    {
        return float4(0.0, 0.0, 0.0, 1.0);
    }

    float3 worldPos = reconstructWorld(input.uv, depth);
    if (!finiteValue(worldPos.x) || !finiteValue(worldPos.y) || !finiteValue(worldPos.z))
    {
        return float4(0.0, 0.0, 0.0, 1.0);
    }

    float4 bufferA = gBufA.Sample(gLinearSampler, input.uv);
    float4 bufferB = gBufB.Sample(gLinearSampler, input.uv);
    float4 bufferC = gBufC.Sample(gLinearSampler, input.uv);

    float3 albedo = bufferA.rgb;
    float metallic = bufferA.a;
    float3 normal = normalize(bufferB.xyz * 2.0 - 1.0);
    float roughness = bufferB.a;
    float3 emissive = bufferC.rgb;
    float occlusion = bufferC.a;
    if (length(normal) < 0.5)
    {
        return float4(0.0, 0.0, 0.0, 1.0);
    }

    float3 viewDir = normalize(gCameraPos.xyz - worldPos);
    float3 lightDir = normalize(gSun.xyz);

    float3 f0 = lerp(float3(0.04, 0.04, 0.04), albedo, metallic);
    float3 halfDir = normalize(lightDir + viewDir);

    float nDotL = saturate(dot(normal, lightDir));
    float nDotV = saturate(dot(normal, viewDir));
    float nDotH = saturate(dot(normal, halfDir));

    float ndf = distributionGGX(nDotH, roughness);
    float geometry = geometrySmith(nDotV, nDotL, roughness);
    float3 fresnel = fresnelSchlick(nDotH, f0);

    float3 specular = ndf * geometry * fresnel / max(4.0 * nDotV * nDotL, 1e-4);
    float3 diffuse = (1.0 - fresnel) * (1.0 - metallic) * albedo / 3.14159265;

    float linearDepth = linearViewDepth(depth);
    uint cascade = 2u;
    if (linearDepth < gShadowSplits.x)
    {
        cascade = 0u;
    }
    else if (linearDepth < gShadowSplits.y)
    {
        cascade = 1u;
    }
    float light = sampleShadowCascade(cascade, worldPos);
    float splitHi = cascade == 0u ? gShadowSplits.x : gShadowSplits.y;
    float cascadeBlend = smoothstep(splitHi - gShadowParams.w, splitHi, linearDepth);
    if (cascade < 2u && cascadeBlend > 0.001)
    {
        float layer = sampleShadowCascade(cascade + 1u, worldPos);
        light = lerp(light, layer, cascadeBlend);
    }

    float3 direct = (diffuse + specular) * (gSunColor.xyz * nDotL * gSun.w) * light;

    float ao = gSsao.Sample(gLinearSampler, input.uv).r;
    float3 ambient = lerp(gSkyHorizon.xyz, gSkyTop.xyz, saturate(normal.y * 0.5 + 0.5)) * albedo * ao;

    return float4(direct + ambient + emissive, 1.0);
}
)";

inline constexpr const char* kSsrBody = R"(
Texture2D gScene : register(t0);
Texture2D gBufA : register(t1);
Texture2D gBufB : register(t2);
Texture2D gDepth : register(t3);
SamplerState gPointSampler : register(s0);
SamplerState gLinearSampler : register(s1);

float4 main(VSOut input) : SV_Target
{
    float depth = gDepth.Sample(gPointSampler, input.uv).r;
    if (depth >= 1.0)
    {
        return float4(0.0, 0.0, 0.0, 1.0);
    }

    float3 worldPos = reconstructWorld(input.uv, depth);
    float3 normal = normalize(gBufB.Sample(gLinearSampler, input.uv).xyz * 2.0 - 1.0);
    float roughness = gBufB.Sample(gLinearSampler, input.uv).a;
    float metallic = gBufA.Sample(gLinearSampler, input.uv).a;
    float3 albedo = gBufA.Sample(gLinearSampler, input.uv).rgb;
    if (length(normal) < 0.5)
    {
        return float4(0.0, 0.0, 0.0, 1.0);
    }

    float3 viewDir = normalize(gCameraPos.xyz - worldPos);
    float3 f0 = lerp(float3(0.04, 0.04, 0.04), albedo, metallic);
    float reflectivity = saturate(length(fresnelSchlick(dot(normal, viewDir), f0)));
    float smoothness = saturate(1.0 - roughness);
    float weight = reflectivity * smoothness;
    if (weight < 0.02)
    {
        return float4(0.0, 0.0, 0.0, 1.0);
    }

    float3 ray = normalize(reflect(-viewDir, normal));
    float stepLength = max(length(worldPos - gCameraPos.xyz) * 0.02, 0.05);

    float3 fallback = skyGradientColor(ray);
    float3 reflection = float3(0.0, 0.0, 0.0);
    float fade = 0.0;

    for (uint i = 0u; i < 24u; ++i)
    {
        float3 samplePoint = worldPos + ray * (stepLength * float(i + 1u));
        float4 projected = mul(float4(samplePoint, 1.0), gViewProj);
        projected.xy /= max(projected.w, 1e-5);
        float2 sampleUv = projected.xy * 0.5 + 0.5;
        sampleUv.y = 1.0 - sampleUv.y;
        if ((projected.w <= 0.0) || any(sampleUv <= 0.0) || any(sampleUv >= 1.0))
        {
            break;
        }
        float hitDepth = gDepth.Sample(gPointSampler, sampleUv).r;
        if (hitDepth >= 1.0)
        {
            break;
        }
        float3 hitWorld = reconstructWorld(sampleUv, hitDepth);
        float hitDistance = length(hitWorld - gCameraPos.xyz);
        float rayDistance = length(samplePoint - gCameraPos.xyz);
        if (hitDistance < rayDistance - 0.05)
        {
            float horizontal = 1.0 - smoothstep(0.55, 0.85, abs(sampleUv.x - 0.5) * 2.0);
            float vertical = 1.0 - smoothstep(0.55, 0.85, abs(sampleUv.y - 0.5) * 2.0);
            reflection = gScene.Sample(gLinearSampler, sampleUv).rgb;
            fade = (1.0 - float(i) / 24.0) * min(horizontal, vertical);
            break;
        }
    }

    float3 result = fade > 0.001 ? reflection * fade : fallback * 0.25 * smoothstep(0.0, 0.3, smoothness);
    return float4(result * weight, 1.0);
}
)";

inline constexpr const char* kSsgiBody = R"(
Texture2D gScene : register(t0);
Texture2D gBufA : register(t1);
Texture2D gBufB : register(t2);
Texture2D gDepth : register(t3);
SamplerState gPointSampler : register(s0);
SamplerState gLinearSampler : register(s1);

static const float2 gSsgiOffsets[8] = {
    float2(1.0, 0.0),
    float2(-1.0, 0.0),
    float2(0.0, 1.0),
    float2(0.0, -1.0),
    float2(0.707, 0.707),
    float2(0.707, -0.707),
    float2(-0.707, 0.707),
    float2(-0.707, -0.707),
};

float4 main(VSOut input) : SV_Target
{
    float depth = gDepth.Sample(gPointSampler, input.uv).r;
    if (depth >= 1.0)
    {
        return float4(0.0, 0.0, 0.0, 1.0);
    }

    float3 worldPos = reconstructWorld(input.uv, depth);
    float3 normal = normalize(gBufB.Sample(gLinearSampler, input.uv).xyz * 2.0 - 1.0);
    float3 albedo = gBufA.Sample(gLinearSampler, input.uv).rgb;
    float metallic = gBufA.Sample(gLinearSampler, input.uv).a;
    float3 albedoFactor = albedo * (1.0 - metallic);

    float centerDepth = linearViewDepth(depth);
    float radiusPixels = clamp(centerDepth * 0.02, 2.0, 16.0);
    float2 texel = gTargetSize.zw;

    float3 indirect = float3(0.0, 0.0, 0.0);
    float totalWeight = 0.0;
    for (uint i = 0u; i < 8u; ++i)
    {
        float2 offset = gSsgiOffsets[i] * radiusPixels * texel;
        float2 sampleUv = input.uv + offset;
        if (any(sampleUv <= 0.0) || any(sampleUv >= 1.0))
        {
            continue;
        }
        float sampleDepthRaw = gDepth.Sample(gPointSampler, sampleUv).r;
        if (sampleDepthRaw >= 1.0)
        {
            continue;
        }
        float sampleDepth = linearViewDepth(sampleDepthRaw);
        float weight = 1.0 - saturate(abs(sampleDepth - centerDepth) / max(centerDepth * 0.25, 0.5));
        indirect += gScene.Sample(gLinearSampler, sampleUv).rgb * weight;
        totalWeight += weight;
    }

    float3 result = totalWeight > 1e-4 ? indirect / totalWeight : float3(0.0, 0.0, 0.0);
    return float4(result * albedoFactor * 0.35, 1.0);
}
)";

inline constexpr const char* kSkyPostBody = R"(
Texture2D gDepth : register(t0);
SamplerState gPointSampler : register(s0);

float4 main(VSOut input) : SV_Target
{
    float depth = gDepth.Sample(gPointSampler, input.uv).r;
    if (depth < 1.0 - 1e-4)
    {
        discard;
    }
    float3 direction = normalize(reconstructWorld(input.uv, 1.0) - gCameraPos.xyz);
    return float4(skyGradientColor(direction), 1.0);
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
Texture2D gSsr : register(t7);
Texture2D gSsgi : register(t8);
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
    color += gSsgi.Sample(gLinearSampler, input.uv).rgb * gCompositeParams.x;
    color += gSsr.Sample(gLinearSampler, input.uv).rgb * gCompositeParams.y;
    color = color * (1.0 - (1.0 - fog.a) * gCompositeParams.z) + fog.rgb * gCompositeParams.z;
    color += bloom * gBloomParams.z;
    color = acesToneMap(color);
    color = gLut.Sample(gLinearSampler, saturate(color)).xyz;
    color = pow(color, 1.0 / 2.2);
    return float4(color, 1.0);
}
)";

inline constexpr const char* kSceneBlendBody = R"(
Texture2D gScene : register(t0);
Texture2D gSsr : register(t1);
Texture2D gSsgi : register(t2);
Texture2D gFog : register(t3);
Texture2D gBloomBase : register(t4);
Texture2D gBloomAccum : register(t5);
SamplerState gLinearSampler : register(s1);

float4 main(VSOut input) : SV_Target
{
    float3 scene = gScene.Sample(gLinearSampler, input.uv).rgb;
    float3 color = scene;
    color += gSsgi.Sample(gLinearSampler, input.uv).rgb * gCompositeParams.x;
    color += gSsr.Sample(gLinearSampler, input.uv).rgb * gCompositeParams.y;
    float4 fog = gFog.Sample(gLinearSampler, input.uv);
    color = color * (1.0 - (1.0 - fog.a) * gCompositeParams.z) + fog.rgb * gCompositeParams.z;
    color += (gBloomBase.Sample(gLinearSampler, input.uv).rgb +
              gBloomAccum.Sample(gLinearSampler, input.uv).rgb) * gBloomParams.z;
    return float4(color, 1.0);
}
)";

inline constexpr const char* kTaaBody = R"(
Texture2D gInput : register(t0);
Texture2D gHistory : register(t1);
Texture2D gDepth : register(t2);
SamplerState gPointSampler : register(s0);
SamplerState gLinearSampler : register(s1);

float3 taaClamp(float2 uv, Texture2D source, float2 texel)
{
    float3 center = source.Sample(gLinearSampler, uv).rgb;
    float3 minColor = center;
    float3 maxColor = center;
    [unroll]
    for (int y = -1; y <= 1; ++y)
    {
        [unroll]
        for (int x = -1; x <= 1; ++x)
        {
            if (x == 0 && y == 0)
            {
                continue;
            }
            float3 sampleColor = source.Sample(gLinearSampler, uv + float2(float(x), float(y)) * texel).rgb;
            minColor = min(minColor, sampleColor);
            maxColor = max(maxColor, sampleColor);
        }
    }
    return (minColor + maxColor) * 0.5;
}

float4 main(VSOut input) : SV_Target
{
    float3 current = gInput.Sample(gLinearSampler, input.uv).rgb;
    float depth = gDepth.Sample(gPointSampler, input.uv).r;

    float3 worldPos = reconstructWorld(input.uv, depth);
    float4 previousClip = mul(float4(worldPos, 1.0), gPrevViewProj);
    float2 previousUv = previousClip.xy / max(previousClip.w, 1e-5) * 0.5 + 0.5;
    previousUv.y = 1.0 - previousUv.y;

    float2 texel = gTargetSize.zw;
    float3 clamped = taaClamp(input.uv, gInput, texel);
    float3 result = current;
    if (previousClip.w > 0.0 && previousUv.x > 0.0 && previousUv.x < 1.0 &&
        previousUv.y > 0.0 && previousUv.y < 1.0)
    {
        float3 history = gHistory.Sample(gLinearSampler, previousUv).rgb;
        float3 limited = clamp(history, min(clamped, current), max(clamped, current));
        result = lerp(limited, current, 0.08);
    }
    return float4(result, 1.0);
}
)";

inline constexpr const char* kFinalBody = R"(
Texture2D gResult : register(t0);
Texture3D gLut : register(t1);
SamplerState gLinearSampler : register(s1);

float3 acesToneMap(float3 color)
{
    return saturate((color * (2.51 * color + 0.03)) / (color * (2.43 * color + 0.59) + 0.14));
}

float4 main(VSOut input) : SV_Target
{
    float3 color = acesToneMap(gResult.Sample(gLinearSampler, input.uv).rgb);
    color = gLut.Sample(gLinearSampler, saturate(color)).rgb;
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