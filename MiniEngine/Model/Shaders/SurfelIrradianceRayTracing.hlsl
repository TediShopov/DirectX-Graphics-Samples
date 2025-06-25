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
static const float M_PI = 3.14159265f;

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
void AntiAliasSpecular( inout float3 texNormal, inout float gloss )
{
    float normalLenSq = dot(texNormal, texNormal);
    float invNormalLen = rsqrt(normalLenSq);
    texNormal *= invNormalLen;
    float normalLen = normalLenSq * invNormalLen;
	float flatness = saturate(1 - abs(ddx(normalLen)) - abs(ddy(normalLen)));
	gloss = exp2(lerp(0, log2(gloss), flatness));
}
struct ShadowPayload
{
    bool hit;
};

float3x3 OuterProduct(float3 a, float3 b)
{
    return float3x3(
        a.x * b.x, a.x * b.y, a.x * b.z,
        a.y * b.x, a.y * b.y, a.y * b.z,
        a.z * b.x, a.z * b.y, a.z * b.z
    );
}

float3x3 InverseMatrix3x3(float3x3 m)
{
    float3 a = m[0]; // column 0
    float3 b = m[1]; // column 1
    float3 c = m[2]; // column 2

    float3 r0 = cross(b, c);
    float3 r1 = cross(c, a);
    float3 r2 = cross(a, b);

    float det = dot(a, r0);
    float invDet = 1.0 / det;

    float3x3 adj = float3x3(
        r0.x, r1.x, r2.x,
        r0.y, r1.y, r2.y,
        r0.z, r1.z, r2.z
    );

    return adj * invDet;
}


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
    float N = surfelsUAV[globalIndex].raySamples.x;
    for (int i = 0; i < N; i++)
    {
        uint3 index3 = DispatchRaysIndex();
        index3.y = i;
        uint seed = GetThreadTemporalSeed(index3, frameIndex);
        float2 rnd;
        rnd.x = RandomFloat01(seed);
        rnd.y = RandomFloat01(seed);
        float3 rayDir = SampleHemisphere(rnd, surfelsUAV[globalIndex].normal);

        meanRayDir += rayDir;
        sumOuter += OuterProduct(rayDir, rayDir);


    // Trace the ray.
        RayDesc ray;
        ray.Origin = surfelsUAV[globalIndex].position;
        ray.Direction = rayDir;
        ray.TMin = 0.001;
        ray.TMax = 10000.0;

        RayPayload payload = { float4(0, 0, 0, 0) };
        TraceRay(Scene, RAY_FLAG_NONE, ~0, 0, 1, 0, ray, payload);
        float3 rayHitWorldPos = payload.color;
        float3 Li = payload.color; // returned radiance
        float cosTheta = saturate(dot(rayDir, surfelsUAV[globalIndex].normal));
        float3 radiance = Li * cosTheta * (2.0f * M_PI);
        accumulatedIrradiance += radiance;
        
    }
    meanRayDir /= N;
    float3x3 cov = sumOuter / N - OuterProduct(meanRayDir, meanRayDir);
    float3x3 inverseCov = InverseMatrix3x3(cov);
    //
    surfelsUAV[globalIndex].mean = float4(meanRayDir, 0);

    //Extract the rows from the covariance matrix
    surfelsUAV[globalIndex].co1 = float4(inverseCov._m00_m01_m02, 0);
    surfelsUAV[globalIndex].co2 = float4(inverseCov._m10_m11_m12, 0);
    surfelsUAV[globalIndex].co3 = float4(inverseCov._m20_m21_m22, 0);



    
    
    
    
    InterlockedAdd(surfelsUAV[globalIndex].raySamples.y, surfelsUAV[globalIndex].raySamples.x);


    //Average
    accumulatedIrradiance /= surfelsUAV[globalIndex].raySamples.x;
    //Integrate

    uint frameOffset = frameIndex - surfelsUAV[globalIndex].padding.x;
    //surfelsUAV[globalIndex].color = float4(accumulatedIrradiance, 1);
    //if (frameOffset < 5)
    //{
    //    surfelsUAV[globalIndex].color = float4(accumulatedIrradiance, 1);
    //    
    //}
    //else
    //{
        float alpha = saturate(0.1f / (1.0f + frameIndex * 0.01));
        surfelsUAV[globalIndex].color = lerp(surfelsUAV[globalIndex].color, float4(accumulatedIrradiance, 1), alpha);
        
    //}

        
}

