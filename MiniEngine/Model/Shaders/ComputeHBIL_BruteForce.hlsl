////////////////////////////////////////////////////////////////////////////////
// Shaders to compute HBIL
////////////////////////////////////////////////////////////////////////////////
//
#include "Global.hlsl"
#include "HBIL.hlsl"

#define MAX_ANGLES	16									// Amount of circle subdivisions per pixel
#define MAX_SAMPLES	16									// Maximum amount of samples per circle subdivision

Texture2D< float >	_tex_depth : register(t0);			// Depth or distance buffer (here we're given depth)
Texture2D 	_tex_normal : register(t1);			// Camera-space normal vectors
Texture2D	_tex_sourceRadiance : register(t2);	// Last frame's reprojected radiance buffer
Texture2D< float >	_tex_blueNoise : register(t3);

struct DebugHBILData
{
	float4 reconstructedWorldSpacePosition;
	float4 normalAtW;
	float4 recomputedNormal;
	float4 bentNormalAtW;

	float4 localCameraDirectionUp;
	float4 localCameraDirectionRight;
	float4 localCameraDirectionAt;

	float4 globalCameraDirectionUp;
	float4 globalCameraDirectionRight;
	float4 globalCameraDirectionAt;
};

RWStructuredBuffer<DebugHBILData> _debug_hbil : register(u0);

cbuffer CB_HBIL : register( b3 ) {
	float4	_bilateralValues;
	float	_gatherSphereMaxRadius_m;		// Radius of the sphere that will gather our irradiance samples (in meters)
	float	_gatherSphereMaxRadius_p;		// Radius of the sphere that will gather our irradiance samples (in pixels)
	float	_temporalAttenuationFactor;		// Attenuation factor of radiance from previous frame
};

//float4	VS( float4 __Position : SV_POSITION ) : SV_POSITION { return __Position; }

////////////////////////////////////////////////////////////////////////////////
// Implement the methods expected by the HBIL header

float LinearizeDepth(float z, float nearZ, float farZ)
{
    return (nearZ * farZ) / (farZ - z * (farZ - nearZ));
}

float RemapFloat(float value, float inMin, float inMax, float outMin, float outMax)
{
    float normalized = (value - inMin) / (inMax - inMin);
    normalized = saturate(normalized);
    return outMin + normalized * (outMax - outMin);
}

//The below code expects the returned Z values to be in "meters"
//By the simple multiplication of the depth buffer with a scalar it is evident that the 
// depth buffer is linear 
float	FetchDepth( float2 _pixelPosition, float _mipLevel ) {

    float depthRaw = _tex_depth.SampleLevel(LinearClamp, _pixelPosition / (_resolution), _mipLevel);
    float linearDepth = LinearizeDepth(depthRaw, _ZNearFar_Q_Z.y, _ZNearFar_Q_Z.z);
    return linearDepth;
    //float normalizedDepth = RemapFloat(linearDepth,  _ZNearFar_Q_Z.x, _ZNearFar_Q_Z.y, 0, 1);
    //return _ZNearFar_Q_Z.y * normalizedDepth;
}

float3	FetchNormal( float2 _pixelPosition, float _mipLevel ) {
    float3 normal =  _tex_normal.SampleLevel(LinearClamp, _pixelPosition / (_resolution), _mipLevel);
    normal.z *= -1;
    return normal;
}

float3	FetchRadiance( float2 _pixelPosition, float _mipLevel ) {
    return _tex_sourceRadiance.SampleLevel(LinearClamp, _pixelPosition / (_resolution), _mipLevel).
    xyz;
}

// This clearly doesn't work: nasty silhouettes show up around objects
float	ComputeMipLevel_Depth( float _radius_pixels, float _stepSize_pixels ) {
//return _debugMipIndex;
	float	pixelArea = PI / (2.0 * MAX_ANGLES) * 2.0 * _radius_pixels * _stepSize_pixels;
	return 0.5 * log2( pixelArea ) * _bilateralValues.y;
//	return 0.5 * log2( pixelArea ) * float2( 0, 2 );	// Unfortunately, sampling lower mips for depth gives very nasty halos! Maybe use max depth? Meh. Not conclusive either...
//	return 1.5 * 0.5 * log2( pixelArea );
//	return 0.5 * log2( pixelArea );
}

