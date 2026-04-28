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

struct VertexIn
{
    float3 PosL : POSITION;
    float3 NormalL : NORMAL;
};

struct VertexOut
{
    float4 PosH : SV_POSITION;
    float3 PosW : POSITION;
    float3 NormalW : NORMAL;
};

VertexOut VS(VertexIn vin)
{
    VertexOut vout;

    float4 posW = mul(float4(vin.PosL, 1.0f), gWorld);
    vout.PosW = posW.xyz;
    vout.NormalW = mul(vin.NormalL, (float3x3)gWorldInvTranspose);
    vout.PosH = mul(float4(vin.PosL, 1.0f), gWorldViewProj);

    return vout;
}

float4 PS(VertexOut pin) : SV_Target
{
    float3 normalW = normalize(pin.NormalW);
    float3 toEye = normalize(gEyePosW - pin.PosW);
    float3 lightDir = normalize(-gLightDir.xyz);
    float3 halfVector = normalize(lightDir + toEye);

    float ndotl = saturate(dot(normalW, lightDir));
    float specularPower = gSpecularAlbedo.w;
    float specularFactor = pow(saturate(dot(normalW, halfVector)), specularPower);

    float3 ambient = gAmbientLight.rgb * gDiffuseAlbedo.rgb;
    float3 diffuse = ndotl * gLightColor.rgb * gDiffuseAlbedo.rgb;
    float3 specular = specularFactor * gLightColor.rgb * gSpecularAlbedo.rgb;

    return float4(ambient + diffuse + specular, gDiffuseAlbedo.a);
}
