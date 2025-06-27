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

struct AdditionalVertexData
{
    float4 uv;
    float4 normal;
    float4 tangent;
    float4 bitangent;
};

RaytracingAccelerationStructure Scene : register(t0, space0);
Texture2D<float3> instanceTextures[] : register(t1, space1);
RWTexture2D<float4> RenderTarget : register(u0);


RWStructuredBuffer<SurfelData> surfelsUAV : register(u1); // world position
RWStructuredBuffer<uint> surlfeListUAV : register(u2); // Stored pointers (indices) to the appropriate surfel data

// Stored pointer to a range of surfle IDs 
// SurfelList[from SurfelGrid[i] to SurfelGrid[i+1]] is all the surfel that occupy a cell with index I
RWStructuredBuffer<uint> surfelGridUAV : register(u3); 
//  --PER INSTANCE DATA--
//Flatenned uvs
RWStructuredBuffer<AdditionalVertexData> AdditionalVertexDataBuffer : register(u4);
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
cbuffer SunDirectionalLight : register(b2)
{
    float4 sunDirection;
    float4 sunColor;
    float4 sunAbmientColor;
};
cbuffer LocalCB : register(b3)
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
    float3	lightColor		// Radiance of directional light
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

void AntiAliasSpecular( inout float3 texNormal, inout float gloss )
{
    float normalLenSq = dot(texNormal, texNormal);
    float invNormalLen = rsqrt(normalLenSq);
    texNormal *= invNormalLen;
    float normalLen = normalLenSq * invNormalLen;
	float flatness = saturate(1 - abs(ddx(normalLen)) - abs(ddy(normalLen)));
	gloss = exp2(lerp(0, log2(gloss), flatness));
}
bool IsInsideViewport(float2 p, Viewport viewport)
{
    return (p.x >= viewport.left && p.x <= viewport.right)
        && (p.y >= viewport.top && p.y <= viewport.bottom);
}
struct ShadowPayload
{
    bool hit;
};


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
    //RayPayload payload = { float4(0, 0, 0, 0) };
    //Payload is now carying view directoin
    RayPayload payload = { float4(-rayDir.xyz,0) };
        //TraceRay(Scene, RAY_FLAG_CULL_BACK_FACING_TRIANGLES, ~0, 0, 1, 0, ray, payload);
    TraceRay(Scene, RAY_FLAG_NONE, ~0, 0, 1, 0, ray, payload);
    RenderTarget[DispatchRaysIndex().xy] = payload.color;
}




//Use per-instance constant buffers to pass the uvs, diffuse/normal textures and material properties

