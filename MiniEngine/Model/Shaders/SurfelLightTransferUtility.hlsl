#include "SurfelASAsserts.hlsli"
#include "RaytracingHlslCompat.h"
static const float M_PI = 3.14159265f;

struct Ray
{
    float3 origin;
    float3 dir;
    
};
float3 GetWorldNormal(float3 localNormal, float3 surfaceNormal)
{
    // Cartesian coordinates in tangent space

    // Create orthonormal basis (TBN)
    float3 up = abs(surfaceNormal.z) < 0.999f ? float3(0, 0, 1) : float3(1, 0, 0);
    float3 tangent = normalize(cross(up, surfaceNormal));
    float3 bitangent = cross(surfaceNormal, tangent);

    // Transform from tangent space to world space
    float3 worldDir =
        tangent * localNormal.x +
        bitangent * localNormal.y +
        surfaceNormal * localNormal.z;
    return worldDir;

    
}

// Generates a direction in a hemisphere around a normal
float3 SampleHemisphere(float2 rand, float3 normal)
{
    // Convert random [0,1]^2 to spherical coordinates
    float phi = 2.0f * 3.14159265f * rand.x;
    float cosTheta = rand.y;
    float sinTheta = sqrt(1.0f - cosTheta * cosTheta);

    // Cartesian coordinates in tangent space
    float3 tangentSample = float3(cos(phi) * sinTheta, sin(phi) * sinTheta, cosTheta);
    return normalize(GetWorldNormal(tangentSample, normal));

}
float3 CosineSampleHemisphere(float2 rand, float3 normal)
{
    const float r = sqrt(rand.x);
    const float theta = 2 * M_PI * rand.y;
 
    const float x = r * cos(theta);
    const float y = r * sin(theta);
 
    float3 tangentSample = float3(x, y, sqrt(max(0.0f, 1 - rand.x)));
    return normalize(GetWorldNormal(tangentSample, normal));
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
