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

#include "SurfelLightTransferUtility.hlsl"

struct AdditionalVertexData
{
    float4 uv;
    float4 normal;
    float4 tangent;
    float4 bitangent;
};



RaytracingAccelerationStructure Scene : register(t0, space0);
Texture2D instanceTextures[] : register(t1, space1);
RWTexture2D<float4> RenderTarget : register(u0);


RWStructuredBuffer<SurfelData> surfelsUAV : register(u1); // world position
RWStructuredBuffer<uint> surlfeListUAV : register(u2); // Stored pointers (indices) to the appropriate surfel data
// Stored pointer to a range of surfle IDs 
// SurfelList[from SurfelGrid[i] to SurfelGrid[i+1]] is all the surfel that occupy a cell with index I
RWStructuredBuffer<uint> surfelGridUAV : register(u3); 

RWStructuredBuffer<uint> rayDispatchUUAV : register(u4); 
//  --PER INSTANCE DATA--
//Flatenned uvs
RWStructuredBuffer<AdditionalVertexData> AdditionalVertexDataBuffer : register(u5);
RWStructuredBuffer<uint> IndexBuffer : register(u6);

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
    uint frameIndex;
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

typedef BuiltInTriangleIntersectionAttributes MyAttributes;
struct RayPayload
{
    float4 color;
};

struct ShadowPayload
{
    bool hit;
};



//Simplified approach. Use count of rays = to surfel count. Each thread would cast X rays based on surfel count.
//This is naive approach however the benefit it that all irradiances can be averaged and integrated in here 
//and there is no reliance on InterclockedAdd that does not support floats
[shader("raygeneration")]
void MyRaygenShader()
{
    uint globalIndex = DispatchRaysIndex().x;
    surfelsUAV[globalIndex].raySamples;

    float3 accumulatedIrradiance = float3(0, 0, 0);
    float3 meanRayDir = float3(0, 0, 0);
    float3x3 sumOuter = 0;

    float variance = surfelsUAV[globalIndex].msme.inconsistency;
    float minRays = 1;
    float maxRays = 30;
    float N = lerp(minRays, maxRays, variance);
    surfelsUAV[globalIndex].raySamples.x = N;
    
    
    //float N = surfelsUAV[globalIndex].raySamples.x;
    for (int i = 0; i < N; i++)
    {
        uint3 index3 = DispatchRaysIndex();
        index3.y = i;
        uint seed = GetThreadTemporalSeed(index3, frameIndex);
        float2 rnd;
        rnd.x = RandomFloat01(seed);
        rnd.y = RandomFloat01(seed);
        float3 rayDir = CosineSampleHemisphere(rnd, surfelsUAV[globalIndex].normal);

        meanRayDir += rayDir;
        sumOuter += OuterProduct(rayDir, rayDir);


    // Trace the ray.
        RayDesc ray;
        ray.Origin = surfelsUAV[globalIndex].position;

        ray.Direction = rayDir;
        ray.TMin = 0.001;
        ray.TMax = 10000.0;

        RayPayload payload = { float4(surfelsUAV[globalIndex].normal.xyz, 0) };
        TraceRay(Scene, RAY_FLAG_NONE, ~0, 0, 1, 0, ray, payload);
        float3 rayHitWorldPos = payload.color;
        float3 Li = payload.color; // returned radiance
        float cosTheta = saturate(dot(rayDir, surfelsUAV[globalIndex].normal));
        //float3 radiance = Li * cosTheta * (2.0f * M_PI);
        float3 radiance = Li * M_PI; accumulatedIrradiance += radiance;
        
    }
    meanRayDir /= N;
    surfelsUAV[globalIndex].mean = float4(meanRayDir, 0);
    accumulatedIrradiance /= surfelsUAV[globalIndex].raySamples.x;

    InterlockedAdd(surfelsUAV[globalIndex].raySamples.y, surfelsUAV[globalIndex].raySamples.x);

    //First time irradiance is accumulated
    if( surfelsUAV[globalIndex].raySamples.y <= surfelsUAV[globalIndex].raySamples.x)
    {
        surfelsUAV[globalIndex].msme.mean = float4(accumulatedIrradiance, 1);
        surfelsUAV[globalIndex].msme.shortMean = accumulatedIrradiance;
    }
    else
    {
        surfelsUAV[globalIndex].color = float4(MultiscaleMeanEstimator(accumulatedIrradiance, surfelsUAV[globalIndex].msme, 0.08), 1);
    }


    
    
    
    



        
}

float3 calculateSurfelsContribution_Experimental(SurfelData surfel, float3 worldPos, float3 interpolatedNormal)
{
    float3 colorContribution = float3(0, 0, 0);
    float3 d = length(worldPos - surfel.position);

     float contribution = 1.f;
    float dotN = dot(interpolatedNormal, normalize(surfel.normal));

    contribution *= saturate(dotN);
    contribution *= saturate(1 - d / surfel.radius);
    contribution = smoothstep(0, 1, contribution);

//    float w; // Spatial weight
//    float maxDistance = surfel.radius; 
//    float t = clamp(d / maxDistance, 0.0, 1.0); // Normalize distance
//    float attenuation = smoothstep(0.0, 1.0, 1.0 - t); // Smoothstep attenuation


    colorContribution = surfel.color * contribution;
    return colorContribution;
        
}