float	ComputeMipLevel_Radiance( float2 _ssPosition, float _centralZ, float _currentZ, float _radius_meters ) {
	float	deltaZ = _centralZ - _currentZ;
	float	distance = sqrt( _radius_meters*_radius_meters + deltaZ*deltaZ );	// Distance from origin
	float	sphereRadius_meters = (0.5 * PI / MAX_ANGLES) * distance;			// Radius of the sphere (in meters) that will serve as footprint for mip computation

	// Estimate how many pixels a disc
	float	screenSize_m = 2.0 * TAN_HALF_FOV * _currentZ;						// Size (in meters) covered by the entire screen at current Z
	float	meters2Pixels = _resolution.y / screenSize_m;
	float	sphereRadius_pixels = meters2Pixels * sphereRadius_meters;			// Radius of the sphere (in pixels)

//	return _bilateralValues.y * log2( sphereRadius_pixels );
	return 1.5 * log2( sphereRadius_pixels );	// Empirical

//	float	pixelArea = PI * sphereRadius_pixels * sphereRadius_pixels;			// Area of the disc covered by the sphere (in square pixels)
//	return _bilateralValues.y * 0.5 * log2( pixelArea );
}

float2	BilateralFilterDepth( float2 _ssCentralPosition, float _centralZ, float3 _lcsCentralNormal, float2 _ssCurrentPosition, float _currentZ, float _radius_meters, float _horizonCosTheta, float _newCosTheta ) {

	// Rebuild camera-space positions & normals
	float2	UV0 = _ssCentralPosition / _resolution;
	float3	csView0 = BuildCameraRay( UV0 );
	float3	csPos0 = _centralZ * csView0;
			csView0 = normalize( csView0 );
	float3	csAt0 = -csView0;
	float3	csRight0 = normalize( cross( csAt0, float3( 0, 1, 0 ) ) );
	float3	csUp0 = cross( csRight0, csAt0 );
	float3	csNormal0 = _lcsCentralNormal.x * csRight0 + _lcsCentralNormal.y * csUp0 + _lcsCentralNormal.z * csAt0;

	float2	UV1 = _ssCurrentPosition / _resolution;
	float3	csView1 = BuildCameraRay( UV1 );
	float3	csPos1 = _currentZ * csView1;
			csView1 = normalize( csView1 );
	float3	csAt1 = -csView1;
	float3	csRight1 = normalize( cross( csAt1, float3( 0, 1, 0 ) ) );
	float3	csUp1 = cross( csRight1, csAt1 );
	float3	lcsCurrentNormal = FetchNormal( _ssCurrentPosition, 0.0 );
	float3	csNormal1 = lcsCurrentNormal.x * csRight1 + lcsCurrentNormal.y * csUp1 + lcsCurrentNormal.z * csAt1;

	// Our criterion is that current position and normal must see our central position to contribute...
	float3	csToCentralPosition = csPos0 - csPos1;
	float	distance2CentralPosition = length( csToCentralPosition );
			csToCentralPosition /= distance2CentralPosition;

	const float2	toleranceMin = float2( -0.02, -0.04 );
	const float2	toleranceMax = float2( -0.4, -0.8 );
	float	verticality = smoothstep( 0.5, 0.9, saturate( -dot( csToCentralPosition, csNormal0 ) ) );
	float2	tolerance = lerp( toleranceMin, toleranceMax, verticality );	// We grow more tolerant for very vertical pixels

//tolerance = toleranceMax;

//float	fade = dot( csToCentralPosition, csNormal1 );
//	float	fade = lerp( _bilateralValues.x, 1.0, smoothstep( -0.01, -0.0, dot( csToCentralPosition, csNormal1 ) ) );
	float	fade = smoothstep( tolerance.y, tolerance.x, dot( csToCentralPosition, csNormal1 ) );

	// To avoid sudden radiance jumps we use a dot product
	#if 0
		float	radianceFade = saturate( dot( csToCentralPosition, csNormal1 ) );
//				radianceFade = pow( radianceFade, 0.5 );
				radianceFade = fastSqrtNR0( radianceFade );					// Not full dot product, sqrt( dot( ) ) actually! (smoother result)
				radianceFade = 1.0
							 - saturate( 0.5 * distance2CentralPosition )	// Dot product fade out is fully effective after 2 meters
							 * (1.0 - radianceFade);
	#else
		float	radianceFade = 1.0
							 - saturate( 0.5 * distance2CentralPosition )	// Dot product fade out is fully effective after 2 meters
							 * pow2( 1.0 - saturate( dot( csToCentralPosition, csNormal1 ) ) );
//radianceFade = 1-saturate( 0.5 * distance2CentralPosition );
//radianceFade = pow( saturate( dot( csToCentralPosition, csNormal1 ) ), 0.5 );
	#endif

//radianceFade = 1;

	return float2( fade, radianceFade );
}

