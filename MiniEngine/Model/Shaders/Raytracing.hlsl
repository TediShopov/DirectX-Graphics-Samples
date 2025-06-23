//*********************************************************
//
// Copyright (c) Microsoft. All rights reserved.
// This code is licensed under the MIT License (MIT).
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
//*********************************************************

#ifndef RAYTRACING_HLSL
#define RAYTRACING_HLSL

#include "RaytracingHlslCompat.h"
#include "SurfelASAsserts.hlsli"


RaytracingAccelerationStructure Scene : register(t0, space0);
Texture2D<float4> diffuseTextures[] : register(t1, space1);
RWTexture2D<float4> RenderTarget : register(u0);


RWStructuredBuffer<SurfelData> surfelsUAV : register(u1); // world position
RWStructuredBuffer<uint> surlfeListUAV : register(u2); // Stored pointers (indices) to the appropriate surfel data
// Stored pointer to a range of surfle IDs 
// SurfelList[from SurfelGrid[i] to SurfelGrid[i+1]] is all the surfel that occupy a cell with index I
RWStructuredBuffer<uint> surfelGridUAV : register(u3); 

//  --PER INSTANCE DATA--
//Flatenned uvs
RWStructuredBuffer<float2> FlattenedUV : register(u4);
RWStructuredBuffer<uint> IndexBuffer : register(u5);
SamplerState defaultSampler : register(s0);






cbuffer RayGen3DBuffer : register(b0)
{
    matrix invViewProj;
    matrix viewToWorld;
    Viewport viewport;
    Viewport stencil;
    
}

cbuffer GridCB : register(b1)
{
    UniformGrid Grid;
};
cbuffer LocalCB : register(b2)
{
    uint4 materialIndex;
    float4 diffuse;
    float4 specular;
    float4 ambient;
    float4 emissive;
    float4 transparent; 
    float opacity;
    float shininess; 
    float specularStrength; 
    float padding;
};


//ConstantBuffer<RayGenConstantBuffer> g_rayGenCB : register(b0);
void FSchlick( inout float3 specular, inout float3 diffuse, float3 lightDir, float3 halfVec )
{
    float fresnel = pow(1.0 - saturate(dot(lightDir, halfVec)), 5.0);
    specular = lerp(specular, 1, fresnel);
    diffuse = lerp(diffuse, 0, fresnel);
}

float3 ApplyAmbientLight(
    float3	diffuse,	// Diffuse albedo
    float	ao,			// Pre-computed ambient-occlusion
    float3	lightColor	// Radiance of ambient light
    )
{
    return ao * diffuse * lightColor;
}



float3 ApplyLightCommon(
    float3	diffuseColor,	// Diffuse albedo
    float3	specularColor,	// Specular albedo
    float	specularMask,	// Where is it shiny or dingy?
    float	gloss,			// Specular power
    float3	normal,			// World-space normal
    float3	viewDir,		// World-space vector from eye to point
    float3	lightDir,		// World-space vector from point to light
    float3	lightColor		// Radiance of directional light
    )
{
    float3 halfVec = normalize(lightDir - viewDir);
    float nDotH = saturate(dot(halfVec, normal));

    FSchlick( diffuseColor, specularColor, lightDir, halfVec );

    float specularFactor = specularMask * pow(nDotH, gloss) * (gloss + 2) / 8;

    float nDotL = saturate(dot(normal, lightDir));

    return nDotL * lightColor * (diffuseColor + specularFactor * specularColor);
}

float3 ApplyDirectionalLight(
    float3	diffuseColor,	// Diffuse albedo
    float3	specularColor,	// Specular albedo
    float	specularMask,	// Where is it shiny or dingy?
    float	gloss,			// Specular power
    float3	normal,			// World-space normal
    float3	viewDir,		// World-space vector from eye to point
    float3	lightDir,		// World-space vector from point to light
    float3	lightColor,		// Radiance of directional light
    float3	shadowCoord,	// Shadow coordinate (Shadow map UV & light-relative Z)
	Texture2D<float> ShadowMap
    )
{

    return  ApplyLightCommon(
        diffuseColor,
        specularColor,
        specularMask,
        gloss,
        normal,
        viewDir,
        lightDir,
        lightColor
        );
}

float3 ApplyPointLight(
    float3	diffuseColor,	// Diffuse albedo
    float3	specularColor,	// Specular albedo
    float	specularMask,	// Where is it shiny or dingy?
    float	gloss,			// Specular power
    float3	normal,			// World-space normal
    float3	viewDir,		// World-space vector from eye to point
    float3	worldPos,		// World-space fragment position
    float3	lightPos,		// World-space light position
    float	lightRadiusSq,
    float3	lightColor		// Radiance of directional light
    )
{
    float3 lightDir = lightPos - worldPos;
    float lightDistSq = dot(lightDir, lightDir);
    float invLightDist = rsqrt(lightDistSq);
    lightDir *= invLightDist;

    // modify 1/d^2 * R^2 to fall off at a fixed radius
    // (R/d)^2 - d/R = [(1/d^2) - (1/R^2)*(d/R)] * R^2
    float distanceFalloff = lightRadiusSq * (invLightDist * invLightDist);
    distanceFalloff = max(0, distanceFalloff - rsqrt(distanceFalloff));

    return distanceFalloff * ApplyLightCommon(
        diffuseColor,
        specularColor,
        specularMask,
        gloss,
        normal,
        viewDir,
        lightDir,
        lightColor
        );
}

