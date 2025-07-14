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


typedef BuiltInTriangleIntersectionAttributes MyAttributes;
struct RayPayload
{
    float4 color;
};

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


    uint i0 = IndexBuffer[primitiveIndex * 3 + 0];
    uint i1 = IndexBuffer[primitiveIndex * 3 + 1];
    uint i2 = IndexBuffer[primitiveIndex * 3 + 2];
    AdditionalVertexData v0 = AdditionalVertexDataBuffer[i0];
    AdditionalVertexData v1 = AdditionalVertexDataBuffer[i1];
    AdditionalVertexData v2 = AdditionalVertexDataBuffer[i2];

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
    //float3 diffuseColor = instanceTextures[diffuseID].Sample(defaultSampler, interpolated.uv);

    payload.color = float4(diffuseColor, 1);
    return;
    
    


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
    payload.color = float4(dirColor, 1);
    
    

    
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