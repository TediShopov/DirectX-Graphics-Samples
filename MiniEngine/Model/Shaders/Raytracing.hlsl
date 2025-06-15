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


cbuffer RayGen3DBuffer : register(b0)
{
    matrix invViewProj;
    matrix viewToWorld;
    Viewport viewport;
    Viewport stencil;
    
}


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
    //float2 lerpValues = (float2)DispatchRaysIndex() / (float2)DispatchRaysDimensions();

    // Orthographic projection since we're raytracing in screen space.
//    float3 rayDir = float3(0, 0, 1);
//    float3 origin = float3(
//        lerp(g_rayGenCB.viewport.left, g_rayGenCB.viewport.right, lerpValues.x),
//        lerp(g_rayGenCB.viewport.bottom, g_rayGenCB.viewport.top, lerpValues.y),
//        0.0f);

    
    // Get dispatch pixel coords
    float2 screenUV = (float2)DispatchRaysIndex() / (float2)DispatchRaysDimensions(); // [0,1]

    // Map to NDC space [-1,1]
    float2 ndc = screenUV * 2.0 - 1.0;
    ndc.x = -ndc.x; // Flip Y if needed for DX convention
    ndc.y = -ndc.y; // Flip Y if needed for DX convention

    // Ray origin is the camera position
    float4 originH = mul(float4(0, 0, 0, 1), viewToWorld);
    float3 origin = originH.xyz / originH.w;

   // surfelGridUAV[0] = 0;
   // surlfeListUAV[0] = 0;

    // Reconstruct world-space ray direction from NDC through inverse view-projection
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

        // Write the raytraced color to the output texture.
        RenderTarget[DispatchRaysIndex().xy] = payload.color;

//    if (IsInsideViewport(origin.xy, stencil))
//    {
//        // Trace the ray.
//        // Set the ray's extents.
//        RayDesc ray;
//        ray.Origin = origin;
//        ray.Direction = rayDir;
//        // Set TMin to a non-zero small value to avoid aliasing issues due to floating - point errors.
//        // TMin should be kept small to prevent missing geometry at close contact areas.
//        ray.TMin = 0.001;
//        ray.TMax = 10000.0;
//        RayPayload payload = { float4(0, 0, 0, 0) };
//        //TraceRay(Scene, RAY_FLAG_CULL_BACK_FACING_TRIANGLES, ~0, 0, 1, 0, ray, payload);
//        TraceRay(Scene, RAY_FLAG_NONE, ~0, 0, 1, 0, ray, payload);
//
//        // Write the raytraced color to the output texture.
//        RenderTarget[DispatchRaysIndex().xy] = payload.color;
//    }
//    else
//    {
//        // Render interpolated DispatchRaysIndex outside the stencil window
//        RenderTarget[DispatchRaysIndex().xy] = float4(screenUV, 0, 1);
//    }
}

[shader("closesthit")]
void MyClosestHitShader(inout RayPayload payload, in MyAttributes attr)
{
    float3 barycentrics = float3(1 - attr.barycentrics.x - attr.barycentrics.y, attr.barycentrics.x, attr.barycentrics.y);
    payload.color = float4(barycentrics, 1);
}

[shader("miss")]
void MyMissShader(inout RayPayload payload)
{
    payload.color = float4(0, 0, 0, 1);
}

#endif // RAYTRACING_HLSL