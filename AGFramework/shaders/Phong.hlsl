static const uint DIRECTIONAL_LIGHT_COUNT = 1;
static const uint POINT_LIGHT_COUNT = 6;
static const uint SPOT_LIGHT_COUNT = 2;

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
    float4x4 gTexTransform;
    float3 gEyePosW;
    float gPad0;
    float4 gAmbientLight;
    float4 gDiffuseAlbedo;
    float4 gSpecularAlbedo;
    DirectionalLightData gDirectionalLights[DIRECTIONAL_LIGHT_COUNT];
    PointLightData gPointLights[POINT_LIGHT_COUNT];
    SpotLightData gSpotLights[SPOT_LIGHT_COUNT];
}

cbuffer DrawSettings : register(b1)
{
    float gAlphaCutoff;
    float3 gDrawSettingsPadding;
}

float3 ComputeSpecular(float3 normalW, float3 lightVector, float3 toEye, float shininess)
{
    float3 halfVector = normalize(lightVector + toEye);
    float specularFactor = pow(saturate(dot(normalW, halfVector)), shininess);
    return specularFactor * gSpecularAlbedo.rgb;
}

float3 ApplyDirectionalLight(float3 albedo, float3 normalW, float3 toEye, DirectionalLightData lightData)
{
    float3 lightVector = normalize(-lightData.Direction.xyz);
    float ndotl = saturate(dot(normalW, lightVector));
    float3 diffuse = ndotl * lightData.Color.rgb * albedo;
    float3 specular = ComputeSpecular(normalW, lightVector, toEye, gSpecularAlbedo.w) * lightData.Color.rgb;
    return diffuse + specular;
}

float3 ApplyPointLight(float3 albedo, float3 normalW, float3 toEye, float3 posW, PointLightData lightData)
{
    float3 toLight = lightData.Position.xyz - posW;
    float distanceToLight = length(toLight);
    float range = max(lightData.Params.x, 0.001f);
    if (distanceToLight >= range)
    {
        return 0.0f;
    }

    float3 lightVector = toLight / max(distanceToLight, 0.001f);
    float attenuation = pow(saturate(1.0f - distanceToLight / range), max(lightData.Params.y, 1.0f));
    float ndotl = saturate(dot(normalW, lightVector));
    float3 diffuse = ndotl * lightData.Color.rgb * albedo;
    float3 specular = ComputeSpecular(normalW, lightVector, toEye, gSpecularAlbedo.w) * lightData.Color.rgb;
    return attenuation * (diffuse + specular);
}

float3 ApplySpotLight(float3 albedo, float3 normalW, float3 toEye, float3 posW, SpotLightData lightData)
{
    float3 toLight = lightData.Position.xyz - posW;
    float distanceToLight = length(toLight);
    float range = max(lightData.Params.x, 0.001f);
    if (distanceToLight >= range)
    {
        return 0.0f;
    }

    float3 lightVector = toLight / max(distanceToLight, 0.001f);
    float3 spotDirection = normalize(-lightData.Direction.xyz);
    float spotCosine = dot(lightVector, spotDirection);
    float outerCone = lightData.Params.z;
    float innerCone = max(lightData.Params.y, outerCone + 0.0001f);
    float spotFactor = saturate((spotCosine - outerCone) / (innerCone - outerCone));
    if (spotFactor <= 0.0f)
    {
        return 0.0f;
    }

    float attenuation = pow(saturate(1.0f - distanceToLight / range), max(lightData.Params.w, 1.0f));
    float ndotl = saturate(dot(normalW, lightVector));
    float3 diffuse = ndotl * lightData.Color.rgb * albedo;
    float3 specular = ComputeSpecular(normalW, lightVector, toEye, gSpecularAlbedo.w) * lightData.Color.rgb;
    return attenuation * spotFactor * (diffuse + specular);
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
    if (gAlphaCutoff >= 0.0f)
    {
        clip(texColor.a - gAlphaCutoff);
    }

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
    float3 ambient = gAmbientLight.rgb * albedoSample.rgb;
    float3 directionalLighting = 0.0f;
    float3 pointLighting = 0.0f;
    float3 spotLighting = 0.0f;

    [unroll]
    for (uint lightIndex = 0; lightIndex < DIRECTIONAL_LIGHT_COUNT; ++lightIndex)
    {
        directionalLighting += ApplyDirectionalLight(albedoSample.rgb, normalW, toEye, gDirectionalLights[lightIndex]);
    }

    [unroll]
    for (uint lightIndex = 0; lightIndex < POINT_LIGHT_COUNT; ++lightIndex)
    {
        pointLighting += ApplyPointLight(albedoSample.rgb, normalW, toEye, posW, gPointLights[lightIndex]);
    }

    [unroll]
    for (uint lightIndex = 0; lightIndex < SPOT_LIGHT_COUNT; ++lightIndex)
    {
        spotLighting += ApplySpotLight(albedoSample.rgb, normalW, toEye, posW, gSpotLights[lightIndex]);
    }

    return float4(ambient + directionalLighting + pointLighting + spotLighting, albedoSample.a);
}
