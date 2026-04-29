cbuffer ObjectConstants : register(b0)
{
    float4x4 gWorld;
    float4x4 gWorldInvTranspose;
    float4x4 gWorldViewProj;
    float4x4 gTexTransform;
    float3 gEyePosW;
    float gPad0;
    float4 gAmbientLight;
    float4 gLightDir;
    float4 gLightColor;
    float4 gDiffuseAlbedo;
    float4 gSpecularAlbedo;
}

Texture2D gTexture0 : register(t0);
Texture2D gTexture1 : register(t1);
Texture2D gTexture2 : register(t2);
SamplerState gsamLinearWrap : register(s0);

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
};

struct FullscreenVertexOut
{
    float4 PosH : SV_POSITION;
    float2 TexC : TEXCOORD;
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
    float3 normalW = normalize(pin.NormalW);
    GBufferOutput output;
    output.Albedo = float4(texColor.rgb * gDiffuseAlbedo.rgb, texColor.a * gDiffuseAlbedo.a);
    output.Normal = float4(normalW * 0.5f + 0.5f, 1.0f);
    output.Position = float4(pin.PosW, 1.0f);
    return output;
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
    float4 albedoSample = gTexture0.Sample(gsamLinearWrap, pin.TexC);
    if (albedoSample.a < 0.001f)
    {
        return float4(0.03f, 0.05f, 0.08f, 1.0f);
    }

    float3 normalW = normalize(gTexture1.Sample(gsamLinearWrap, pin.TexC).xyz * 2.0f - 1.0f);
    float3 posW = gTexture2.Sample(gsamLinearWrap, pin.TexC).xyz;

    float3 toEye = normalize(gEyePosW - posW);
    float3 lightDir = normalize(-gLightDir.xyz);
    float3 halfVector = normalize(lightDir + toEye);

    float ndotl = saturate(dot(normalW, lightDir));
    float specularPower = gSpecularAlbedo.w;
    float specularFactor = pow(saturate(dot(normalW, halfVector)), specularPower);

    float3 ambient = gAmbientLight.rgb * albedoSample.rgb;
    float3 diffuse = ndotl * gLightColor.rgb * albedoSample.rgb;
    float3 specular = specularFactor * gLightColor.rgb * gSpecularAlbedo.rgb;

    return float4(ambient + diffuse + specular, albedoSample.a);
}
