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

float3 ComputeSkyboxDirection(float2 texCoord)
{
	const float2 ndc = float2(texCoord.x * 2.0f - 1.0f, 1.0f - texCoord.y * 2.0f);
	const float4 worldPosition = mul(float4(ndc, 1.0f, 1.0f), gInvViewProj);
	return normalize(worldPosition.xyz / max(worldPosition.w, 0.0001f) - gEyePosW);
}

float3 SampleSkybox(float2 texCoord)
{
	if (gImageBasedLightingSettings.w <= 0.5f)
	{
		return gBackgroundColor.rgb;
	}

	const float3 direction = ComputeSkyboxDirection(texCoord);
	const float skyboxIntensity = gImageBasedLightingWeights.z;
	return DecodeImageBasedLightingSample(gEnvironmentMap.SampleLevel(gsamLinearClamp, direction, 0.0f)) * skyboxIntensity;
}

float2 ComputeTexCoordFromPixel(int2 pixelCoord)
{
	uint width = 1;
	uint height = 1;
	gDepthBuffer.GetDimensions(width, height);
	return (float2(pixelCoord) + float2(0.5f, 0.5f)) / float2(max(width, 1u), max(height, 1u));
}

float3 ReconstructWorldPosition(int2 pixelCoord, float depth)
{
	const float2 texCoord = ComputeTexCoordFromPixel(pixelCoord);
	const float2 ndc = float2(texCoord.x * 2.0f - 1.0f, 1.0f - texCoord.y * 2.0f);
	const float4 worldPosition = mul(float4(ndc, depth, 1.0f), gInvViewProj);
	return worldPosition.xyz / max(worldPosition.w, 0.0001f);
}

float3 ApplyOutputTransform(float3 color)
{
	const float exposure = max(gImageBasedLightingWeights.w, 0.001f);
	const float3 toneMapped = 1.0f.xxx - exp(-max(color, 0.0f.xxx) * exposure);
	return pow(saturate(toneMapped), 1.0f / 2.2f);
}

float4 DeferredLightingPS(FullscreenVertexOut pin) : SV_Target
{
	const float3 backgroundColor = gBackgroundColor.rgb;
	const int2 pixelCoord = int2(pin.PosH.xy);
	float4 albedoSample = gTexture0.Load(int3(pixelCoord, 0));
	if (albedoSample.a < 0.001f)
	{
		return float4(ApplyOutputTransform(SampleSkybox(pin.TexC)), 1.0f);
	}

	float4 normalSample = gTexture1.Load(int3(pixelCoord, 0));
	const float depthSample = gDepthBuffer.Load(int3(pixelCoord, 0)).r;
	float3 normalW = normalize(normalSample.xyz * 2.0f - 1.0f);
	float3 posW = ReconstructWorldPosition(pixelCoord, depthSample);
	float4 pbrParams = gTexture2.Load(int3(pixelCoord, 0));

	const int debugViewMode = (int)round(gAuxiliarySettings.x);
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

	const float3 posV = mul(float4(posW, 1.0f), gView).xyz;
	const float viewDepth = abs(posV.z);
	const uint shadowCascadeIndex = SelectShadowCascade(viewDepth);
	const float directionalShadowFactor = ComputeDirectionalShadowFactor(posW, normalW, viewDepth, shadowCascadeIndex);

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

	float3 toEye = normalize(gEyePosW - posW);
	const float roughness = GetPerceptualRoughness(pbrParams);
	const float metallic = GetMetallic(pbrParams);
	const float ambientOcclusion = GetAmbientOcclusion(pbrParams);
	const float iblIntensity = GetIblIntensity(pbrParams);
	const float3 F0 = ComputeMaterialF0(albedoSample.rgb, metallic);
	const float NdotV = saturate(dot(normalW, toEye));
	const float3 F = FresnelSchlickRoughness(NdotV, F0, roughness);
	const float3 kS = F;
	const float3 kD = (1.0f.xxx - kS) * (1.0f - metallic);
	const float3 irradiance = DecodeImageBasedLightingSample(gIrradianceMap.Sample(gsamLinearClamp, normalW));
	const float diffuseIblStrength = max(gImageBasedLightingWeights.x, 0.0f);
	const float specularIblStrength = max(gImageBasedLightingWeights.y, 0.0f);
	const float3 diffuseIBL = irradiance * albedoSample.rgb * diffuseIblStrength;
	const float3 reflectionVector = reflect(-toEye, normalW);
	const float maxReflectionLod = max(gImageBasedLightingSettings.x, 0.0f);
	const float3 prefilteredColor =
	    DecodeImageBasedLightingSample(gPrefilterMap.SampleLevel(gsamLinearClamp, reflectionVector, roughness * maxReflectionLod));
	const float2 brdf = gBrdfLut.Sample(gsamLinearClamp, float2(NdotV, roughness)).rg;
	const float3 specularIBL = prefilteredColor * (F * brdf.x + brdf.y) * iblIntensity * specularIblStrength;
	const float3 ambientDiffuse = kD * diffuseIBL;
	const float3 ambientSpecular = specularIBL;
	const float3 ambientFloor = gAmbientFloor.rgb * max(gAmbientFloor.w, 0.0f) * albedoSample.rgb * (1.0f - metallic);
	float3 ambient = (gAmbientLight.rgb * gAmbientLight.w * ambientDiffuse + gAmbientLight.w * ambientSpecular + ambientFloor) *
	                 ambientOcclusion;
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

	const float3 directLighting = directionalLighting + pointLighting + spotLighting;

	if (debugViewMode == 12)
	{
		return float4(lerp(backgroundColor, directLighting, opacity), 1.0f);
	}

	if (debugViewMode == 13)
	{
		return float4(lerp(backgroundColor, ambient, opacity), 1.0f);
	}

	const float3 litColor = ambient + directLighting;
	const float3 finalColor = lerp(backgroundColor, litColor, opacity);
	return float4(ApplyOutputTransform(finalColor), 1.0f);
}