float3	SampleIrradiance_TEMP( float2 _ssCentralPosition, float2 _ssPosition, float2 _ssStep, float _radius_meters, float2 _sinCosGamma, float _centralZ, float3 _csCentralNormal, float _mipLevelDepth, float2 _integralFactors, inout float _radianceSamplingWeight, inout float3 _previousRadiance, inout float _maxCosTheta ) {

	// Read new Z and compute new horizon angle candidate
	float	Z = FetchDepth( _ssPosition, _mipLevelDepth );
	float	deltaZ = _centralZ - Z;																// Z difference, in meters
	float	recHypo = rsqrt( _radius_meters*_radius_meters + deltaZ*deltaZ );					// 1 / sqrt( z� + r� )
	float	cosTheta = (_sinCosGamma.x * _radius_meters + _sinCosGamma.y * deltaZ) * recHypo;	// cos(theta) = [sin(gamma)*r + cos(gamma)*z] / sqrt( z� + r� )

	// Filter outlier horizon values
//float	previousZ = 0.0;	// NEEDED?
	float2	bilateralWeights = BilateralFilterDepth( _ssCentralPosition, _centralZ, _csCentralNormal, _ssPosition, Z, _radius_meters, _maxCosTheta, cosTheta );
	_radianceSamplingWeight *= bilateralWeights.x;
//	cosTheta = lerp( -1.0, cosTheta, bilateralWeight );	// Flatten if rejected

	// Update any rising horizon
	if ( cosTheta <= _maxCosTheta )
		return 0.0;	// Below the horizon... No visible contribution.

	#if SAMPLE_NEIGHBOR_RADIANCE
		float	mipLevel_Radiance = ComputeMipLevel_Radiance( _ssPosition, _centralZ, Z, _radius_meters );
//		_previousRadiance = FetchRadiance( _ssPosition, mipLevel_Radiance );
//		_previousRadiance = FetchRadiance( _ssPosition - _bilateralValues.w * _ssStep, 0.0 );
		if ( _radianceSamplingWeight > 0.0 ) {
//			_previousRadiance = lerp( _previousRadiance, FetchRadiance( _ssPosition, 0.0 ), _radianceSamplingWeight );	// Accept new radiance depending on accumulated sampling weight...
			float3	newRadiance = bilateralWeights.y * FetchRadiance( _ssPosition, 0.0 );
			_previousRadiance = lerp( _previousRadiance, newRadiance, _radianceSamplingWeight );	// Accept new radiance depending on accumulated sampling weight...
		}
	#endif

	// Integrate over horizon difference (always from smallest to largest angle otherwise we get negative results!)
	float3	incomingRadiance = _previousRadiance * IntegrateSolidAngle( _integralFactors, cosTheta, _maxCosTheta );

// #TODO: Integrate with linear interpolation of irradiance as well??
// #TODO: Integrate with Fresnel F0!

	_maxCosTheta = cosTheta;	// Register a new positive horizon
//	_previousDeltaZ = deltaZ;		// Accept new depth difference

	return incomingRadiance;
}

