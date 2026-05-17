#include "LightingCommon.hlsli"

struct VertexIn
{
    float3 PosL : POSITION;
    float3 NormalL : NORMAL;
    float2 TexC : TEXCOORD;
};

struct GeometryVertexOut
{
    float4 PosH : SV_POSITION;
    float3 PosW : POSITION;
    float3 NormalW : NORMAL;
    float2 TexC : TEXCOORD;
};

struct GBufferOutput
{
    float4 Albedo : SV_Target0;
    float4 Normal : SV_Target1;
    float4 Position : SV_Target2;
    float4 Material : SV_Target3;
};

GeometryVertexOut GeometryVS(VertexIn vin)
{
    GeometryVertexOut vout;

    float4 posW = mul(float4(vin.PosL, 1.0f), gWorld);
    vout.PosW = posW.xyz;
    vout.NormalW = mul(vin.NormalL, (float3x3)gWorldInvTranspose);
    vout.PosH = mul(float4(vin.PosL, 1.0f), gWorldViewProj);
    float4 texCoord = mul(float4(vin.TexC, 0.0f, 1.0f), gTexTransform);
    vout.TexC = texCoord.xy;

    return vout;
}

GBufferOutput GeometryPS(GeometryVertexOut pin)
{
    float4 texColor = gTexture0.Sample(gsamLinearWrap, pin.TexC);
    if (gAuxiliarySettings.x >= 0.0f)
    {
        clip(texColor.a - gAuxiliarySettings.x);
    }

    float3 normalW = normalize(pin.NormalW);
    GBufferOutput output;
    output.Albedo = float4(texColor.rgb * gDiffuseAlbedo.rgb, texColor.a * gDiffuseAlbedo.a);
    output.Normal = float4(normalW * 0.5f + 0.5f, 1.0f);
    output.Position = float4(pin.PosW, 1.0f);
    output.Material = gPbrParams;
    return output;
}
