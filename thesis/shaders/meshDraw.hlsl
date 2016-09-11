cbuffer objectBuffer : register(b0)
{
	float4x4 WorldToProject;
	float4x4 ObjectToWorld;
	float3 LightDir;
	float3 LightColor;
	float LightIntensity;
	float3 AmbientColor;
};
struct VSInput
{
    float3 position : POSITION;
    float3 normal : NORMAL;
    float3 tan : TANGENT;
    float2 uv : TEXCOORD;
};
struct PSInput
{
    float3x3 tbn : TANGENT;
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD1;
};

Texture2D DiffTex : register(t0);
Texture2D NormTex : register(t1);
SamplerState Sampler : register(s0);

PSInput vsMain(VSInput input)
{
    PSInput result;
    float3 bit = cross(input.tan, input.normal);
	result.tbn[0] = input.tan;
    result.tbn[1] = bit;
    result.tbn[2] = input.normal;
    result.position = mul(WorldToProject, float4(input.position, 1.f));
	result.uv = input.uv;

    return result;
}

float4 psMain(PSInput input) : SV_TARGET
{
	float4 result = DiffTex.Sample(Sampler, input.uv);
    float3 normal = NormTex.Sample( Sampler, input.uv).xyz;
    normal = mad(normal, 2.f, -1.f);
    normal = mul(normal, input.tbn);
    normal = normalize(normal);

	result.xyz *= saturate(dot(normal, LightDir)) * LightColor * LightIntensity;
	result.xyz += AmbientColor;

    return result;
}