float3	GatherIrradiance_TEMP( float2 _ssPosition, float2 _ssStep, float2 _ssMaxPosition, float2 _csDirection, float3 _csNormal, float _noise, float2 _sinCosGamma, float _stepSize_meters, uint _stepsCount, float _centralZ, float3 _centralRadiance, out float3 _csBentNormal, out float _AO, inout float4 _DEBUG ) {

	// Pre-compute factors for the integrals
	float2	integralFactors_Front = ComputeIntegralFactors( _csDirection, _csNormal );
//	float2	integralFactors_Back = ComputeIntegralFactors( -_csDirection, _csNormal );
	float2	integralFactors_Back = float2( -integralFactors_Front.x, integralFactors_Front.y );

	// Compute initial cos(angle) for front & back horizons
	// We do that by projecting the camera-space direction csDirection onto the tangent plane given by the normal
	//	then the cosine of the angle from the Z axis is simply given by the Pythagorean theorem:
	//                             P
	//			   N\  |Z		  -*-
	//				 \ |	  ---  ^
	//				  \|  ---      |
	//             --- *..........>+ csDirection
	//        --- 
	//
	float	hitDistance_Front = -dot( _csDirection, _csNormal.xy ) / _csNormal.z;
	float	planeCosTheta_Front = hitDistance_Front / sqrt( hitDistance_Front*hitDistance_Front + 1.0 );	// Assuming length(_csDirection) == 1
	float	planeCosTheta_Back = -planeCosTheta_Front;	// Back cosine is simply the mirror value

// Show horizon effect ON/OFF
//planeCosTheta_Front = -1.0;
//planeCosTheta_Back = -1.0;

	// Gather irradiance from front & back directions while updating the horizon angles at the same time
	float3	sumRadiance = 0.0;
	float3	previousRadiance_Front = _centralRadiance;
	float3	previousRadianceBack = _centralRadiance;
//*
	float	radius_meters = 0.0;
//float	radius_pixels = 0.0;
//float	stepSize_pixels = length(_ssStep);
	float2	ssPosition_Front = _ssPosition - _noise * _ssStep;
	float2	ssPosition_Back = _ssPosition + _noise * _ssStep;
	float	maxCosTheta_Front = planeCosTheta_Front;
	float	maxCosTheta_Back = planeCosTheta_Back;
	float	radianceSamplingWeight_Front = 1.0;
	float	radianceSamplingWeight_Back = 1.0;
	[loop]
	for ( uint stepIndex=0; stepIndex < _stepsCount; stepIndex++ ) {
		radius_meters += _stepSize_meters;
		ssPosition_Front += _ssStep;
		ssPosition_Back -= _ssStep;

//radius_pixels += stepSize_pixels;
//float	mipLevel = ComputeMipLevel_Depth( radius_pixels, stepSize_pixels );
float	mipLevel = 0.0;

		sumRadiance += SampleIrradiance_TEMP( _ssPosition, ssPosition_Front, _ssStep, radius_meters, _sinCosGamma, _centralZ, _csNormal, mipLevel, integralFactors_Front, radianceSamplingWeight_Front, previousRadiance_Front, maxCosTheta_Front );
		sumRadiance += SampleIrradiance_TEMP( _ssPosition, ssPosition_Back, -_ssStep, radius_meters, float2( -_sinCosGamma.x, _sinCosGamma.y ), _centralZ, _csNormal, mipLevel, integralFactors_Back, radianceSamplingWeight_Back, previousRadianceBack, maxCosTheta_Back );
	}
//*/

	// Accumulate bent normal direction by rebuilding and averaging the front & back horizon vectors
	_csBentNormal = IntegrateNormal( _csDirection, _csNormal, maxCosTheta_Back, maxCosTheta_Front );

	// DON'T NORMALIZE THE RESULT NOW OR WE GET BIAS!
//	_csBentNormal = normalize( _csBentNormal );

	#if USE_NORMAL_INFLUENCE_FOR_AO
		// Compute AO for this slice. Interval is regular [0,1]
//		_AO = IntegrateSolidAngle( integralFactors_Front, 1.0, maxCosTheta_Front )
//			+ IntegrateSolidAngle( integralFactors_Back, 1.0, maxCosTheta_Back );

		// For our special case, theta0=0
		_AO = integralFactors_Front.x * (FastPosAcos( maxCosTheta_Front ) - maxCosTheta_Front * sqrt( 1.0 - maxCosTheta_Front*maxCosTheta_Front )) + integralFactors_Front.y * (1.0 - maxCosTheta_Front*maxCosTheta_Front)
			+ integralFactors_Back.x * (FastPosAcos( maxCosTheta_Back ) - maxCosTheta_Back * sqrt( 1.0 - maxCosTheta_Back*maxCosTheta_Back )) + integralFactors_Back.y * (1.0 - maxCosTheta_Back*maxCosTheta_Back);
	#else
		// Compute AO for this slice (in [0,2]!!)
		_AO = 2.0 - maxCosTheta_Back - maxCosTheta_Front;
	#endif


_DEBUG = float4( _csBentNormal, 0 );


	return sumRadiance;
}