//Ray generetion relying on ray dispatch data
//[shader("raygeneration")]
//void MyRaygenShader()
//{
//     uint globalIndex = DispatchRaysIndex().x;
//    uint surfelIndex = rayDispatchUUAV[globalIndex];
//    uint seed = GetThreadTemporalSeed(DispatchRaysIndex(),frameIndex);
//    float2 rnd;
//    rnd.x = RandomFloat01(seed);
//    rnd.y = RandomFloat01(seed);
//    float3 rayDir = SampleHemisphere(rnd, surfelsUAV[surfelIndex].normal);
//
//    // Trace the ray.
//    RayDesc ray;
//    ray.Origin = surfelsUAV[surfelIndex].position;
//    ray.Direction = rayDir;
//    ray.TMin = 0.001;
//    ray.TMax = 10000.0;
//    RayPayload payload = { float4(0, 0, 0, 0) };
//    TraceRay(Scene, RAY_FLAG_NONE, ~0, 0, 1, 0, ray, payload);
//    float3 rayHitWorldPos = payload.color;
//
//    //Accumulate radiance
//    float3 Li = payload.color;             // returned radiance
//    float cosTheta = saturate(dot(rayDir, surfelsUAV[surfelIndex].normal));
//
//    float3 irradiance = Li * cosTheta * (2.0f * M_PI);
//    InterlockedAdd(surfelsUAV[surfelIndex].color.r, irradiance.r);
//
//    
//    //uint3 cellIndex = ComputeGridIndex(rayHitWorldPos, Grid.gridOrigin, Grid.cellSize);
//    //uint flattenedIndex = HashGridIndex(cellIndex, Grid);
//    //uint surfelIdFrom = surfelGridUAV[flattenedIndex];
//    //uint surfelIdTo = surfelGridUAV[flattenedIndex + 1];
//
//    //+
//    //
//
//    //uint surfelCount = surfelIdTo - surfelIdFrom;
//    //RenderTarget[DispatchRaysIndex().xy] = payload.color;
//}

struct Ray
{
    float3 origin;
    float3 dir;
    
};

bool IntersectRayWithSurfel(Ray ray, SurfelData surfel, out float t)
{

    // Plane intersection
    float denom = dot(ray.dir, surfel.normal);
    if (abs(denom) < 1e-4) return false; // Parallel ray

    float3 toSurfel = surfel.position - ray.origin;
    t = dot(toSurfel, surfel.normal) / denom;
    if (t < 0) return false; // Intersection is behind ray origin

    // Intersection point
    float3 hitPoint = ray.origin + t * ray.dir;

    // Check if inside disk
    float dist2 = dot(hitPoint - surfel.position, hitPoint - surfel.position);
    return dist2 <= (surfel.radius * surfel.radius);
}


[shader("closesthit")]
void MyClosestHitShader(inout RayPayload payload, in MyAttributes attr)
{
    float3 hitPos = WorldRayOrigin() + RayTCurrent() * WorldRayDirection();
    uint primitiveIndex = PrimitiveIndex();
    uint instanceID = InstanceID();
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
        if(surfel.padding.y < 5)
        {
            continue;
        }

        if(IntersectRayWithSurfel(ra, surfel, t))
        {
            payload.color = surfel.color;
            payload.color.w = 1;
            return;
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

    uint instanceId = (materialIndex.x);
    uint specularID = instanceId * 3;
    uint normalID = instanceId * 3 +1;
    uint diffuseID = instanceId * 3+ 2;
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
    payload.color = float4(dirColor, 1);

    
    //TODO chekck if ray is intersecting with another surfel.
    //Use surfels irradiance from previous frame if that is the case

    
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