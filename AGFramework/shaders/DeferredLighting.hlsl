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
	const float3 backgroundColor = gBackgroundColor.rgb;
	float4 albedoSample = gTexture0.Sample(gsamLinearWrap, pin.TexC);
	if (albedoSample.a < 0.001f)
	{
		return float4(backgroundColor, 1.0f);
	}

	float3 normalW = normalize(gTexture1.Sample(gsamLinearWrap, pin.TexC).xyz * 2.0f - 1.0f);
	float3 posW = gTexture2.Sample(gsamLinearWrap, pin.TexC).xyz;
	float4 pbrParams = gTexture3.Sample(gsamLinearWrap, pin.TexC);

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
	const DirectionalShadowProjectionInfo directionalShadowProjection = ProjectIntoDirectionalShadowMap(posW, debugCascadeIndex);

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
		const float shadowDepth = SampleDirectionalShadowMapDepth(posW, debugCascadeIndex);
		const float3 shadowViz = shadowDepth.xxx;
		return float4(lerp(backgroundColor, shadowViz, opacity), 1.0f);
	}

	if (debugViewMode == 7)
	{
		if (!directionalShadowProjection.IsInsideShadowMap)
		{
			const float3 outsideViz = float3(0.85f, 0.15f, 0.2f);
			return float4(lerp(backgroundColor, outsideViz, opacity), 1.0f);
		}

		const float3 frustumViz =
		    float3(directionalShadowProjection.Uv.x, directionalShadowProjection.Uv.y, saturate(directionalShadowProjection.Depth));
		return float4(lerp(backgroundColor, frustumViz, opacity), 1.0f);
	}

	if (debugViewMode == 8)
	{
		return float4(lerp(backgroundColor, pbrParams.x.xxx, opacity), 1.0f);
	}

	if (debugViewMode == 9)
	{
		return float4(lerp(backgroundColor, pbrParams.y.xxx, opacity), 1.0f);
	}

	if (debugViewMode == 10)
	{
		return float4(lerp(backgroundColor, pbrParams.z.xxx, opacity), 1.0f);
	}

	if (debugViewMode == 11)
	{
		const float iblIntensityViz = saturate(pbrParams.w * 0.5f);
		return float4(lerp(backgroundColor, iblIntensityViz.xxx, opacity), 1.0f);
	}

	float3 toEye = normalize(gEyePosW - posW);
	const float roughness = GetPerceptualRoughness(pbrParams);
	const float metallic = GetMetallic(pbrParams);
	const float ambientOcclusion = GetAmbientOcclusion(pbrParams);
	const float iblIntensity = GetIblIntensity(pbrParams);
	const float3 diffuseColor = ComputeDiffuseColor(albedoSample.rgb, metallic);
	const float3 F0 = ComputeMaterialF0(albedoSample.rgb, metallic);
	const float NdotV = saturate(dot(normalW, toEye));
	const float3 F = FresnelSchlickRoughness(NdotV, F0, roughness);
	const float3 kS = F;
	const float3 kD = (1.0f.xxx - kS) * (1.0f - metallic);
	const float3 irradiance = DecodeImageBasedLightingSample(gIrradianceMap.Sample(gsamLinearClamp, normalW));
	const float diffuseIblStrength = max(gImageBasedLightingWeights.x, 0.0f);
	const float specularIblStrength = max(gImageBasedLightingWeights.y, 0.0f);
	const float3 diffuseIBL = irradiance * diffuseColor * diffuseIblStrength;
	const float3 reflectionVector = reflect(-toEye, normalW);
	const float maxReflectionLod = max(gImageBasedLightingSettings.x, 0.0f);
	const float3 prefilteredColor =
	    DecodeImageBasedLightingSample(gPrefilterMap.SampleLevel(gsamLinearClamp, reflectionVector, roughness * maxReflectionLod));
	const float2 brdf = gBrdfLut.Sample(gsamLinearClamp, float2(NdotV, 1.0f - roughness)).rg;
	const float3 specularIBL = prefilteredColor * (F * brdf.x + brdf.y) * iblIntensity * specularIblStrength;
	// Keep the default scene readable even when the imported IBL set is dark or heavily rough.
	const float3 ambientDiffuse = diffuseColor * 0.45f + kD * diffuseIBL;
	const float3 ambientSpecular = specularIBL;
	float3 ambient = (gAmbientLight.rgb * gAmbientLight.w * ambientDiffuse + gAmbientLight.w * ambientSpecular) * ambientOcclusion;
	float3 directionalLighting = 0.0f;
	float3 pointLighting = 0.0f;
	float3 spotLighting = 0.0f;

	[unroll] for (uint lightIndex = 0; lightIndex < DIRECTIONAL_LIGHT_COUNT; ++lightIndex)
	{
		directionalLighting +=
		    directionalShadowFactor * ApplyDirectionalLight(albedoSample.rgb, pbrParams, normalW, toEye, gDirectionalLights[lightIndex]);
	}

	[unroll] for (uint lightIndex = 0; lightIndex < POINT_LIGHT_COUNT; ++lightIndex)
	{
		pointLighting += ApplyPointLight(albedoSample.rgb, pbrParams, normalW, toEye, posW, gPointLights[lightIndex]);
	}

	[unroll] for (uint lightIndex = 0; lightIndex < SPOT_LIGHT_COUNT; ++lightIndex)
	{
		spotLighting += ApplySpotLight(albedoSample.rgb, pbrParams, normalW, toEye, posW, gSpotLights[lightIndex]);
	}

	const float3 litColor = ambient + directionalLighting + pointLighting + spotLighting;
	const float3 finalColor = lerp(backgroundColor, litColor, opacity);
	return float4(finalColor, 1.0f);
}
