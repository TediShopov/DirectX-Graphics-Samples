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
RWTexture2D<float4> RenderTarget : register(u0);


RWStructuredBuffer<SurfelData> surfelsUAV : register(u1); // world position
RWStructuredBuffer<uint> surlfeListUAV : register(u2); // Stored pointers (indices) to the appropriate surfel data
// Stored pointer to a range of surfle IDs 
// SurfelList[from SurfelGrid[i] to SurfelGrid[i+1]] is all the surfel that occupy a cell with index I
RWStructuredBuffer<uint> surfelGridUAV : register(u3); 
RWStructuredBuffer<uint> rayDispatchUUAV : register(u4); 


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


//ConstantBuffer<RayGenConstantBuffer> g_rayGenCB : register(b0);

typedef BuiltInTriangleIntersectionAttributes MyAttributes;
struct RayPayload
{
    float4 color;
};
// Generates a direction in a hemisphere around a normal
float3 SampleHemisphere(float2 rand, float3 normal)
{
    // Convert random [0,1]^2 to spherical coordinates
    float phi = 2.0f * 3.14159265f * rand.x;
    float cosTheta = rand.y;
    float sinTheta = sqrt(1.0f - cosTheta * cosTheta);

    // Cartesian coordinates in tangent space
    float3 tangentSample = float3(cos(phi) * sinTheta, sin(phi) * sinTheta, cosTheta);

    // Create orthonormal basis (TBN)
    float3 up = abs(normal.z) < 0.999f ? float3(0, 0, 1) : float3(1, 0, 0);
    float3 tangent = normalize(cross(up, normal));
    float3 bitangent = cross(normal, tangent);

    // Transform from tangent space to world space
    float3 worldDir =
        tangent * tangentSample.x +
        bitangent * tangentSample.y +
        normal * tangentSample.z;

    return normalize(worldDir);
}



bool IsInsideViewport(float2 p, Viewport viewport)
{
    return (p.x >= viewport.left && p.x <= viewport.right)
        && (p.y >= viewport.top && p.y <= viewport.bottom);
}

[shader("raygeneration")]
void MyRaygenShader()
{
    //float2 screenUV = (float2) DispatchRaysIndex() / (float2) DispatchRaysDimensions(); // [0,1]
    // Map to NDC space [-1,1]
    //float2 ndc = screenUV * 2.0 - 1.0;
    //ndc.x = -ndc.x; // Flip Y if needed for DX convention
    //ndc.y = -ndc.y; // Flip Y if needed for DX convention

    //float4 target = mul(float4(ndc.x, ndc.y, 1.0f, 1.0f), invViewProj);
    //float3 worldPos = target.xyz / target.w;
     uint globalIndex = DispatchRaysIndex().x;
    uint surfelIndex = rayDispatchUUAV[globalIndex];
    uint seed = GetThreadTemporalSeed(DispatchRaysIndex(),frameIndex);
    float2 rnd;
    rnd.x = RandomFloat01(seed);
    rnd.y = RandomFloat01(seed);
    float3 rayDir = SampleHemisphere(rnd, surfelsUAV[surfelIndex].normal);

    // Trace the ray.
    RayDesc ray;
    ray.Origin = surfelsUAV[surfelIndex].position;
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
    RenderTarget[DispatchRaysIndex().xy] = payload.color;
}

[shader("closesthit")]
void MyClosestHitShader(inout RayPayload payload, in MyAttributes attr)
{
    // Compute hit point in world space
    float3 hitPos = WorldRayOrigin() + RayTCurrent() * WorldRayDirection();
    payload.color = float4(hitPos, 1);
    
    //float3 barycentrics = float3(1 - attr.barycentrics.x - attr.barycentrics.y, attr.barycentrics.x, attr.barycentrics.y);
    //payload.color = float4(barycentrics, 1);
}

[shader("miss")]
void MyMissShader(inout RayPayload payload)
{
    payload.color = float4(0, 0, 0, 1);
}

#endif // RAYTRACING_HLSL