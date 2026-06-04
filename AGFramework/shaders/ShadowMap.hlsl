cbuffer ShadowPassConstants : register(b0)
{
	float4x4 gWorldLightViewProj;
	float4x4 gTexTransform;
}

cbuffer AuxiliarySettings : register(b1)
{
	float4 gAuxiliarySettings;
}

cbuffer GeometryInstanceSettings : register(b2)
{
	float4 gDrawPositionOffset;
}

cbuffer ShadowTextureSettings : register(b3)
{
	float4 gShadowTextureFlags;
}

Texture2D gTexture0 : register(t0);
Texture2D gOpacityTexture : register(t1);
SamplerState gsamLinearWrap : register(s0);

float ResolveOpacityMask(float4 opacitySample)
{
	const float rgbMask = max(max(opacitySample.r, opacitySample.g), opacitySample.b);
	return opacitySample.a < 0.999f ? opacitySample.a : rgbMask;
}

struct VertexIn
{
	float3 PosL : POSITION;
	float3 NormalL : NORMAL;
	float2 TexC : TEXCOORD;
};

struct ShadowVertexOut
{
	float4 PosH : SV_POSITION;
	float2 TexC : TEXCOORD;
};

ShadowVertexOut ShadowVS(VertexIn vin)
{
	ShadowVertexOut vout;
	vout.PosH = mul(float4(vin.PosL + gDrawPositionOffset.xyz, 1.0f), gWorldLightViewProj);
	float4 texCoord = mul(float4(vin.TexC, 0.0f, 1.0f), gTexTransform);
	vout.TexC = texCoord.xy;
	return vout;
}

void ShadowAlphaCutoutPS(ShadowVertexOut pin)
{
	float4 texColor = gTexture0.Sample(gsamLinearWrap, pin.TexC);
	float opacity = texColor.a;
	if (gShadowTextureFlags.x > 0.5f)
	{
		opacity *= ResolveOpacityMask(gOpacityTexture.Sample(gsamLinearWrap, pin.TexC));
	}
	if (gAuxiliarySettings.x >= 0.0f)
	{
		clip(opacity - gAuxiliarySettings.x);
	}
}