//This does not need to be a ray-tracing pass 
//Closest hit shader to visualize contributions from Surfel Data based on Mahalonobis Distance
[shader("closesthit")]
void MyClosestHitShader(inout RayPayload payload, in MyAttributes attr)
{

    // Compute hit point in world space
    float3 worldPos = WorldRayOrigin() + RayTCurrent() * WorldRayDirection();
    uint primitiveIndex = PrimitiveIndex();
    uint instanceID = InstanceID();

    //Get all for now
    //TODO make a bounding box around the surfel to only allow surfel in some range
    //TODO make a raycast toward surfels position to see if it contributes
    //uint3 index = ComputeGridIndex(worldPos, Grid.gridOrigin, Grid.cellSize);
    float3 colorE = float3(0, 0, 0);
    uint surfelNum = 0;
    uint surfelStride = 0;

    
    uint3 idx = ComputeGridIndex(worldPos, Grid.gridOrigin, Grid.cellSize);

    uint linearIndex = HashGridIndex(idx, Grid);
    uint surfelListIndexFrom = surfelGridUAV[linearIndex];
    uint surfelListIndexTo = surfelGridUAV[linearIndex + 1];

//    for (uint i = surfelListIndexFrom; i < surfelListIndexTo; ++i)
//    {
//        uint index = surlfeListUAV[i];
//        SurfelData surfel = surfelsUAV[index];
//
//        float3x3 covarianceInverse = 0;
//        covarianceInverse._m00_m01_m02 = surfel.co1;
//        covarianceInverse._m10_m11_m12 = surfel.co2;
//        covarianceInverse._m20_m21_m22 = surfel.co3;
//
//        float3 d = worldPos - surfel.position;
//        float3 dTransformed = mul(covarianceInverse, d);
//        float D2 = dot(d, dTransformed); // Mahalanobis distance squared
//        D2 /= 3000.0f;
//                    //D2 /= 10000.0f;
//        float w = exp(-0.5 * D2); // Gaussian weight
//
//        float3 L = -normalize(d); // light dir toward surfel
//        float NdotL = saturate(dot(surfel.normal, L));
//        if (NdotL > 0.0f)
//        {
//            colorE += surfel.color * NdotL * w;
//        }
//    }


    

    SurfelData mock;
    mock.position = float4(worldPos, 1);
    mock.radius = Grid.cellSize.x*2;
    //mock.radius = Grid.cellSize.x /3.0f;
    uint3 bb[2];
    SurfelSOIBoundingCells(mock, Grid, bb);

    uint3 minCell = bb[0];
    uint3 maxCell = bb[1];


    



    // Iterate over overlapping cells
    for (uint z = minCell.z; z <= maxCell.z; ++z)
    {
        for (uint y = minCell.y; y <= maxCell.y; ++y)
        {
            for (uint x = minCell.x; x <= maxCell.x; ++x)
            {
                //uint linearIndex = x + y * gridDim.x + z * gridDim.x * gridDim.y;
                uint3 idx = uint3(x, y, z);
                uint linearIndex = HashGridIndex(idx,Grid);
                uint surfelListIndexFrom = surfelGridUAV[linearIndex];
                uint surfelListIndexTo = surfelGridUAV[linearIndex+1];

                for (uint i = surfelListIndexFrom; i < surfelListIndexTo; ++i)
                {
                    uint index = surlfeListUAV[i];
                    SurfelData surfel = surfelsUAV[index];
                    float3 d = worldPos - surfel.position;
                    float3 L = -normalize(d); // light dir toward surfel
                    float NdotL = saturate(dot(surfel.normal, L));
                    if (NdotL <= 0.0f)
                    {
                        continue;
                    }

                    float3x3 covarianceInverse = 0;
                    covarianceInverse._m00_m01_m02 = surfel.co1;
                    covarianceInverse._m10_m11_m12 = surfel.co2;
                    covarianceInverse._m20_m21_m22 = surfel.co3;

                    float3 dTransformed = mul(covarianceInverse, d);
                    float D2 = dot(d, dTransformed); // Mahalanobis distance squared
                    D2 /= 3000.0f;
                    //D2 /= 10000.0f;
                    float w = exp(-0.5 * D2); // Gaussian weight

                        colorE += surfel.color * NdotL * w;
                    
                }

            }
        }
}

    payload.color = float4(colorE, 1);

    
    

//    surfelsUAV.GetDimensions(surfelNum,surfelStride);
//
//    for (uint i = 0; i < surfelNum; ++i)
//    {
//        float3x3 covarianceInverse = 0;
//        covarianceInverse._m00_m01_m02 = surfelsUAV[i].co1;
//        covarianceInverse._m10_m11_m12 = surfelsUAV[i].co2;
//        covarianceInverse._m20_m21_m22 = surfelsUAV[i].co3;
//
//        float3 d = worldPos - surfelsUAV[i].position;
//        float3 dTransformed = mul(covarianceInverse, d);
//        float D2 = dot(d, dTransformed); // Mahalanobis distance squared
//        //D2 /= 2000.0f;
//        D2 /= 10000.0f;
//        float w = exp(-0.5 * D2); // Gaussian weight
//
//        float3 L = -normalize(d); // light dir toward surfel
//        float NdotL = saturate(dot(surfelsUAV[i].normal, L));
//        //if (NdotL > 0.0f && w > threshold)
//        if (NdotL > 0.0f)
//        {
//        // optional visibility check with ray as before...
//            colorE += surfelsUAV[i].color * NdotL * w;
//            //colorE += float3(10,10,10) * NdotL * w;
//        }
//    }
//    payload.color = float4(colorE, 1);
}

