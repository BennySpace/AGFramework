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
	float4x4 gInvViewProj;
	float4x4 gTexTransform;
	float3 gEyePosW;
	float gPad0;
	float4 gAmbientLight;
	float4 gBackgroundColor;
	float4 gAmbientFloor;
	float4 gDiffuseAlbedo;
	float4 gPbrParams;
	float4 gImageBasedLightingSettings;
	float4 gImageBasedLightingWeights;
	DirectionalLightData gDirectionalLights[DIRECTIONAL_LIGHT_COUNT];
	PointLightData gPointLights[POINT_LIGHT_COUNT];
	SpotLightData gSpotLights[SPOT_LIGHT_COUNT];
	float4x4 gShadowLightViewProj[SHADOW_CASCADE_COUNT];
	float4 gShadowCascadeSplits;
	float4 gShadowMapMetrics;
	float4 gShadowSettings0;
	float4 gShadowSettings1;
	float4 gShadowSettings2;
}

cbuffer AuxiliarySettings : register(b1)
{
	float4 gAuxiliarySettings;
}

Texture2D gTexture0 : register(t0);
Texture2D gTexture1 : register(t1);
Texture2D gTexture2 : register(t2);
Texture2DArray gShadowMap : register(t3);
TextureCube gIrradianceMap : register(t4);
TextureCube gPrefilterMap : register(t5);
TextureCube gEnvironmentMap : register(t6);
Texture2D gBrdfLut : register(t7);
Texture2D gDepthBuffer : register(t8);
SamplerState gsamLinearWrap : register(s0);
SamplerComparisonState gsamShadow : register(s1);
SamplerState gsamLinearClamp : register(s2);

uint GetShadowCascadeCount()
{
	return clamp((uint)round(gShadowSettings0.x), 1u, SHADOW_CASCADE_COUNT);
}

