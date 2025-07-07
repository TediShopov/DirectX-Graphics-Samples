
#include "Common.hlsli"
#include "Lighting.hlsli"

cbuffer surfelColor : register(b1)
{
    float4 color;
}

//Texture2D<float3> texDiffuse		: register(t0);
//Texture2D<float3> texSpecular		: register(t1);
//Texture2D<float3> texNormal			: register(t3);
//Texture2D<float> texSSAO			: register(t12);
//Texture2D<float> texShadow			: register(t13);




struct VSOutput
{
	sample float4 position : SV_Position;
	sample float3 worldPos : WorldPos;
	sample float2 uv : TexCoord0;
	sample float3 viewDir : TexCoord1;
	sample float3 shadowCoord : TexCoord2;
	sample float3 normal : Normal;
//	sample float3 tangent : Tangent;
//	sample float3 bitangent : Bitangent;
};

struct MRT
{
	float3 Color : SV_Target0;
	//float3 Normal : SV_Target1;
};

[RootSignature(Renderer_RootSig)]
MRT main(VSOutput vsOutput) 
{
	MRT mrt;
    mrt.Color = color;

    return mrt;
//
//	//mrt.Normal = vsOutput.normal;
//    //mrt.Color = float4(1, 0, 0, 1);
//    //mrt.Color = AmbientColor;
//    mrt.Color = color;
//	return mrt;
}
