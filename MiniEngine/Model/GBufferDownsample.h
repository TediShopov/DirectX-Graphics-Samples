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

#include "CompiledShaders/DownsampleCS.h"
enum DownsampledGBuffer
	{
		DIFFUSE,DEPTH,NORMAL
	};

__declspec(align(16)) struct DownsampleData {
	Matrix4	 worldToCamera;
	XMFLOAT2 InvSourceResolution; // 1.0 / (fullWidth, fullHeight)
};

	class GBufferDownsample {
		//Hold the pointers to actual GBuffer
		GBufferPtrs m_GBuffer;

		ComputePSO m_DownsamplePSO = {(L"Downsample CS PSO")};
		RootSignature m_DownsampleRS;
		DownsampleData m_DownsampleCB;

		//The additional resource required to comptue HBIL based on the paper
		// •  Diffuse lighting (Fullscreen, RGB, HDR, All Mips), used to store the diffuse part of the lighting that is reprojected and re-injected next frame
		// •  Push/Pull-Buffer (Fullscreen, RGB, HDR, All Mips), serves as the 2nd ping-pong buffer (diffuse lighting buffer  being the first one) to compute the push/pull chain after reprojection (cf. Section 3.2).   
		ColorBuffer m_DiffuseLighting;
		ColorBuffer m_PushPullDiffuseLighting;

		DescriptorHeap m_DownsampleHeap;

		
		ColorBuffer m_DownsampledGBuffer[3];

	//	ColorBuffer m_QuarterResDepth;
	//	ColorBuffer m_QuarterResDiffuse;
	//	ColorBuffer m_QuarterResNormal;


	public:

	ColorBuffer*	GetDownsampledBufferPtr()
		{
			return m_DownsampledGBuffer;

		}

		GBufferDownsample(){}

		void CreateRootSignature()
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
		void CreatePSO()
		{
		  m_DownsamplePSO.SetRootSignature(m_DownsampleRS);
		  m_DownsamplePSO.SetComputeShader(g_pDownsampleCS, sizeof(g_pDownsampleCS));
		  m_DownsamplePSO.Finalize();

		}

		void InitializeQuarterResBuffer()
		{
			auto d = m_GBuffer.g_Depth;
			m_DownsampledGBuffer[DEPTH].Create(L"Quarter Res Depth Buffer", d->GetWidth() / 4, d->GetHeight() / 4, 0, DXGI_FORMAT_R32_FLOAT);
			m_DownsampledGBuffer[DIFFUSE].Create(L"Quarter Res Diffuse Buffer", d->GetWidth() / 4, d->GetHeight() / 4, 0, m_GBuffer.g_Diffuse->GetFormat());
			m_DownsampledGBuffer[NORMAL].Create(L"Quarter Res Normal Buffer", d->GetWidth() / 4, d->GetHeight() / 4, 0, m_GBuffer.g_Normal->GetFormat());
		}


		void CreateDescriptorHeap() {

		  m_DownsampleHeap.Create(L"DOWNSAMPLE INPUT OUTPUT HEAP", D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 6);
		  
		  ExtendedUtility::CopyDescriptorsToHeap(m_DownsampleHeap, {
			  m_GBuffer.g_Depth->GetDepthSRV(),
			  m_GBuffer.g_Diffuse->GetSRV(),
			  m_GBuffer.g_Normal->GetSRV(),
			  m_DownsampledGBuffer[DEPTH].GetUAV(),
			  m_DownsampledGBuffer[DIFFUSE].GetUAV(),
			  m_DownsampledGBuffer[NORMAL].GetUAV(),
			  }
		  );

		}

		void Dispatch(ComputeContext& cfx,const Camera& camera) {

			ScopedTimer _prof(L"Downsampling Depth", cfx);

			cfx.SetPipelineState(m_DownsamplePSO);
			cfx.SetRootSignature(m_DownsampleRS);

			cfx.TransitionResource(*m_GBuffer.g_Diffuse, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
			cfx.TransitionResource(*m_GBuffer.g_Depth, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
			cfx.TransitionResource(*m_GBuffer.g_Normal, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

			cfx.TransitionResource(m_DownsampledGBuffer[DEPTH], D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,true);
			cfx.TransitionResource(m_DownsampledGBuffer[DIFFUSE], D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,true);
			cfx.TransitionResource(m_DownsampledGBuffer[NORMAL], D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,true);

			DepthBuffer* d= m_GBuffer.g_Depth;

			//Assign the resolution to cb
			XMFLOAT2 invSourceResolution;
			invSourceResolution.x = 1.0f / d->GetWidth();
			invSourceResolution.y = 1.0f / d->GetHeight();

			m_DownsampleCB.InvSourceResolution = invSourceResolution;
			cfx.SetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, m_DownsampleHeap.GetHeapPointer());
			//Setup parameters
			cfx.SetDynamicConstantBufferView(0, sizeof(DownsampleData), &m_DownsampleCB);
			cfx.SetDescriptorTable(1,m_DownsampleHeap[0]);
			cfx.SetDescriptorTable(2,m_DownsampleHeap[3]);
			cfx.Dispatch2D(d->GetWidth(), d->GetHeight(), 8, 8);
		}

		bool m_debugReadingEnabled = true;
		

		void Setup(GBufferPtrs gbuffer,GraphicsPSO quadPSO)
		{
			this->m_GBuffer = gbuffer;
			InitializeQuarterResBuffer();
			CreateDescriptorHeap();
			CreateRootSignature();
			CreatePSO();
		}


	};