uint SelectShadowCascade(float viewDepth)
{
	const uint cascadeCount = GetShadowCascadeCount();
	uint cascadeIndex = cascadeCount - 1;

	[unroll] for (uint index = 0; index < SHADOW_CASCADE_COUNT; ++index)
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

float SampleDirectionalShadowVisibility(float3 posW, float3 normalW, uint cascadeIndex)
{
	float4 shadowPosH = mul(float4(posW, 1.0f), gShadowLightViewProj[cascadeIndex]);
	shadowPosH.xyz /= max(shadowPosH.w, 0.0001f);

	float2 shadowUv = float2(shadowPosH.x * 0.5f + 0.5f, -shadowPosH.y * 0.5f + 0.5f);

	if (shadowUv.x < 0.0f || shadowUv.x > 1.0f || shadowUv.y < 0.0f || shadowUv.y > 1.0f)
	{
		return 1.0f;
	}

	if (shadowPosH.z <= 0.0f || shadowPosH.z >= 1.0f)
	{
		return 1.0f;
	}

	const float3 lightVector = normalize(-gDirectionalLights[0].Direction.xyz);
	const float normalAlignment = saturate(dot(normalW, lightVector));
	const float2 shadowTexelSize = gShadowMapMetrics.zw;
	const float pcfRadius = max(gShadowSettings0.y, 0.0f);
	const float receiverBias = max(gShadowSettings2.x, gShadowSettings2.y * (1.0f - normalAlignment)) +
	                           max(shadowTexelSize.x, shadowTexelSize.y) * gShadowSettings2.z;
	const float compareDepth = shadowPosH.z - receiverBias;

	float visibility = 0.0f;
	float sampleCount = 0.0f;

	[unroll] for (int offsetY = -1; offsetY <= 1; ++offsetY)
	{
		[unroll] for (int offsetX = -1; offsetX <= 1; ++offsetX)
		{
			const float2 sampleOffset = float2((float)offsetX, (float)offsetY) * shadowTexelSize * pcfRadius;
			visibility += gShadowMap.SampleCmpLevelZero(gsamShadow, float3(shadowUv + sampleOffset, cascadeIndex), compareDepth);
			sampleCount += 1.0f;
		}
	}

	return visibility / max(sampleCount, 1.0f);
}

struct DirectionalShadowProjectionInfo
{
	float2 Uv;
	float Depth;
	bool IsInsideShadowMap;
};

DirectionalShadowProjectionInfo ProjectIntoDirectionalShadowMap(float3 posW, uint cascadeIndex)
{
	DirectionalShadowProjectionInfo info;
	info.Uv = 0.0f.xx;
	info.Depth = 0.0f;
	info.IsInsideShadowMap = false;

	float4 shadowPosH = mul(float4(posW, 1.0f), gShadowLightViewProj[cascadeIndex]);
	shadowPosH.xyz /= max(shadowPosH.w, 0.0001f);

	info.Uv = float2(shadowPosH.x * 0.5f + 0.5f, -shadowPosH.y * 0.5f + 0.5f);
	info.Depth = shadowPosH.z;
	info.IsInsideShadowMap =
	    info.Uv.x >= 0.0f && info.Uv.x <= 1.0f && info.Uv.y >= 0.0f && info.Uv.y <= 1.0f && info.Depth > 0.0f && info.Depth < 1.0f;
	return info;
}

float SampleDirectionalShadowMapDepth(float3 posW, uint cascadeIndex)
{
	DirectionalShadowProjectionInfo projectionInfo = ProjectIntoDirectionalShadowMap(posW, cascadeIndex);
	if (!projectionInfo.IsInsideShadowMap)
	{
		return 1.0f;
	}

	return gShadowMap.SampleLevel(gsamLinearWrap, float3(projectionInfo.Uv, cascadeIndex), 0.0f).r;
}

float ComputeDirectionalShadowFactor(float3 posW, float3 normalW, float viewDepth, uint cascadeIndex)
{
	if (gShadowSettings0.w < 0.5f)
	{
		return 1.0f;
	}

	const uint cascadeCount = GetShadowCascadeCount();
	if (viewDepth > gShadowCascadeSplits[cascadeCount - 1u])
	{
		return 1.0f;
	}

	float visibility = SampleDirectionalShadowVisibility(posW, normalW, cascadeIndex);

	if (cascadeIndex + 1u < cascadeCount)
	{
		const float cascadeNear = cascadeIndex == 0u ? 1.0f : gShadowCascadeSplits[cascadeIndex - 1u];
		const float cascadeFar = gShadowCascadeSplits[cascadeIndex];
		const float cascadeRange = max(cascadeFar - cascadeNear, 0.001f);
		const float blendBand = max(0.5f, cascadeRange * 0.1f);
		const float blendStart = cascadeFar - blendBand;

		if (viewDepth > blendStart)
		{
			const float nextVisibility = SampleDirectionalShadowVisibility(posW, normalW, cascadeIndex + 1u);
			const float blendFactor = saturate((viewDepth - blendStart) / blendBand);
			visibility = lerp(visibility, nextVisibility, blendFactor);
		}
	}

	return lerp(1.0f, visibility, saturate(gShadowSettings0.z));
}

static const float PI = 3.14159265359f;

float GetMetallic(float4 pbrParams)
{
	return saturate(pbrParams.x);
}

float GetPerceptualRoughness(float4 pbrParams)
{
	return clamp(pbrParams.y, 0.04f, 1.0f);
}

float GetAmbientOcclusion(float4 pbrParams)
{
	return saturate(pbrParams.z);
}

float GetIblIntensity(float4 pbrParams)
{
	return max(pbrParams.w, 0.0f);
}

float DistributionGGX(float3 normalW, float3 halfwayVector, float roughness)
{
	const float a = roughness * roughness;
	const float a2 = a * a;
	const float ndoth = saturate(dot(normalW, halfwayVector));
	const float ndoth2 = ndoth * ndoth;
	const float denominator = ndoth2 * (a2 - 1.0f) + 1.0f;
	return a2 / max(PI * denominator * denominator, 0.0001f);
}

float GeometrySchlickGGX(float ndotv, float roughness)
{
	const float r = roughness + 1.0f;
	const float k = (r * r) / 8.0f;
	return ndotv / max(ndotv * (1.0f - k) + k, 0.0001f);
}

float GeometrySmith(float3 normalW, float3 toEye, float3 lightVector, float roughness)
{
	const float ndotv = saturate(dot(normalW, toEye));
	const float ndotl = saturate(dot(normalW, lightVector));
	return GeometrySchlickGGX(ndotv, roughness) * GeometrySchlickGGX(ndotl, roughness);
}

float3 FresnelSchlick(float cosTheta, float3 F0)
{
	return F0 + (1.0f - F0) * pow(1.0f - saturate(cosTheta), 5.0f);
}

float3 ComputeDielectricF0()
{
	return 0.04f.xxx;
}

float3 ComputeMaterialF0(float3 albedo, float metallic)
{
	return lerp(ComputeDielectricF0(), albedo, metallic);
}

float3 ComputeDiffuseColor(float3 albedo, float metallic)
{
	return albedo * (1.0f - metallic);
}

float3 FresnelSchlickRoughness(float cosTheta, float3 F0, float roughness)
{
	const float3 oneMinusRoughness = float3(1.0f - roughness, 1.0f - roughness, 1.0f - roughness);
	return F0 + (max(oneMinusRoughness, F0) - F0) * pow(1.0f - saturate(cosTheta), 5.0f);
}

float3 DecodeImageBasedLightingSample(float4 encodedSample)
{
	return encodedSample.rgb;
}

float3 ComputeCookTorranceLighting(float3 albedo, float4 pbrParams, float3 normalW, float3 toEye, float3 lightVector, float3 radiance)
{
	const float3 halfwayVector = normalize(toEye + lightVector);
	const float roughness = GetPerceptualRoughness(pbrParams);
	const float metallic = GetMetallic(pbrParams);
	const float3 F0 = ComputeMaterialF0(albedo, metallic);
	const float3 F = FresnelSchlick(dot(halfwayVector, toEye), F0);
	const float NDF = DistributionGGX(normalW, halfwayVector, roughness);
	const float G = GeometrySmith(normalW, toEye, lightVector, roughness);
	const float ndotv = saturate(dot(normalW, toEye));
	const float ndotl = saturate(dot(normalW, lightVector));
	const float3 specular = (NDF * G * F) / max(4.0f * ndotv * ndotl, 0.0001f);
	const float3 kS = F;
	const float3 kD = (1.0f.xxx - kS) * (1.0f - metallic);
	const float3 diffuseColor = ComputeDiffuseColor(albedo, metallic);
	return (kD * diffuseColor / PI + specular) * radiance * ndotl;
}

float3 ApplyDirectionalLight(float3 albedo, float4 pbrParams, float3 normalW, float3 toEye, DirectionalLightData lightData)
{
	const float3 lightVector = normalize(-lightData.Direction.xyz);
	return ComputeCookTorranceLighting(albedo, pbrParams, normalW, toEye, lightVector, lightData.Color.rgb);
}

float3 ApplyPointLight(float3 albedo, float4 pbrParams, float3 normalW, float3 toEye, float3 posW, PointLightData lightData)
{
	float3 toLight = lightData.Position.xyz - posW;
	const float distanceToLight = length(toLight);
	const float range = max(lightData.Params.x, 0.001f);
	if (distanceToLight >= range)
	{
		return 0.0f;
	}

	const float3 lightVector = toLight / max(distanceToLight, 0.001f);
	const float attenuation = pow(saturate(1.0f - distanceToLight / range), max(lightData.Params.y, 1.0f));
	return ComputeCookTorranceLighting(albedo, pbrParams, normalW, toEye, lightVector, attenuation * lightData.Color.rgb);
}

float3 ApplySpotLight(float3 albedo, float4 pbrParams, float3 normalW, float3 toEye, float3 posW, SpotLightData lightData)
{
	float3 toLight = lightData.Position.xyz - posW;
	const float distanceToLight = length(toLight);
	const float range = max(lightData.Params.x, 0.001f);
	if (distanceToLight >= range)
	{
		return 0.0f;
	}

	const float3 lightVector = toLight / max(distanceToLight, 0.001f);
	const float3 spotDirection = normalize(-lightData.Direction.xyz);
	const float spotCosine = dot(lightVector, spotDirection);
	const float outerCone = lightData.Params.z;
	const float innerCone = max(lightData.Params.y, outerCone + 0.0001f);
	const float spotFactor = saturate((spotCosine - outerCone) / (innerCone - outerCone));
	if (spotFactor <= 0.0f)
	{
		return 0.0f;
	}

	const float attenuation = pow(saturate(1.0f - distanceToLight / range), max(lightData.Params.w, 1.0f));
	return ComputeCookTorranceLighting(albedo, pbrParams, normalW, toEye, lightVector, attenuation * spotFactor * lightData.Color.rgb);
}