////Closest hit shader to debug BRDF 
//[shader("closesthit")]
//void MyClosestHitShader(inout RayPayload payload, in MyAttributes attr)
//{
//    // Compute hit point in world space
//    float3 hitPos = WorldRayOrigin() + RayTCurrent() * WorldRayDirection();
//    uint primitiveIndex = PrimitiveIndex();
//    uint instanceID = InstanceID();
//    //ATTEMP TO RECONSTRCUT THE ACTUAL UVs
//    //Assumption is that primitive index is "autogenerated" as per docs
//    //But the actual index could be retrieves with prim index indexing
//    uint i0 = IndexBuffer[primitiveIndex * 3 + 0];
//    uint i1 = IndexBuffer[primitiveIndex * 3 + 1];
//    uint i2 = IndexBuffer[primitiveIndex * 3 + 2];
//
//    AdditionalVertexData v0 = AdditionalVertexDataBuffer[i0];
//    AdditionalVertexData v1 = AdditionalVertexDataBuffer[i1];
//    AdditionalVertexData v2 = AdditionalVertexDataBuffer[i2];
//    
//    
//    
//    float3 barycentrics = float3(1 - attr.barycentrics.x - attr.barycentrics.y, attr.barycentrics.x, attr.barycentrics.y);
//
//    //Interpolated all the vertex data
//    AdditionalVertexData interpolated;
//    interpolated.uv = barycentrics.x * v0.uv + barycentrics.y * v1.uv + barycentrics.z * v2.uv;
//    interpolated.normal= barycentrics.x * v0.normal + barycentrics.y * v1.normal + barycentrics.z * v2.normal;
//    interpolated.tangent= barycentrics.x * v0.tangent + barycentrics.y * v1.tangent + barycentrics.z * v2.tangent;
//    interpolated.bitangent= barycentrics.x * v0.bitangent + barycentrics.y * v1.bitangent + barycentrics.z * v2.bitangent;
//
//    uint instanceId = (materialIndex.x);
//    uint specularID = instanceId * 3;
//    uint normalID = instanceId * 3 +1;
//    uint diffuseID = instanceId * 3+ 2;
//    float3 diffuseColor = instanceTextures[diffuseID].SampleLevel(defaultSampler, interpolated.uv, 0);
//    float3 specularColor = instanceTextures[specularID].SampleLevel(defaultSampler, interpolated.uv, 0);
//    float3 normalColor = instanceTextures[normalID].SampleLevel(defaultSampler, interpolated.uv, 0);
//
//
//    float gloss = 128.0;
//    float3 normal;
//
//    {
//        normal = normalColor * 2.0 - 1.0;
//        AntiAliasSpecular(normal, gloss);
//        float3x3 tbn = float3x3(normalize(interpolated.tangent).xyz, normalize(interpolated.bitangent).xyz, normalize(interpolated.normal).xyz);
//        normal = normalize(mul(normal, tbn));
//    }
//    float3 specularAlbedo = float3( 0.56, 0.56, 0.56 );
//    float specularMask = specularColor;
//    float3 dirColor = ApplyDirectionalLight(diffuseColor, float3(0, 0, 0), specularAlbedo, specularMask, normal,payload.color, sunDirection, sunColor);
//    payload.color = float4(dirColor, 1);
//
//    
//    //Cast Shadow Ray
//    RayDesc shadowRay;
//    shadowRay.Origin = hitPos + normal * 0.001;
//    shadowRay.Direction = normalize(sunDirection);
//    shadowRay.TMin = 0.001;
//    shadowRay.TMax = 1e6;
//    ShadowPayload shadowPayload = { true };
//
//    TraceRay(Scene, RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH | RAY_FLAG_SKIP_CLOSEST_HIT_SHADER, ~0, 0, 1, 1, shadowRay, shadowPayload);
//    if(shadowPayload.hit == true)
//    {
//        payload.color = float4(diffuseColor,1) * sunAbmientColor;
//    }
//
//
//
//    
//    
//}

[shader("miss")]
void MyMissShader(inout RayPayload payload)
{
    payload.color = float4(0, 0, 0, 1);
}
[shader("miss")]
void ShadowMissShader(inout ShadowPayload payload)
{
    payload.hit = false;
}



#endif // RAYTRACING_HLSL