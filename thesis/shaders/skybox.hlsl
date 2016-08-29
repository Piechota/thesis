struct VSInput
{
    float3 position : POSITION;
    float2 uv : TEXCOORD;
};
struct PSInput
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD;
};

SamplerState Sampler : register(s0);

#ifdef VERTEX_S
cbuffer frameBuffer : register(b0)
{
	float4x4 WorldToProject;
}
PSInput vsMain(VSInput input)
{
    PSInput result;
   
    result.position = mul( WorldToProject, float4(input.position, 0.f));
    result.uv = input.uv;

    return result;
}
#endif

#ifdef PIXEL_S
Texture2D DiffTex : register(t0);
float4 psMain(PSInput input) : SV_TARGET
{
    float4 result;

    result = DiffTex.Sample( Sampler, input.uv);

    return result;
}
#endif