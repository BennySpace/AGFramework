cbuffer ObjectConstants : register(b0)
{
    float4x4 gWorld;
    float4x4 gWorldInvTranspose;
    float4x4 gWorldViewProj;
    float3 gEyePosW;
    float gPad0;
    float4 gAmbientLight;
    float4 gLightDir;
    float4 gLightColor;
    float4 gDiffuseAlbedo;
    float4 gSpecularAlbedo;
}

Texture2D gDiffuseMap : register(t0);
SamplerState gsamLinearWrap : register(s0);

struct VertexIn
{
    float3 PosL : POSITION;
    float3 NormalL : NORMAL;
    float2 TexC : TEXCOORD;
};

struct VertexOut
{
    float4 PosH : SV_POSITION;
    float3 PosW : POSITION;
    float3 NormalW : NORMAL;
    float2 TexC : TEXCOORD;
};

VertexOut VS(VertexIn vin)
{
    VertexOut vout;

    float4 posW = mul(float4(vin.PosL, 1.0f), gWorld);
    vout.PosW = posW.xyz;
    vout.NormalW = mul(vin.NormalL, (float3x3)gWorldInvTranspose);
    vout.PosH = mul(float4(vin.PosL, 1.0f), gWorldViewProj);
    vout.TexC = vin.TexC;

    return vout;
}

float4 PS(VertexOut pin) : SV_Target
{
    float4 texColor = gDiffuseMap.Sample(gsamLinearWrap, pin.TexC);
    float3 surfaceAlbedo = texColor.rgb * gDiffuseAlbedo.rgb;
    float3 normalW = normalize(pin.NormalW);
    float3 toEye = normalize(gEyePosW - pin.PosW);
    float3 lightDir = normalize(-gLightDir.xyz);
    float3 halfVector = normalize(lightDir + toEye);

    float ndotl = saturate(dot(normalW, lightDir));
    float specularPower = gSpecularAlbedo.w;
    float specularFactor = pow(saturate(dot(normalW, halfVector)), specularPower);

    float3 ambient = gAmbientLight.rgb * surfaceAlbedo;
    float3 diffuse = ndotl * gLightColor.rgb * surfaceAlbedo;
    float3 specular = specularFactor * gLightColor.rgb * gSpecularAlbedo.rgb;

    return float4(ambient + diffuse + specular, texColor.a * gDiffuseAlbedo.a);
}
