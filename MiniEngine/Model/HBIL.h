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

#include "ExtendedUtility.h"

#include "ModelH3D.h"

#include "CompiledShaders/DownsampleCS.h"

//All the extra buffers that are needed 

__declspec(align(16)) struct DownsampleData {
	XMFLOAT2 InvSourceResolution; // 1.0 / (fullWidth, fullHeight)
};
	
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

__declspec(align(16)) struct CB_HBIL{
	XMFLOAT4	_bilateralValues;
	float	_gatherSphereMaxRadius_m;		// Radius of the sphere that will gather our irradiance samples (in meters)
	float	_gatherSphereMaxRadius_p;		// Radius of the sphere that will gather our irradiance samples (in pixels)
	float	_temporalAttenuationFactor;		// Attenuation factor of radiance from previous frame
};


class HBIL
{
	HBIL_MAIN m_MainHBILCB; 
	HBIL_MAIN m_HBILCameraCB;
	HBIL_MAIN m_HBILExtraCB;
	//Hold the pointers to actual GBuffer
	GBufferPtrs m_GBuffer;

	PSO m_HBILRenderPass = {(L"HBIL Render Pass PSO")};
	RootSignature m_HBILRenderRS;

	ComputePSO m_DownsamplePSO = {(L"Downsample CS PSO")};
	RootSignature m_DownsampleRS;
	DownsampleData m_DownsampleCB;


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

public:
	HBIL()
	{

	}
	void CreateDownsampledRootSignature()
	{
		//Todo initialize the samplers correctly
	  SamplerDesc LinearClamp;
	  LinearClamp.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	  LinearClamp.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	  LinearClamp.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	  LinearClamp.MinLOD = 0;

	  m_DownsampleRS.Reset(3, 1);
	  m_DownsampleRS.InitStaticSampler(0, LinearClamp);

	  m_DownsampleRS[0].InitAsConstantBuffer(0);
	  //The first SRV is the input texture in full resolution
	  m_DownsampleRS[1].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 3);
	  //The first UAV is the output texture in quarter resolution
	  m_DownsampleRS[2].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 0, 3);

	  m_DownsampleRS.Finalize(L"CS Downsampling Root Signature");
	}
	void CreateDownsamplePSO()
	{
	  m_DownsamplePSO.SetRootSignature(m_DownsampleRS);
	  m_DownsamplePSO.SetComputeShader(g_pDownsampleCS, sizeof(g_pDownsampleCS));
	  m_DownsamplePSO.Finalize();

	}
	void CreateHBILRootSignatue()
	{
		//Todo initialize the samplers correctly
	  SamplerDesc LinearWrap;
	  LinearWrap.MaxAnisotropy = 1;
	  LinearWrap.Filter = D3D12_FILTER_MAXIMUM_MIN_MAG_MIP_POINT;
	  SamplerDesc LinearPoint = LinearWrap;

	  m_DownsampleRS.Reset(5, 2);
	  //Todo check the correct positioning of the static samplers
	  m_DownsampleRS.InitStaticSampler(10, LinearPoint);
	  m_DownsampleRS.InitStaticSampler(11, LinearWrap);

	  //Initializing the constant buffer
	  //This CBs are for the HBIL's global utility file
	  m_DownsampleRS[0].InitAsConstantBuffer(0);
	  m_DownsampleRS[1].InitAsConstantBuffer(1);
	  m_DownsampleRS[2].InitAsConstantBuffer(2);
	  //This CBs are the HBIL's application files
	  m_DownsampleRS[3].InitAsConstantBuffer(3);
	  //Supplying The Textures
	  m_DownsampleRS[4].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 4);

	  m_DownsampleRS.Finalize(L"CS Downsampling Root Signature");

	}

	DescriptorHeap m_DownsampleHeap;
	ColorBuffer m_QuarterResDepth;
	ColorBuffer m_QuarterResDiffuse;
	ColorBuffer m_QuarterResNormal;
	void InitializeQuarterResBuffer()
	{
		auto d = m_GBuffer.g_Depth;
		m_QuarterResDepth.Create(L"Quarter Res Depth Buffer", d->GetWidth() / 4, d->GetHeight() / 4, 0, DXGI_FORMAT_R32_FLOAT);

		//TODO make sure of the formats require by the HBIL shader implementation

		m_QuarterResDiffuse.Create(L"Quarter Res Diffuse Buffer", d->GetWidth() / 4, d->GetHeight() / 4, 0, m_GBuffer.g_Color->GetFormat());
		m_QuarterResNormal.Create(L"Quarter Res Normal Buffer", d->GetWidth() / 4, d->GetHeight() / 4, 0, m_GBuffer.g_Normal->GetFormat());

	}
	void CreateDownsampleDescriptorHeap() {
	  m_DownsampleHeap.Create(L"DOWNSAMPLE INPUT OUTPUT HEAP", D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 6);
	  
	  ExtendedUtility::CopyDescriptorsToHeap(m_DownsampleHeap, {
		  m_GBuffer.g_Depth->GetDepthSRV(),
		  m_GBuffer.g_Color->GetSRV(),
		  m_GBuffer.g_Normal->GetSRV(),
		  m_QuarterResDepth.GetUAV(),
		  m_QuarterResDiffuse.GetUAV(),
		  m_QuarterResNormal.GetUAV()
		  }
	  );

	}



	void ComputeDownsampledTexture(ComputeContext& cfx) {

		ScopedTimer _prof(L"Downsampling Depth", cfx);

		cfx.SetPipelineState(m_DownsamplePSO);
		cfx.SetRootSignature(m_DownsampleRS);

		cfx.TransitionResource(*m_GBuffer.g_Color, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
		cfx.TransitionResource(*m_GBuffer.g_Depth, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
		cfx.TransitionResource(*m_GBuffer.g_Normal, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);



		DepthBuffer* d= m_GBuffer.g_Depth;

		//Assign the resolution to cb
		XMFLOAT2 invSourceResolution;
		invSourceResolution.x = 1.0f / d->GetWidth();
		invSourceResolution.y = 1.0f / d->GetWidth();
		m_DownsampleCB.InvSourceResolution = invSourceResolution;



		cfx.SetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, m_DownsampleHeap.GetHeapPointer());
		//Setup parameters
		cfx.SetDynamicConstantBufferView(0, sizeof(DownsampleData), &m_DownsampleCB);
		cfx.SetDescriptorTable(1,m_DownsampleHeap[0]);
		cfx.SetDescriptorTable(2,m_DownsampleHeap[3]);

		cfx.Dispatch2D(d->GetWidth(), d->GetHeight(), 8, 8);
	}

	void Setup(GBufferPtrs gbuffer)
	{
		this->m_GBuffer = gbuffer;

		InitializeQuarterResBuffer();

		CreateDownsampleDescriptorHeap();

		CreateDownsampledRootSignature();


		CreateDownsamplePSO();

	}


	void RenderHBIL()
	{
		
	}



};

