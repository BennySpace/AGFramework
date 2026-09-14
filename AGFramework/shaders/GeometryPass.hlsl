#include "LightingCommon.hlsli"

cbuffer GeometryInstanceSettings : register(b2)
{
	float4 gDrawPositionOffset;
}

cbuffer GeometryMaterialSettings : register(b3)
{
	float4 gDrawDiffuseAlbedo;
	float4 gDrawPbrParams;
}

cbuffer GeometryTextureSettings : register(b4)
{
	float4 gTextureFlags;
}

Texture2D gOpacityTexture : register(t9);

float ResolveOpacityMask(float4 opacitySample)
{
	const float rgbMask = max(max(opacitySample.r, opacitySample.g), opacitySample.b);
	return opacitySample.a < 0.999f ? opacitySample.a : rgbMask;
}

struct VertexIn
{
	float3 PosL : POSITION;
	float3 NormalL : NORMAL;
	float3 TangentL : TANGENT;
	float2 TexC : TEXCOORD;
};

struct GeometryVertexOut
{
	float4 PosH : SV_POSITION;
	float3 PosW : POSITION;
	float3 NormalW : NORMAL;
	float3 TangentW : TANGENT;
	float2 TexC : TEXCOORD;
};

struct GBufferOutput
{
	float4 Albedo : SV_Target0;
	float4 Normal : SV_Target1;
	float4 Material : SV_Target2;
};

GeometryVertexOut GeometryVS(VertexIn vin)
{
	GeometryVertexOut vout;

	const float3 positionL = vin.PosL + gDrawPositionOffset.xyz;
	float4 posW = mul(float4(positionL, 1.0f), gWorld);
	vout.PosW = posW.xyz;
	vout.NormalW = mul(vin.NormalL, (float3x3)gWorldInvTranspose);
	vout.TangentW = mul(vin.TangentL, (float3x3)gWorld);
	vout.PosH = mul(float4(positionL, 1.0f), gWorldViewProj);
	float4 texCoord = mul(float4(vin.TexC, 0.0f, 1.0f), gTexTransform);
	vout.TexC = texCoord.xy;

	return vout;
}

GeometryVertexOut TransparentVS(VertexIn vin)
{
	return GeometryVS(vin);
}

GBufferOutput GeometryPS(GeometryVertexOut pin)
{
	float4 texColor = gTexture0.Sample(gsamLinearWrap, pin.TexC);
	float opacity = texColor.a * gDrawDiffuseAlbedo.a;
	if (gTextureFlags.z > 0.5f)
	{
		opacity *= ResolveOpacityMask(gOpacityTexture.Sample(gsamLinearWrap, pin.TexC));
	}
	if (gAuxiliarySettings.x >= 0.0f)
	{
		clip(opacity - gAuxiliarySettings.x);
	}

	float3 normalW = normalize(pin.NormalW);
	if (gTextureFlags.x > 0.5f)
	{
		float3 tangentW = normalize(pin.TangentW - normalW * dot(pin.TangentW, normalW));
		float3 bitangentW = normalize(cross(normalW, tangentW));
		float3 normalT = gTexture1.Sample(gsamLinearWrap, pin.TexC).xyz * 2.0f - 1.0f;
		normalW = normalize(normalT.x * tangentW + normalT.y * bitangentW + normalT.z * normalW);
	}

	const float metallicOverride = saturate(gPbrParams.x);
	const float roughnessScale = max(gPbrParams.y, 0.04f) / 0.5f;
	const float ambientOcclusionScale = saturate(gPbrParams.z);
	const float iblIntensityScale = max(gPbrParams.w, 0.0f);
	float4 materialParams = gDrawPbrParams;
	if (gTextureFlags.y > 0.5f)
	{
		const float3 orm = gTexture2.Sample(gsamLinearWrap, pin.TexC).rgb;
		materialParams.x = saturate(orm.b);
		materialParams.y = clamp(orm.g, 0.04f, 1.0f);
		materialParams.z = saturate(orm.r);
	}
	materialParams.x = max(materialParams.x, metallicOverride);
	materialParams.y = clamp(materialParams.y * roughnessScale, 0.04f, 1.0f);
	materialParams.z = saturate(materialParams.z * ambientOcclusionScale);
	materialParams.w = max(materialParams.w * iblIntensityScale, 0.0f);

	GBufferOutput output;
	output.Albedo = float4(texColor.rgb * gDiffuseAlbedo.rgb * gDrawDiffuseAlbedo.rgb, opacity * gDiffuseAlbedo.a);
	output.Normal = float4(normalW * 0.5f + 0.5f, (gTextureFlags.x > 0.5f ? 1.0f : 0.0f) + (gTextureFlags.y > 0.5f ? 2.0f : 0.0f));
	output.Material = materialParams;
	return output;
}

