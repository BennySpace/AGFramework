static const uint DIRECTIONAL_LIGHT_COUNT = 1;
static const uint POINT_LIGHT_COUNT = 6;
static const uint SPOT_LIGHT_COUNT = 2;
static const uint SHADOW_CASCADE_COUNT = 4;

struct DirectionalLightData
{
    float4 Direction;
    float4 Color;
};

struct PointLightData
{
    float4 Position;
    float4 Color;
    float4 Params;
};

struct SpotLightData
{
    float4 Position;
    float4 Direction;
    float4 Color;
    float4 Params;
};

cbuffer ObjectConstants : register(b0)
{
    float4x4 gWorld;
    float4x4 gWorldInvTranspose;
    float4x4 gWorldViewProj;
    float4x4 gView;
    float4x4 gTexTransform;
    float3 gEyePosW;
    float gPad0;
    float4 gAmbientLight;
    float4 gDiffuseAlbedo;
    float4 gSpecularAlbedo;
    DirectionalLightData gDirectionalLights[DIRECTIONAL_LIGHT_COUNT];
    PointLightData gPointLights[POINT_LIGHT_COUNT];
    SpotLightData gSpotLights[SPOT_LIGHT_COUNT];
    float4x4 gShadowLightViewProj[SHADOW_CASCADE_COUNT];
    float4 gShadowCascadeSplits;
    float4 gShadowMapMetrics;
    float4 gShadowSettings0;
    float4 gShadowSettings1;
}

cbuffer AuxiliarySettings : register(b1)
{
    float4 gAuxiliarySettings;
}

Texture2D gTexture0 : register(t0);
Texture2D gTexture1 : register(t1);
Texture2D gTexture2 : register(t2);
Texture2DArray gShadowMap : register(t3);
SamplerState gsamLinearWrap : register(s0);
SamplerComparisonState gsamShadow : register(s1);

uint GetShadowCascadeCount()
{
    return clamp((uint)round(gShadowSettings0.x), 1u, SHADOW_CASCADE_COUNT);
}

uint SelectShadowCascade(float viewDepth)
{
    const uint cascadeCount = GetShadowCascadeCount();
    uint cascadeIndex = cascadeCount - 1;

    [unroll]
    for (uint index = 0; index < SHADOW_CASCADE_COUNT; ++index)
    {
        if (index >= cascadeCount)
        {
            break;
        }

        if (viewDepth <= gShadowCascadeSplits[index])
        {
            cascadeIndex = index;
            break;
        }
    }

    return cascadeIndex;
}

float ComputeDirectionalShadowFactor(float3 posW, uint cascadeIndex)
{
    if (gShadowSettings0.w < 0.5f)
    {
        return 1.0f;
    }

    float4 shadowPosH = mul(float4(posW, 1.0f), gShadowLightViewProj[cascadeIndex]);
    shadowPosH.xyz /= max(shadowPosH.w, 0.0001f);

    float2 shadowUv = float2(
        shadowPosH.x * 0.5f + 0.5f,
        -shadowPosH.y * 0.5f + 0.5f);

    if (shadowUv.x < 0.0f || shadowUv.x > 1.0f || shadowUv.y < 0.0f || shadowUv.y > 1.0f)
    {
        return 1.0f;
    }

    if (shadowPosH.z <= 0.0f || shadowPosH.z >= 1.0f)
    {
        return 1.0f;
    }

    const float2 shadowTexelSize = gShadowMapMetrics.zw;
    const float pcfRadius = max(gShadowSettings0.y, 0.0f);

    float visibility = 0.0f;
    float sampleCount = 0.0f;

    [unroll]
    for (int offsetY = -1; offsetY <= 1; ++offsetY)
    {
        [unroll]
        for (int offsetX = -1; offsetX <= 1; ++offsetX)
        {
            const float2 sampleOffset = float2((float)offsetX, (float)offsetY) * shadowTexelSize * pcfRadius;
            visibility += gShadowMap.SampleCmpLevelZero(
                gsamShadow,
                float3(shadowUv + sampleOffset, cascadeIndex),
                shadowPosH.z);
            sampleCount += 1.0f;
        }
    }

    visibility /= max(sampleCount, 1.0f);

    return lerp(1.0f, visibility, saturate(gShadowSettings0.z));
}

float3 ComputeSpecular(float3 normalW, float3 lightVector, float3 toEye, float shininess)
{
    float3 halfVector = normalize(lightVector + toEye);
    float specularFactor = pow(saturate(dot(normalW, halfVector)), shininess);
    return specularFactor * gSpecularAlbedo.rgb;
}

float3 ApplyDirectionalLight(float3 albedo, float3 normalW, float3 toEye, DirectionalLightData lightData)
{
    float3 lightVector = normalize(-lightData.Direction.xyz);
    float ndotl = saturate(dot(normalW, lightVector));
    float3 diffuse = ndotl * lightData.Color.rgb * albedo;
    float3 specular = ComputeSpecular(normalW, lightVector, toEye, gSpecularAlbedo.w) * lightData.Color.rgb;
    return diffuse + specular;
}

float3 ApplyPointLight(float3 albedo, float3 normalW, float3 toEye, float3 posW, PointLightData lightData)
{
    float3 toLight = lightData.Position.xyz - posW;
    float distanceToLight = length(toLight);
    float range = max(lightData.Params.x, 0.001f);
    if (distanceToLight >= range)
    {
        return 0.0f;
    }

    float3 lightVector = toLight / max(distanceToLight, 0.001f);
    float attenuation = pow(saturate(1.0f - distanceToLight / range), max(lightData.Params.y, 1.0f));
    float ndotl = saturate(dot(normalW, lightVector));
    float3 diffuse = ndotl * lightData.Color.rgb * albedo;
    float3 specular = ComputeSpecular(normalW, lightVector, toEye, gSpecularAlbedo.w) * lightData.Color.rgb;
    return attenuation * (diffuse + specular);
}

float3 ApplySpotLight(float3 albedo, float3 normalW, float3 toEye, float3 posW, SpotLightData lightData)
{
    float3 toLight = lightData.Position.xyz - posW;
    float distanceToLight = length(toLight);
    float range = max(lightData.Params.x, 0.001f);
    if (distanceToLight >= range)
    {
        return 0.0f;
    }

    float3 lightVector = toLight / max(distanceToLight, 0.001f);
    float3 spotDirection = normalize(-lightData.Direction.xyz);
    float spotCosine = dot(lightVector, spotDirection);
    float outerCone = lightData.Params.z;
    float innerCone = max(lightData.Params.y, outerCone + 0.0001f);
    float spotFactor = saturate((spotCosine - outerCone) / (innerCone - outerCone));
    if (spotFactor <= 0.0f)
    {
        return 0.0f;
    }

    float attenuation = pow(saturate(1.0f - distanceToLight / range), max(lightData.Params.w, 1.0f));
    float ndotl = saturate(dot(normalW, lightVector));
    float3 diffuse = ndotl * lightData.Color.rgb * albedo;
    float3 specular = ComputeSpecular(normalW, lightVector, toEye, gSpecularAlbedo.w) * lightData.Color.rgb;
    return attenuation * spotFactor * (diffuse + specular);
}
