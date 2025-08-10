#pragma once

#include <d3d12.h>
#include <dxcapi.h>

#include <vector>
#include <initializer_list>

#include "RootSignature.h"
#include "Math/Vector.h"
#include "GraphicsCore.h"
#include "BufferManager.h"
#include "ReadbackBuffer.h"
#include "Camera.h"
#include "UniformGrid.h"
#include "CommandContext.h"
#include "TextureManager.h"

#include "ExtendedUtility.h"

#include "ModelH3D.h"

#include "CompiledShaders/ComputeHBIL_BruteForce.h"

//All the extra buffers that are needed 

	
 __declspec(align(16)) struct HBIL_MAIN {
	XMFLOAT2		_resolution;	// viewport resolution (in pixels)
	float		_time;
	float		_deltaTime;

	XMFLOAT4		_debugValues;

	XMFLOAT4		_mouseUVs;		// XY=Current Mouse UV, ZW=Referenced Mouse UV (set when alt+clicking the screen)

	UINT		_flags;
	UINT		_framesCount;
	UINT		_debugMipIndex;
	float		_environmentIntensity;

	float		_sunIntensity;
	float		_forcedAlbedo;
	float		_coneAngleBias;
	float		_exposure;
};

__declspec(align(16)) struct CB_Camera {
	XMMATRIX	_camera2World;
	XMMATRIX	_world2Camera;
	XMMATRIX	_proj2World;
	XMMATRIX	_world2Proj;
	XMMATRIX	_camera2Proj;
	XMMATRIX	_proj2Camera;

	XMFLOAT4	_ZNearFar_Q_Z;			// XY=Near/Far Clip, Z=Q=Zf/(Zf-Zn), W=0
	XMFLOAT4	_cameraSubPixelOffset;	// XY=Un-jitter vector, ZW=sub-pixel jitter offset

	// Previous frame matrices
	XMMATRIX	_previousWorld2Proj;
	XMMATRIX	_previoucCamera2CurrentCamera;
	XMMATRIX	_currentCamera2PrevioucCamera;
};

__declspec(align(16)) struct CBSH {
	XMFLOAT4 _SH[9];
};

__declspec(align(16)) struct CB_HBIL {
	XMFLOAT4	_bilateralValues;
	float	_gatherSphereMaxRadius_m;		// Radius of the sphere that will gather our irradiance samples (in meters)
	float	_gatherSphereMaxRadius_p;		// Radius of the sphere that will gather our irradiance samples (in pixels)
	float	_temporalAttenuationFactor;		// Attenuation factor of radiance from previous frame
};

	//All the data would be collected for a point at the exact center of the screen
	//U:0.5 V:0.5
	struct DebugHBILData {
		XMFLOAT4 reconstructedWorldSpacePosition;
		XMFLOAT4 normalAtW;
		XMFLOAT4 recomputedNormal;
		XMFLOAT4 bentNormalAtW;
		XMFLOAT4 perSliceAngle;

		XMFLOAT4 localCameraDirectionUp;
		XMFLOAT4 localCameraDirectionRight;
		XMFLOAT4 localCameraDirectionAt;

		XMFLOAT4 globalCameraDirectionUp;
		XMFLOAT4 globalCameraDirectionRight;
		XMFLOAT4 globalCameraDirectionAt;

		XMFLOAT4 maxAnglesInDirection;
		XMFLOAT4 ssSamplesOfMaxAngles;
		XMFLOAT4 wsSampleFront;
		XMFLOAT4 wsSampleBack;


	};


class HBIL
{
protected:
	bool m_debugReadingEnabled = true;
	int framesCount = 0;

	HBIL_MAIN m_MainHBILCB; 
	CB_Camera m_HBILCameraCB;
	CBSH m_CBSH;

	//Hold the pointers to actual GBuffer
	GBufferPtrs m_GBuffer;
	ColorBuffer	m_DownsampledBuffers;

	GraphicsPSO m_HBILRenderPass = {(L"HBIL Render Pass PSO")};
	RootSignature m_HBILRenderRS;

	//The additional resource required to comptue HBIL based on the paper
	// •  Diffuse lighting (Fullscreen, RGB, HDR, All Mips), used to store the diffuse part of the lighting that is reprojected and re-injected next frame
	// •  Push/Pull-Buffer (Fullscreen, RGB, HDR, All Mips), serves as the 2nd ping-pong buffer (diffuse lighting buffer  being the first one) to compute the push/pull chain after reprojection (cf. Section 3.2).   
	ColorBuffer m_DiffuseLighting;
	ColorBuffer m_PushPullDiffuseLighting;

	// •  4x4 Split Radiance (1/4 Resolution, RGB, HDR, No Mip), temporary quarter-res buffers where the  reprojected/reconstructed radiance from last frame is stored (allocated as Texture2DArray)  
	// •  4x4 Split Normal (1/4 Resolution, R8G8_SNORM, No Mip), temporary quarter-res buffer where the G-Buffer  normal is stored as HBIL input (allocated as Texture2DArray)  
	// •  4x4 Split Depth (1/4 Resolution, R16F, No Mip), temporary quarter-res buffer where the G-Buffer depth is  stored as HBIL input (allocated as Texture2DArray)  
	
	ColorBuffer m_SplitRadiance;
	ColorBuffer m_SplitNormal;
	DepthBuffer m_SplitDepth;


 


	int m_debugAngles = 16;
	ByteAddressBuffer m_DebugHBIL;
	ReadbackBuffer m_DebugHBILReadback;
	ColorBuffer* m_inputBuffers;

	DescriptorHeap m_HBILHeap;
	TextureRef m_BlueNoiseTexture;

public:
	bool m_renderAtColorBuffer = false;
	//Ouput irradiance buffer
	ColorBuffer m_OutputIrradiance;
	ColorBuffer m_OutputBentCone;
	std::vector<DebugHBILData> m_DebugHBILActual;

	CB_HBIL m_HBILExtraCB;
	HBIL();

#pragma region Initialization
	virtual void Setup(GBufferPtrs gbuffer,ColorBuffer* downsampledGBuffers,GraphicsPSO quadPSO);

	virtual void CreateHBILPSO(GraphicsPSO quadRenderingPSO);

	virtual void CreateHBILRootSignatue(); 

	virtual void CreateHBILHeap(ColorBuffer* quarterResGBuffer);
#pragma endregion
	void ReadDebugHBIL(GraphicsContext& gfx,D3D12_RESOURCE_STATES endState ,bool flushImmediate = true);

	//Setups the camera constant buffer to be send to the HBIL algortihm
	//Make sure the coordinate system is LEFT HANDED !!! 
	void UpdateCameraCBufferLH(const Camera& camera, CB_Camera& cameraCBuffer);

	void RenderHBIL(GraphicsContext& gfx,const Camera& camera);
};

