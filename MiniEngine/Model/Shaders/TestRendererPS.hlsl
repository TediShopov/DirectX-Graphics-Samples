#include "Lighting.hlsli"
#include "Common.hlsli"
////[RootSignature(Renderer_RootSig)]
//float4 main() : SV_Target
//{
//    return float4(1, 0, 1, 1); // Magenta
//}


Texture2D<float3> texDiffuse		: register(t0); Texture2D<float3> texSpecular		: register(t1);
Texture2D<float3> texNormal			: register(t3);
Texture2D<float> texSSAO			: register(t12);
Texture2D<float> texShadow			: register(t13);
Texture2D indirrectDiffuseIrradianceSBGI: register(t18);
Texture2D indirrectDiffuseIrradianceHBIL: register(t19);


struct VSOutput
{
	sample float4 position : SV_Position;
	sample float3 worldPos : WorldPos;
	sample float2 uv : TexCoord0;
	sample float3 viewDir : TexCoord1;
	sample float3 shadowCoord : TexCoord2;
	sample float3 normal : Normal;
	sample float3 tangent : Tangent;
	sample float3 bitangent : Bitangent;
};

struct MRT
{
	float3 Color : SV_Target0;
	float3 Normal : SV_Target1;
};

static const float M_PI = 3.14159265359;

[RootSignature(Renderer_RootSig)]
MRT main(VSOutput vsOutput)
{

	MRT mrt;
    if (	vsOutput.tangent.x == 0 && vsOutput.tangent.y==0	 && vsOutput.tangent.z==0)
    {
        mrt.Color = float4(0, 1, 0,1);
        mrt.Normal = float4(0, 1, 0,1);
        return mrt;
    }

//    vsOutput.normal = float3(0, -1, 0);
//    vsOutput.bitangent = float3(1, 0, 0);
//    vsOutput.tangent = float3(0, 0, 1);

        uint2 pixelPos = uint2(vsOutput.position.xy);
# define SAMPLE_TEX(texName) texName.Sample(defaultSampler, vsOutput.uv)

    float3 diffuseAlbedo = SAMPLE_TEX(texDiffuse);

    float3 colorSum = 0;
    {
        float ao = texSSAO[pixelPos];
        colorSum += ApplyAmbientLight( diffuseAlbedo, ao, AmbientColor );
    }


    float gloss = 128.0;
    float3 normal;
    {
        normal = SAMPLE_TEX(texNormal) * 2.0 - 1.0;
        AntiAliasSpecular(normal, gloss);
        float3x3 tbn = float3x3(normalize(vsOutput.tangent), normalize(vsOutput.bitangent), normalize(vsOutput.normal));
        normal = normalize(mul(normal, tbn));
    }

    float3 specularAlbedo = float3( 0.56, 0.56, 0.56 );
    float specularMask = SAMPLE_TEX(texSpecular).r;
    float3 viewDir = normalize(vsOutput.viewDir);

    colorSum += ApplyDirectionalLight( diffuseAlbedo, specularAlbedo, specularMask, gloss, normal, viewDir, SunDirection, SunColor, vsOutput.shadowCoord, texShadow );

	ShadeLights(colorSum, pixelPos,
		diffuseAlbedo,
		specularAlbedo,
		specularMask,
		gloss,
		normal,
		viewDir,
		vsOutput.worldPos
		);

	mrt.Normal = normal;

    float2 screenSpaceUV = vsOutput.position.xy / float2(1920, 1080);

    //The confidence weighing will blend between the SBGI indirect diffuse and the HBIL indirect diffuse 
    //The blend depens on per-pixel confidence defined by the SSAO
    if(BlendMode > 0)
    {
        float4 SBGI = indirrectDiffuseIrradianceSBGI[pixelPos];
        float4 HBIL = indirrectDiffuseIrradianceHBIL[pixelPos];

        //AO is bigger than threshold world position is not suffuciently occluded to use HBIL
        float confidence = step(texSSAO[pixelPos],AOThreshold);
        //float confidence = saturate(texSSAO[pixelPos] * (1 / AOThreshold));
        //float confidence = saturate(texSSAO[pixelPos] - AOThreshold);
        float4 resultIrradiance = lerp(SBGI, SBGI + HBIL, confidence);

        colorSum    += (diffuseAlbedo * resultIrradiance) / M_PI;
    }
    else
    {
        float4 resultIrradiance = indirrectDiffuseIrradianceSBGI[pixelPos];
        colorSum    += (diffuseAlbedo * resultIrradiance) / M_PI;
        
    }
    
	mrt.Color = colorSum;
	return mrt;
}
