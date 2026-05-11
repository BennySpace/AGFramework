#include "LightingCommon.hlsli"

struct FullscreenVertexOut
{
    float4 PosH : SV_POSITION;
    float2 TexC : TEXCOORD;
};

float3 GetCascadeDebugColor(uint cascadeIndex)
{
    if (cascadeIndex == 0)
    {
        return float3(0.95f, 0.25f, 0.2f);
    }
    if (cascadeIndex == 1)
    {
        return float3(0.2f, 0.8f, 0.3f);
    }
    if (cascadeIndex == 2)
    {
        return float3(0.2f, 0.45f, 0.95f);
    }

    return float3(0.95f, 0.85f, 0.2f);
}

FullscreenVertexOut FullscreenVS(uint vertexId : SV_VertexID)
{
    FullscreenVertexOut vout;

    float2 texCoord = float2((vertexId << 1) & 2, vertexId & 2);
    vout.TexC = texCoord;
    vout.PosH = float4(texCoord * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f), 0.0f, 1.0f);

    return vout;
}

float4 DeferredLightingPS(FullscreenVertexOut pin) : SV_Target
{
    const float3 backgroundColor = float3(0.03f, 0.05f, 0.08f);
    float4 albedoSample = gTexture0.Sample(gsamLinearWrap, pin.TexC);
    if (albedoSample.a < 0.001f)
    {
        return float4(backgroundColor, 1.0f);
    }

    float3 normalW = normalize(gTexture1.Sample(gsamLinearWrap, pin.TexC).xyz * 2.0f - 1.0f);
    float3 posW = gTexture2.Sample(gsamLinearWrap, pin.TexC).xyz;

    const int debugViewMode = (int)round(gAuxiliarySettings.x);
    const float positionVizScale = gAuxiliarySettings.y;
    const float opacity = saturate(albedoSample.a);

    if (debugViewMode == 1)
    {
        return float4(lerp(backgroundColor, albedoSample.rgb, opacity), 1.0f);
    }

    if (debugViewMode == 2)
    {
        const float3 normalViz = normalW * 0.5f + 0.5f;
        return float4(lerp(backgroundColor, normalViz, opacity), 1.0f);
    }

    if (debugViewMode == 3)
    {
        const float3 positionViz = saturate(posW * positionVizScale + 0.5f);
        return float4(lerp(backgroundColor, positionViz, opacity), 1.0f);
    }

    const float3 posV = mul(float4(posW, 1.0f), gView).xyz;
    const float viewDepth = abs(posV.z);
    const uint shadowCascadeIndex = SelectShadowCascade(viewDepth);
    const uint cascadeCount = GetShadowCascadeCount();
    const uint debugCascadeIndex = min((uint)round(gAuxiliarySettings.z), cascadeCount - 1u);
    const float directionalShadowFactor = ComputeDirectionalShadowFactor(posW, normalW, viewDepth, shadowCascadeIndex);
    const float spot0ShadowFactor = ComputeSpotShadowFactor(posW, normalW, 0u);
    const DirectionalShadowProjectionInfo directionalShadowProjection = ProjectIntoDirectionalShadowMap(posW, debugCascadeIndex);
    const SpotShadowProjectionInfo spotShadowProjection = ProjectIntoSpotShadowMap(posW);

    if (debugViewMode == 4)
    {
        const float3 cascadeColor = GetCascadeDebugColor(shadowCascadeIndex);
        return float4(lerp(backgroundColor, cascadeColor, opacity), 1.0f);
    }

    if (debugViewMode == 5)
    {
        const float3 shadowViz = directionalShadowFactor.xxx;
        return float4(lerp(backgroundColor, shadowViz, opacity), 1.0f);
    }

    if (debugViewMode == 6)
    {
        const float3 shadowViz = spot0ShadowFactor.xxx;
        return float4(lerp(backgroundColor, shadowViz, opacity), 1.0f);
    }

    if (debugViewMode == 7)
    {
        if (!spotShadowProjection.IsInsideShadowMap)
        {
            const float3 outsideViz = float3(0.85f, 0.15f, 0.2f);
            return float4(lerp(backgroundColor, outsideViz, opacity), 1.0f);
        }

        const float3 frustumViz = float3(
            spotShadowProjection.Uv.x,
            spotShadowProjection.Uv.y,
            saturate(spotShadowProjection.Depth));
        return float4(lerp(backgroundColor, frustumViz, opacity), 1.0f);
    }

    if (debugViewMode == 8)
    {
        const float shadowDepth = SampleDirectionalShadowMapDepth(posW, debugCascadeIndex);
        const float3 shadowViz = shadowDepth.xxx;
        return float4(lerp(backgroundColor, shadowViz, opacity), 1.0f);
    }

    if (debugViewMode == 9)
    {
        const float shadowDepth = SampleSpotShadowMapDepth(posW);
        const float3 shadowViz = shadowDepth.xxx;
        return float4(lerp(backgroundColor, shadowViz, opacity), 1.0f);
    }

    if (debugViewMode == 10)
    {
        if (!directionalShadowProjection.IsInsideShadowMap)
        {
            const float3 outsideViz = float3(0.85f, 0.15f, 0.2f);
            return float4(lerp(backgroundColor, outsideViz, opacity), 1.0f);
        }

        const float3 frustumViz = float3(
            directionalShadowProjection.Uv.x,
            directionalShadowProjection.Uv.y,
            saturate(directionalShadowProjection.Depth));
        return float4(lerp(backgroundColor, frustumViz, opacity), 1.0f);
    }

    float3 toEye = normalize(gEyePosW - posW);
    float3 ambient = gAmbientLight.rgb * albedoSample.rgb;
    float3 directionalLighting = 0.0f;
    float3 pointLighting = 0.0f;
    float3 spotLighting = 0.0f;

    [unroll]
    for (uint lightIndex = 0; lightIndex < DIRECTIONAL_LIGHT_COUNT; ++lightIndex)
    {
        directionalLighting += directionalShadowFactor * ApplyDirectionalLight(albedoSample.rgb, normalW, toEye, gDirectionalLights[lightIndex]);
    }

    [unroll]
    for (uint lightIndex = 0; lightIndex < POINT_LIGHT_COUNT; ++lightIndex)
    {
        pointLighting += ApplyPointLight(albedoSample.rgb, normalW, toEye, posW, gPointLights[lightIndex]);
    }

    [unroll]
    for (uint lightIndex = 0; lightIndex < SPOT_LIGHT_COUNT; ++lightIndex)
    {
        const float spotShadowFactor = lightIndex == 0u ? spot0ShadowFactor : ComputeSpotShadowFactor(posW, normalW, lightIndex);
        spotLighting += spotShadowFactor * ApplySpotLight(albedoSample.rgb, normalW, toEye, posW, gSpotLights[lightIndex]);
    }

    const float3 litColor = ambient + directionalLighting + pointLighting + spotLighting;
    const float3 finalColor = lerp(backgroundColor, litColor, opacity);
    return float4(finalColor, 1.0f);
}