typedef BuiltInTriangleIntersectionAttributes MyAttributes;
struct RayPayload
{
    float4 color;
};

bool IsInsideViewport(float2 p, Viewport viewport)
{
    return (p.x >= viewport.left && p.x <= viewport.right)
        && (p.y >= viewport.top && p.y <= viewport.bottom);
}

[shader("raygeneration")]
void MyRaygenShader()
{
    float2 screenUV = (float2) DispatchRaysIndex() / (float2) DispatchRaysDimensions(); // [0,1]
    // Map to NDC space [-1,1]
    float2 ndc = screenUV * 2.0 - 1.0;
    ndc.x = -ndc.x; // Flip Y if needed for DX convention
    ndc.y = -ndc.y; // Flip Y if needed for DX convention

    // Ray origin is the camera position
    float4 originH = mul(float4(0, 0, 0, 1), viewToWorld);
    float3 origin = originH.xyz / originH.w;

    float4 target = mul(float4(ndc.x, ndc.y, 1.0f, 1.0f), invViewProj);
    float3 worldPos = target.xyz / target.w;
    float3 rayDir = normalize(worldPos - origin);


    
        // Trace the ray.
        // Set the ray's extents.
    RayDesc ray;
    ray.Origin = origin;
    ray.Direction = rayDir;
        // Set TMin to a non-zero small value to avoid aliasing issues due to floating - point errors.
        // TMin should be kept small to prevent missing geometry at close contact areas.
    ray.TMin = 0.001;
    ray.TMax = 10000.0;
    RayPayload payload = { float4(0, 0, 0, 0) };
        //TraceRay(Scene, RAY_FLAG_CULL_BACK_FACING_TRIANGLES, ~0, 0, 1, 0, ray, payload);
    TraceRay(Scene, RAY_FLAG_NONE, ~0, 0, 1, 0, ray, payload);
    RenderTarget[DispatchRaysIndex().xy] = payload.color;
//    float3 rayHitWorldPos = payload.color;
//
//    uint3 cellIndex = ComputeGridIndex(rayHitWorldPos, Grid.gridOrigin, Grid.cellSize);
//    uint flattenedIndex = HashGridIndex(cellIndex, Grid);
//    uint surfelIdFrom = surfelGridUAV[flattenedIndex];
//    uint surfelIdTo = surfelGridUAV[flattenedIndex + 1];
//
//    uint surfelCount = surfelIdTo - surfelIdFrom;
//
//    float4 color = float4(0, 0, 0, 0);
//
//    
//    if(surfelCount > 0)
//    {
//        color = payload.color;
//    }
//    //color = float4(FlattenedUV[0].x, FlattenedUV[0].y, 1, 1);
}

//Use per-instance constant buffers to pass the uvs, diffuse/normal textures and material properties

[shader("closesthit")]
void MyClosestHitShader(inout RayPayload payload, in MyAttributes attr)
{
    // Compute hit point in world space
    float3 hitPos = WorldRayOrigin() + RayTCurrent() * WorldRayDirection();
    uint primitiveIndex = PrimitiveIndex();
    uint instanceID = InstanceID();
    //ATTEMP TO RECONSTRCUT THE ACTUAL UVs
    //Assumption is that primitive index is "autogenerated" as per docs
    //But the actual index could be retrieves with prim index indexing
    uint i0 = IndexBuffer[primitiveIndex * 3 + 0];
    uint i1 = IndexBuffer[primitiveIndex * 3 + 1];
    uint i2 = IndexBuffer[primitiveIndex * 3 + 2];
    float2 uv0 = FlattenedUV[i0];
    float2 uv1 = FlattenedUV[i1];
    float2 uv2 = FlattenedUV[i2];
    float3 barycentrics = float3(1 - attr.barycentrics.x - attr.barycentrics.y, attr.barycentrics.x, attr.barycentrics.y);
    float2 interpolatedUV = barycentrics.x * uv0 + barycentrics.y * uv1 + barycentrics.z * uv2;
    uint diffuseID = materialIndex-1;
    float4 diffuseColor = diffuseTextures[diffuseID].SampleLevel(defaultSampler, interpolatedUV, 0);
    //payload.color = float4(interpolatedUV, 1, 1);
    payload.color = diffuseColor * diffuse;

    float gloss = 128.0;

    float3 normal = float3(0, 0, 1);
    //{
    //    normal = SAMPLE_TEX(texNormal) * 2.0 - 1.0;
    //    AntiAliasSpecular(normal, gloss);
    //    float3x3 tbn = float3x3(normalize(vsOutput.tangent), normalize(vsOutput.bitangent), normalize(vsOutput.normal));
    //    normal = normalize(mul(normal, tbn));
    //}

    float3 specularAlbedo = float3( 0.56, 0.56, 0.56 );
    //float specularMask = SAMPLE_TEX(texSpecular).g;
    float specularMask = 0.2f;
    //float3 viewDir = normalize(vsOutput.viewDir);
    
    
    
    //ApplyDirectionalLight(diffuseColor,float3(0,0,0),specularAlbedo,specularMask,normal,)
    //ApplyPointLight(diffuseColor,float3(0,0,0),specularAlbedo,specularMask,normal,)
    //ApplyPointLight()

    
}

[shader("miss")]
void MyMissShader(inout RayPayload payload)
{
    payload.color = float4(0, 0, 0, 1);
}

#endif // RAYTRACING_HLSL