struct PSInput
{
    float4 position : SV_POSITION;
    float4 color : COLOR;
};
////////////////////////////////////////////////////////////////////////////////
// Computes bent cone & irradiance gathering from pixel's surroundings
struct PS_OUT {
	float4	irradiance : SV_TARGET0;
	float4	bentCone : SV_TARGET1;
};


//PS_OUT	main(PSInput input) {
//    float4 __Position = input.position;
//    float2 sourceResolution = _resolution;
//   float2 targetResolutoin = _resolution;
//
//	
//    PS_OUT debugEarlyOut;
//
//	float2	UV = __Position.xy / _resolution;
//	uint2	pixelPosition = uint2( floor( __Position.xy ) );
//
//    //float2 UV = float2(0.5f, 0.5f);
//	//uint2	pixelPosition = uint2( floor( __Position.xy ) );
//	//uint2	pixelPosition = uint2(_resolution.x*UV.x,_resolution.y*UV.y);
//	float	noise = 1;	// ACTUAL GOOD VALUE!
//	float	noise2 = 1;
//
//	
//	// Setup camera ray
//	float3	csView = BuildCameraRay( UV );
//	float	Z2Distance = length( csView );
//			csView /= Z2Distance;
//	float3	wsView = mul( float4( csView, 0.0 ), _camera2World ).xyz;
//
//	// Read back depth, normal & central radiance value from last frame
//    //float Z = Z_FAR * _tex_depth[pixelPosition];
//    float Z = FetchDepth(pixelPosition,0);
//
//			Z -= 1e-2;	// !IMPORTANT! Prevent acnea by offseting the central depth a tiny bit closer
//
//    float3 centralRadiance = FetchRadiance(pixelPosition,0); // Read back last frame's radiance value that we can use as a fallback for neighbor areas
//
//	// Compute local camera-space
//	float3	wsPos = _camera2World[3].xyz + Z * Z2Distance * wsView;
//	float3	wsRight = normalize( cross( wsView, _camera2World[1].xyz ) );
//	float3	wsUp = cross( wsRight, wsView );
//	float3	wsAt = -wsView;
//
//
//	// Express local camera-space vectors in global camera-space
//	float3	gcsRight = float3( dot( wsRight, _camera2World[0].xyz ), dot( wsRight, _camera2World[1].xyz ), dot( wsRight, _camera2World[2].xyz ) );
//	float3	gcsUp = float3( dot( wsUp, _camera2World[0].xyz ), dot( wsUp, _camera2World[1].xyz ), dot( wsUp, _camera2World[2].xyz ) );
//
//
//
//
//
//	float2	sinCosGamma;
//
//    float3 csNormal = FetchNormal(pixelPosition, 0);
//			csNormal.z = max( 1e-3, csNormal.z );	// Make sure it's never 0!
//    csNormal = mul(float4(csNormal, 0), _world2Camera);
//
//	
//    float eps=0.0005f;
//    if (abs(UV.x - 0.5f) < eps && abs(UV.y - 0.5f) < eps)
//    {
//        _debug_hbil[0].reconstructedWorldSpacePosition = float4(wsPos, 1);
//	
//        _debug_hbil[0].localCameraDirectionRight = float4(wsRight, 0);
//        _debug_hbil[0].localCameraDirectionAt = float4(wsAt, 0);
//        _debug_hbil[0].localCameraDirectionUp = float4(wsUp, 0);
//
//        _debug_hbil[0].globalCameraDirectionRight = float4(gcsRight, 0);
//        _debug_hbil[0].globalCameraDirectionAt = float4(0, 0, 0, 0);
//        _debug_hbil[0].globalCameraDirectionUp = float4(gcsUp, 0);
//	
//		
//    }
//
//	
//    debugEarlyOut.irradiance = float4(csNormal,0);
//	debugEarlyOut.bentCone = float4(wsAt,0);
//    //Out.irradiance = float4(0, 0, 0, 1);;
//
//	return debugEarlyOut;
//}