struct ForwardSurface
{
	float3 Albedo;
	float3 NormalW;
	float4 PbrParams;
	float Opacity;
};

ForwardSurface ResolveForwardSurface(GeometryVertexOut pin)
{
	ForwardSurface surface;
	const float4 texColor = gTexture0.Sample(gsamLinearWrap, pin.TexC);
	surface.Opacity = saturate(texColor.a * gDrawDiffuseAlbedo.a);
	if (gTextureFlags.z > 0.5f)
	{
		surface.Opacity *= ResolveOpacityMask(gOpacityTexture.Sample(gsamLinearWrap, pin.TexC));
	}
	clip(surface.Opacity - 0.001f);

	surface.NormalW = normalize(pin.NormalW);
	if (gTextureFlags.x > 0.5f)
	{
		const float3 tangentW = normalize(pin.TangentW - surface.NormalW * dot(pin.TangentW, surface.NormalW));
		const float3 bitangentW = normalize(cross(surface.NormalW, tangentW));
		const float3 normalT = gTexture1.Sample(gsamLinearWrap, pin.TexC).xyz * 2.0f - 1.0f;
		surface.NormalW = normalize(normalT.x * tangentW + normalT.y * bitangentW + normalT.z * surface.NormalW);
	}

	const float metallicOverride = saturate(gPbrParams.x);
	const float roughnessScale = max(gPbrParams.y, 0.04f) / 0.5f;
	const float ambientOcclusionScale = saturate(gPbrParams.z);
	const float iblIntensityScale = max(gPbrParams.w, 0.0f);
	surface.PbrParams = gDrawPbrParams;
	if (gTextureFlags.y > 0.5f)
	{
		const float3 orm = gTexture2.Sample(gsamLinearWrap, pin.TexC).rgb;
		surface.PbrParams.x = saturate(orm.b);
		surface.PbrParams.y = clamp(orm.g, 0.04f, 1.0f);
		surface.PbrParams.z = saturate(orm.r);
	}
	surface.PbrParams.x = max(surface.PbrParams.x, metallicOverride);
	surface.PbrParams.y = clamp(surface.PbrParams.y * roughnessScale, 0.04f, 1.0f);
	surface.PbrParams.z = saturate(surface.PbrParams.z * ambientOcclusionScale);
	surface.PbrParams.w = max(surface.PbrParams.w * iblIntensityScale, 0.0f);
	surface.Albedo = texColor.rgb * gDiffuseAlbedo.rgb * gDrawDiffuseAlbedo.rgb;
	return surface;
}

