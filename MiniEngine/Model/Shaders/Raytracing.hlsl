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
//cbuffer LocalCB : register(b2)
//{
//    uint4 a;
//};


//ConstantBuffer<RayGenConstantBuffer> g_rayGenCB : register(b0);

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
    float3 rayHitWorldPos = payload.color;

    uint3 cellIndex = ComputeGridIndex(rayHitWorldPos, Grid.gridOrigin, Grid.cellSize);
    uint flattenedIndex = HashGridIndex(cellIndex, Grid);
    uint surfelIdFrom = surfelGridUAV[flattenedIndex];
    uint surfelIdTo = surfelGridUAV[flattenedIndex + 1];

    uint surfelCount = surfelIdTo - surfelIdFrom;

    float4 color = float4(0, 0, 0, 0);

    
    if(surfelCount > 0)
    {
        color = payload.color;
    }
    //color = float4(FlattenedUV[0].x, FlattenedUV[0].y, 1, 1);
    RenderTarget[DispatchRaysIndex().xy] = color;
}

//Use per-instance constant buffers to pass the uvs, diffuse/normal textures and material properties

[shader("closesthit")]
void MyClosestHitShader(inout RayPayload payload, in MyAttributes attr)
{
    // Compute hit point in world space
    float3 hitPos = WorldRayOrigin() + RayTCurrent() * WorldRayDirection();
    uint primitiveIndex = PrimitiveIndex();
    uint instanceID = InstanceID();

    

    
    //For now is just the flattened UV one after the other
    uint uvAttribOffset = 0;
    float2 uv0 = FlattenedUV[primitiveIndex*3 + 0];
    float2 uv1 = FlattenedUV[primitiveIndex*3 + 1];
    float2 uv2 = FlattenedUV[primitiveIndex*3 + 2];

    float3 barycentrics = float3(1 - attr.barycentrics.x - attr.barycentrics.y, attr.barycentrics.x, attr.barycentrics.y);
    float2 interpolatedUV = barycentrics.x * uv0 + barycentrics.y * uv1 + barycentrics.z * uv2;

    uint diffuseID = instanceID * 3;
    float4 diffuseColor = diffuseTextures[diffuseID].SampleLevel(defaultSampler, interpolatedUV, 0);
    //float4 diffuseColor =  diffuseTex.Sample(defaultSampler, interpolatedUV);
    //float4 diffuseColor =  diffuseTex.Sampl(defaultSampler, interpolatedUV);
    //float4 diffuseColor = diffuseTex.SampleLevel(defaultSampler, interpolatedUV, 0);

    payload.color = diffuseColor;
    //payload.color = float4(interpolatedUV, 0, 1);
    //payload.color = float4(interpolatedUV, saturate(diffuseTex[primitiveIndex]).x, 1);

    
}

[shader("miss")]
void MyMissShader(inout RayPayload payload)
{
    payload.color = float4(0, 0, 0, 1);
}

#endif // RAYTRACING_HLSL