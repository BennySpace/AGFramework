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