float3 ComputeForwardPbrLighting(ForwardSurface surface, float3 posW)
{
	const float3 toEye = normalize(gEyePosW - posW);
	const float3 posV = mul(float4(posW, 1.0f), gView).xyz;
	const float viewDepth = abs(posV.z);
	const uint shadowCascadeIndex = SelectShadowCascade(viewDepth);
	const float directionalShadowFactor = ComputeDirectionalShadowFactor(posW, surface.NormalW, viewDepth, shadowCascadeIndex);
	const float roughness = GetPerceptualRoughness(surface.PbrParams);
	const float metallic = GetMetallic(surface.PbrParams);
	const float ambientOcclusion = GetAmbientOcclusion(surface.PbrParams);
	const float iblIntensity = GetIblIntensity(surface.PbrParams);
	const float3 F0 = ComputeMaterialF0(surface.Albedo, metallic);
	const float NdotV = saturate(dot(surface.NormalW, toEye));
	const float3 F = FresnelSchlickRoughness(NdotV, F0, roughness);
	const float3 kD = (1.0f.xxx - F) * (1.0f - metallic);
	const float3 irradiance = DecodeImageBasedLightingSample(gIrradianceMap.Sample(gsamLinearClamp, surface.NormalW));
	const float3 diffuseIbl = irradiance * surface.Albedo * max(gImageBasedLightingWeights.x, 0.0f);
	const float3 reflectionVector = reflect(-toEye, surface.NormalW);
	const float3 prefilteredColor = DecodeImageBasedLightingSample(
	    gPrefilterMap.SampleLevel(gsamLinearClamp, reflectionVector, roughness * max(gImageBasedLightingSettings.x, 0.0f)));
	const float2 brdf = gBrdfLut.Sample(gsamLinearClamp, float2(NdotV, roughness)).rg;
	const float3 specularIbl = prefilteredColor * (F * brdf.x + brdf.y) * iblIntensity * max(gImageBasedLightingWeights.y, 0.0f);
	const float3 ambientFloor = gAmbientFloor.rgb * max(gAmbientFloor.w, 0.0f) * surface.Albedo * (1.0f - metallic);
	float3 ambient = (gAmbientLight.rgb * gAmbientLight.w * (kD * diffuseIbl) + gAmbientLight.w * specularIbl + ambientFloor) *
	                 ambientOcclusion;
	float3 directLighting = 0.0f;
	[unroll] for (uint lightIndex = 0; lightIndex < DIRECTIONAL_LIGHT_COUNT; ++lightIndex)
	{
		directLighting += directionalShadowFactor *
		                  ApplyDirectionalLight(surface.Albedo, surface.PbrParams, surface.NormalW, toEye, gDirectionalLights[lightIndex]);
	}
	[unroll] for (uint lightIndex = 0; lightIndex < POINT_LIGHT_COUNT; ++lightIndex)
	{
		directLighting += ApplyPointLight(surface.Albedo, surface.PbrParams, surface.NormalW, toEye, posW, gPointLights[lightIndex]);
	}
	[unroll] for (uint lightIndex = 0; lightIndex < SPOT_LIGHT_COUNT; ++lightIndex)
	{
		directLighting += ApplySpotLight(surface.Albedo, surface.PbrParams, surface.NormalW, toEye, posW, gSpotLights[lightIndex]);
	}
	return ambient + directLighting;
}

float3 ComputeForwardPhongLighting(ForwardSurface surface, float3 posW)
{
	const float3 toEye = normalize(gEyePosW - posW);
	const float3 posV = mul(float4(posW, 1.0f), gView).xyz;
	const float viewDepth = abs(posV.z);
	const uint shadowCascadeIndex = SelectShadowCascade(viewDepth);
	const float directionalShadowFactor = ComputeDirectionalShadowFactor(posW, surface.NormalW, viewDepth, shadowCascadeIndex);
	const float roughness = GetPerceptualRoughness(surface.PbrParams);
	const float metallic = GetMetallic(surface.PbrParams);
	const float ambientOcclusion = GetAmbientOcclusion(surface.PbrParams);
	float3 directLighting = 0.0f;
	[unroll] for (uint lightIndex = 0; lightIndex < DIRECTIONAL_LIGHT_COUNT; ++lightIndex)
	{
		directLighting += directionalShadowFactor *
		                  ApplyPhongDirectionalLight(surface.Albedo, surface.NormalW, toEye, roughness, metallic, gDirectionalLights[lightIndex]);
	}
	[unroll] for (uint lightIndex = 0; lightIndex < POINT_LIGHT_COUNT; ++lightIndex)
	{
		directLighting += ApplyPhongPointLight(surface.Albedo, surface.NormalW, toEye, posW, roughness, metallic, gPointLights[lightIndex]);
	}
	[unroll] for (uint lightIndex = 0; lightIndex < SPOT_LIGHT_COUNT; ++lightIndex)
	{
		directLighting += ApplyPhongSpotLight(surface.Albedo, surface.NormalW, toEye, posW, roughness, metallic, gSpotLights[lightIndex]);
	}
	const float3 ambient =
	    (gAmbientLight.rgb * gAmbientLight.w * surface.Albedo + gAmbientFloor.rgb * gAmbientFloor.w * surface.Albedo) * ambientOcclusion;
	return ambient + directLighting;
}

float4 TransparentPS(GeometryVertexOut pin) : SV_Target
{
	const ForwardSurface surface = ResolveForwardSurface(pin);
	return float4(ApplyOutputTransform(ComputeForwardPbrLighting(surface, pin.PosW)), surface.Opacity);
}

float4 TransparentPhongPS(GeometryVertexOut pin) : SV_Target
{
	const ForwardSurface surface = ResolveForwardSurface(pin);
	return float4(ApplyOutputTransform(ComputeForwardPhongLighting(surface, pin.PosW)), surface.Opacity);
}