PS_OUT	main(PSInput input) {
    float4 __Position = input.position;
    float2 sourceResolution = _resolution;
    float2 targetResolutoin = _resolution;

	
    PS_OUT debugEarlyOut;
//PS_OUT	PS( float4 __Position : SV_POSITION ) {
	float2	UV = __Position.xy / _resolution;
	uint2	pixelPosition = uint2( floor( __Position.xy ) );
	//float	noise = frac( _tex_blueNoise[pixelPosition & 0x3F] + SQRT2 * _framesCount );	// ACTUAL GOOD VALUE!
	//float	noise2 = frac( _time + _tex_blueNoise[uint2( float2( 32659.167 * UV.x, 173227.3 * UV.y ) ) & 0x3F] );
	float	noise = 1;	// ACTUAL GOOD VALUE!
	float	noise2 = 1;

	// Setup camera ray
	float3	csView = BuildCameraRay( UV );
	float	Z2Distance = length( csView );
			csView /= Z2Distance;
	float3	wsView = mul( float4( csView, 0.0 ), _camera2World ).xyz;

	// Read back depth, normal & central radiance value from last frame
    //float Z = Z_FAR * _tex_depth[pixelPosition];
    float Z = FetchDepth(pixelPosition,0);

			Z -= 1e-2;	// !IMPORTANT! Prevent acnea by offseting the central depth a tiny bit closer

    float3 centralRadiance = FetchRadiance(pixelPosition,0); // Read back last frame's radiance value that we can use as a fallback for neighbor areas

	// Compute local camera-space
	float3	wsPos = _camera2World[3].xyz + Z * Z2Distance * wsView;
	float3	wsRight = normalize( cross( wsView, _camera2World[1].xyz ) );
	float3	wsUp = cross( wsRight, wsView );
	float3	wsAt = -wsView;


	// Express local camera-space vectors in global camera-space
	float3	gcsRight = float3( dot( wsRight, _camera2World[0].xyz ), dot( wsRight, _camera2World[1].xyz ), dot( wsRight, _camera2World[2].xyz ) );
	float3	gcsUp = float3( dot( wsUp, _camera2World[0].xyz ), dot( wsUp, _camera2World[1].xyz ), dot( wsUp, _camera2World[2].xyz ) );

    float eps=0.0005f;
	
	
	float2	sinCosGamma;
    float3 wsSurfaceNormal = FetchNormal(pixelPosition, 0);
    float3 csNormal;
    csNormal.x = dot(wsSurfaceNormal, wsRight);
    csNormal.y = dot(wsSurfaceNormal, wsUp);
    csNormal.z = dot(wsSurfaceNormal, wsAt);
			//csNormal.z = max( 1e-3, csNormal.z );	// Make sure it's never 0!

	// Compute screen radius of gather sphere
	float	screenSize_m = 2.0 * Z * TAN_HALF_FOV;																	// Vertical size of the screen in meters when extended to distance Z
	float	meter2Pixel = _resolution.y / screenSize_m;																// Gives us the conversion factor to go from meters to pixels
	float	sphereRadius_pixels = meter2Pixel * _gatherSphereMaxRadius_m;
			sphereRadius_pixels = min( _gatherSphereMaxRadius_p, sphereRadius_pixels );								// Prevent it to grow larger than our fixed limit
	float	radiusStepSize_pixels = max( 1.0, sphereRadius_pixels / MAX_SAMPLES );									// This gives us our radial step size in pixels
	uint	samplesCount = clamp( uint( ceil( sphereRadius_pixels / radiusStepSize_pixels ) ), 1, MAX_SAMPLES );	// Reduce samples count if possible
//	float	radiusStepSize_meters = sphereRadius_pixels / (samplesCount * meter2Pixel);								// This gives us our radial step size in meters
	float	radiusStepSize_meters = radiusStepSize_pixels / (csView.z * meter2Pixel);								// This gives us our radial step size in meters

	// Start gathering radiance and bent normal by subdividing the screen-space disk around our pixel into N slices
	float4	GATHER_DEBUG = 0.0;
	float3	sumIrradiance = 0.0;
	float	sumAO = 0.0;
	float3	csAverageBentNormal = 0.0;
	float	averageAO = 0.0;
	float	varianceAO = 0.0;
//	float	phiNoise = Bayer1D_16( _framesCount ) / 16.0f;
	float	phiNoise = 2.6457513110645905905016157536393 * _framesCount;
//phiNoise = 0.0;
//phiNoise = noise;
//noise = 0.0;

#if MAX_ANGLES > 1
	[loop]
	for ( uint angleIndex=0; angleIndex < MAX_ANGLES; angleIndex++ )
#else
	uint	angleIndex = 0;
#endif
	{
		float	phi = (angleIndex + phiNoise) * PI / MAX_ANGLES;

		// Build camera-space and screen-space walk directions
		float2	csDirection;
		sincos( phi, csDirection.y, csDirection.x );

		float3	gcsDirection = csDirection.x * gcsRight + csDirection.y * gcsUp;	// Since csDirection gives us the components along local camera-space's right and up vectors, and we know their expression in global camera-space, it's easy find the equivalent global camera-space direction...

		float2	ssDirection = normalize( gcsDirection.xy );							// We normalize since we want to take integer pixel steps
				ssDirection.y = -ssDirection.y;										// Going upward in camera space means going downward in screen space...
				ssDirection *= radiusStepSize_pixels;								// Scale by our step size

		// Build angular correction based on deviation from central camera axis
		sinCosGamma.x = gcsDirection.z;
		sinCosGamma.y = sqrt( 1.0 - sinCosGamma.x*sinCosGamma.x );
		sinCosGamma *= 0.99;	// Needed otherwise sometimes the cos(theta) is outside the [-1,1] range and acos gives a NaN!

		// Gather irradiance and average cone direction for that slice
		float3	csBentNormal;
		float	AO;
		sumIrradiance += GatherIrradiance_TEMP( __Position.xy, ssDirection, _resolution, csDirection, csNormal, noise, sinCosGamma, radiusStepSize_meters, samplesCount, Z, centralRadiance, csBentNormal, AO, GATHER_DEBUG );
		csAverageBentNormal += csBentNormal;
		sumAO += AO;

		// We're using running variance computation from https://www.johndcook.com/blog/standard_deviation/
		//	Avg(N) = Avg(N-1) + [V(N) - Avg(N-1)] / N
		//	S(N) = S(N-1) + [V(N) - Avg(N-1)] * [V(N) - Avg(N)]
		// And variance = S(finalN) / (finalN-1)
		//
		float	previousAverageAO = averageAO;
		averageAO += (AO - averageAO) / (1+angleIndex);
		varianceAO += (AO - previousAverageAO) * (AO - averageAO);
	}

	// Finalize bent cone
	csAverageBentNormal = normalize( csAverageBentNormal );

	// Use AO to compute cone angle
	#if USE_NORMAL_INFLUENCE_FOR_AO
		sumAO /= MAX_ANGLES;		// Normalize
	#else
		sumAO /= 2.0 * MAX_ANGLES;	// Normalize (remember here that the returned AO is in [0,2] range for each slice)
	#endif

	float	cosAverageConeAngle = 1.0 - sumAO;

	#if MAX_ANGLES > 1
		varianceAO /= MAX_ANGLES;
  #endif
//	float	stdDeviation = PI * sqrt( varianceAO );		// Technically in [0,2PI]
	float	stdDeviation = 0.5 * sqrt( varianceAO );	// Now AO standard deviation in [0,1]

//stdDeviation = 0.0;

	// Finalize irradiance
	sumIrradiance *= PI / MAX_ANGLES;
	sumIrradiance = max( 0.0, sumIrradiance );

	// Write result
	PS_OUT	Out;
	Out.irradiance = float4( sumIrradiance, 0 );


// Debugging the RGB10A2 and R11G11B10_FLOAT formats
//Out.irradiance = float4( 0, 0, 0, 0.75 / 4.0 );
//Out.irradiance = float4( 0, 0, 0, 1.999 / 3.0 );
//Out.irradiance = float4( 1, 0, 0, 1 );



	// [18/02/13] RGBA8_SNORM requires some encoding
//	Out.bentCone = float4( max( 0.01, cosAverageConeAngle ) * csAverageBentNormal, stdDeviation );
	Out.bentCone = float4( max( 0.01, sqrt( cosAverageConeAngle ) ) * csAverageBentNormal, stdDeviation );

//const float	MIN_ENCODABLE_VALUE = 1.0 / 128.0;
//csAverageBentNormal = sqrt( max( MIN_ENCODABLE_VALUE, cosAverageConeAngle ) ) * csAverageBentNormal;
//csAverageBentNormal = csNormal;
//Out.bentCone = float4( csAverageBentNormal, stdDeviation );


//////////////////////////////////////////////
// WRITE DEBUG VALUE INTO BENT CONE BUFFER
float3	DEBUG_VALUE = float3( 1,0,1 );
DEBUG_VALUE = csNormal;
DEBUG_VALUE = csNormal.x * wsRight + csNormal.y * wsUp + csNormal.z * wsAt;	// World-space normal
    //float3 worldDebugNormal = mul(float4(csNormal, 0), _camera2World);
 DEBUG_VALUE = mul(float4(csNormal, 0), _camera2World); // World-space normal
DEBUG_VALUE = csAverageBentNormal;
    //DEBUG_VALUE = mul(float4(csAverageBentNormal, 0), _camera2World); // World-space normal
DEBUG_VALUE = csAverageBentNormal.x * wsRight + csAverageBentNormal.y * wsUp + csAverageBentNormal.z * wsAt;	// World-space normal
//DEBUG_VALUE = cosAverageConeAngle;
//DEBUG_VALUE = dot( ssAverageBentNormal, N );
//DEBUG_VALUE = 0.01 * Z;
//DEBUG_VALUE = sphereRadius_pixels / _gatherSphereMaxRadius_p;
//DEBUG_VALUE = 0.1 * (radiusStepSize_pixels-1);
//DEBUG_VALUE = 0.5 * float(samplesCount) / MAX_SAMPLES;
//DEBUG_VALUE = varianceConeAngle;
//DEBUG_VALUE = stdDeviation;
//DEBUG_VALUE = float3( GATHER_DEBUG.xy, 0 );
//DEBUG_VALUE = float3( GATHER_DEBUG.zw, 0 );
//DEBUG_VALUE = 0.4 * localCamera2World[3];
//DEBUG_VALUE = GATHER_DEBUG.xyz;
    //DEBUG_VALUE = worldDebugNormal;
    //Out.bentCone = float4( DEBUG_VALUE, 1 );
//
//////////////////////////////////////////////
    if (abs(UV.x - 0.5f) < eps && abs(UV.y - 0.5f) < eps)
    {
        _debug_hbil[0].reconstructedWorldSpacePosition = float4(wsPos, 1);
        _debug_hbil[0].normalAtW = float4(FetchNormal(pixelPosition, 0), 1);
        _debug_hbil[0].recomputedNormal = float4(csNormal.x * wsRight + csNormal.y * wsUp + csNormal.z * wsAt, 0);
        _debug_hbil[0].bentNormalAtW = float4(csAverageBentNormal.x * wsRight + csAverageBentNormal.y * wsUp + csAverageBentNormal.z * wsAt, 0);
	
        _debug_hbil[0].localCameraDirectionRight = float4(wsRight, 0);
        _debug_hbil[0].localCameraDirectionAt = float4(wsAt, 0);
        _debug_hbil[0].localCameraDirectionUp = float4(wsUp, 0);

        _debug_hbil[0].globalCameraDirectionRight = float4(gcsRight, 0);
        _debug_hbil[0].globalCameraDirectionAt = float4(0, 0, 0, 0);
        _debug_hbil[0].globalCameraDirectionUp = float4(gcsUp, 0);
    }
    Out.irradiance = float4(FetchNormal(pixelPosition, 0), 1);
    Out.bentCone = float4(csNormal.x * wsRight + csNormal.y * wsUp + csNormal.z * wsAt, 1);

	
    Out.irradiance = Out.irradiance * 0.5f +0.5f;
    Out.bentCone = Out.bentCone * 0.5f +0.5f;

	return Out;
}