[shader("closesthit")]
void MyClosestHitShader(inout RayPayload payload, in MyAttributes attr)
{
    float3 hitPos = WorldRayOrigin() + RayTCurrent() * WorldRayDirection();
    uint instanceID = InstanceID();
    uint primitiveIndex = PrimitiveIndex();

    
    float3 originatingSurfelNormal = payload.color;
    //ATTEMP TO RECONSTRCUT THE ACTUAL UVs
    //Assumption is that primitive index is "autogenerated" as per docs
    //But the actual index could be retrieves with prim index indexing
    uint i0 = IndexBuffer[primitiveIndex * 3 + 0];
    uint i1 = IndexBuffer[primitiveIndex * 3 + 1];
    uint i2 = IndexBuffer[primitiveIndex * 3 + 2];

    AdditionalVertexData v0 = AdditionalVertexDataBuffer[i0];
    AdditionalVertexData v1 = AdditionalVertexDataBuffer[i1];
    AdditionalVertexData v2 = AdditionalVertexDataBuffer[i2];
    

   uint3 cellIndex = ComputeGridIndex(hitPos, Grid.gridOrigin, Grid.cellSize);
   uint flattenedIndex = HashGridIndex(cellIndex, Grid);
   uint surfelIdFrom = surfelGridUAV[flattenedIndex];
   uint surfelIdTo = surfelGridUAV[flattenedIndex + 1];

    
    float3 radiance = float3(0, 0, 0);


    for (int i = surfelIdFrom; i < surfelIdTo; i++)
    {
        uint surfelIndex = surlfeListUAV[i];
        SurfelData surfel = surfelsUAV[surfelIndex];
        Ray ra;
        ra.origin = WorldRayOrigin();
        //ra.origin = float3(-100000, -100000, -10000);
        //ra.dir = float3(0, 1, 0);
        ra.dir = WorldRayDirection();
        float t;

        //Used for counting how many rays have been fired from surfel
        if(surfel.raySamples.y <20)
        {
            continue;
        }

        if(IntersectRayWithSurfel(ra, surfel, t))
        {
            //FINISH CODE HERE
//            float3 albedo = float3(1, 1, 1); // or assume float3(1,1,1) if no albedo
//            float3 irradiance = surfel.color.rgb; // surfel's accumulated irradiance
//            float3 radiance = irradiance * albedo / M_PI;
            float3 directionToRayOrigin = -WorldRayDirection();

            
            //float3 surfelRadiance = calculateSurfelsContribution_Experimental(surfel, WorldRayOrigin(), normalize(directionToRayOrigin));
            float3 surfelRadiance = calculateSurfelsContribution_Experimental(surfel, WorldRayOrigin(), surfel.normal);
            radiance += float4(radiance, 1.0);
            uint outO;
            InterlockedExchange(surfelsUAV[surfelIndex].contribution.y, frameIndex, outO);
        }
        
    }
    //payload.color = float4(0, 0, 0, 1);
    
    float3 barycentrics = float3(1 - attr.barycentrics.x - attr.barycentrics.y, attr.barycentrics.x, attr.barycentrics.y);

    //Interpolated all the vertex data
    AdditionalVertexData interpolated;
    interpolated.uv = barycentrics.x * v0.uv + barycentrics.y * v1.uv + barycentrics.z * v2.uv;
    interpolated.normal= barycentrics.x * v0.normal + barycentrics.y * v1.normal + barycentrics.z * v2.normal;
    interpolated.tangent= barycentrics.x * v0.tangent + barycentrics.y * v1.tangent + barycentrics.z * v2.tangent;
    interpolated.bitangent= barycentrics.x * v0.bitangent + barycentrics.y * v1.bitangent + barycentrics.z * v2.bitangent;

    uint instanceId = (materialIndex.x) - 1;
    uint specularID = instanceId * 3 ;
    uint normalID = instanceId * 3 +1;
    uint diffuseID = instanceId * 3 + 2;

    float3 diffuseColor = instanceTextures[diffuseID].SampleLevel(defaultSampler, interpolated.uv, 0);
    float3 specularColor = instanceTextures[specularID].SampleLevel(defaultSampler, interpolated.uv, 0);
    float3 normalColor = instanceTextures[normalID].SampleLevel(defaultSampler, interpolated.uv, 0);


    float gloss = 128.0;
    float3 normal;

    {
        normal = normalColor * 2.0 - 1.0;
        AntiAliasSpecular(normal, gloss);
        float3x3 tbn = float3x3(normalize(interpolated.tangent).xyz, normalize(interpolated.bitangent).xyz, normalize(interpolated.normal).xyz);
        normal = normalize(mul(normal, tbn));
    }
    float3 specularAlbedo = float3( 0.56, 0.56, 0.56 );
    float specularMask = specularColor;

    //This is not the viewiign direction toward the camera but 
    //Direction towards the surfel in worlds space
    float3 towardSurfel = -WorldRayDirection();
    float3 dirColor = ApplyDirectionalLight(diffuseColor, float3(0, 0, 0), specularAlbedo, specularMask, normal,towardSurfel, sunDirection, sunColor);
    radiance += dirColor;

    payload.color = float4(radiance, 1);

    

    
    //Cast Shadow Ray
    RayDesc shadowRay;
    shadowRay.Origin = hitPos + normal * 0.001;
    shadowRay.Direction = normalize(sunDirection);
    shadowRay.TMin = 0.001;
    shadowRay.TMax = 1e6;
    ShadowPayload shadowPayload = { true };

    TraceRay(Scene, RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH | RAY_FLAG_SKIP_CLOSEST_HIT_SHADER, ~0, 0, 1, 1, shadowRay, shadowPayload);
    if(shadowPayload.hit == true)
    {
        //payload.color = float4(diffuseColor,1) * sunAbmientColor;
        payload.color = float4(0, 0, 0, 1);
    }
}

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