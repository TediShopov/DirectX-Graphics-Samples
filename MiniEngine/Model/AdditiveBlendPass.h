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
#include "ExtendedUtility.h"
#include "ModelH3D.h"

#include "SurfelGI.h"
#include "CommandContext.h"
#include "Renderer.h"
#include "Math/Vector.h"
#include <limits>

#include "CompiledShaders/BlendPS.h"
using namespace DirectX;
using namespace Math;

class AdditiveBlendPass
{
public:
	GraphicsPSO m_PSO;
	RootSignature m_RootSignature;
	DescriptorHeap m_DescriptorHear;
	GBufferPtrs m_GBuffer;

	void Setup(GBufferPtrs gbuffer, ColorBuffer* rayTracingOutColor,
		const unsigned char* VS, UINT sizeVS,
		const unsigned char* PS , UINT sizePS
	
	) {
		m_GBuffer = gbuffer;
		DXGI_FORMAT ColorFormat = m_GBuffer.g_Color->GetFormat();
		DXGI_FORMAT NormalFormat = m_GBuffer.g_Normal->GetFormat();
		DXGI_FORMAT DepthFormat = m_GBuffer.g_Depth->GetFormat();
		DXGI_FORMAT formats[2] = { ColorFormat, NormalFormat };

		InitializeRootSignature();
		InitializePSO(VS,sizeVS,PS,sizePS);
		InitializeDescriptorHeap(
			rayTracingOutColor
		);

	}

	virtual void SetupRenderStage(GraphicsContext& gfxContext,
		const D3D12_VIEWPORT& viewport,
		const D3D12_RECT& scissor,
		ColorBuffer& blendTextureSource,
		ColorBuffer& blendTextureTarget) {

		gfxContext.SetPipelineState(m_PSO);
		gfxContext.SetRootSignature(m_RootSignature);
		gfxContext.SetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,m_DescriptorHear.GetHeapPointer());

		gfxContext.TransitionResource(blendTextureSource, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, true);
		gfxContext.TransitionResource(blendTextureTarget, D3D12_RESOURCE_STATE_RENDER_TARGET, true);

		//Possible disable the normal from this pass
		D3D12_CPU_DESCRIPTOR_HANDLE rtvs[]{ m_GBuffer.g_Color->GetRTV(), m_GBuffer.g_Normal->GetRTV() };
		gfxContext.SetRenderTargets(ARRAYSIZE(rtvs), rtvs);
		gfxContext.SetViewportAndScissor(viewport, scissor);
		gfxContext.SetDescriptorTable(1, m_DescriptorHear[0]);

		//RenderTriangleObject(gfxContext, camera.GetViewProjMatrix(), camera.GetPosition(), TestRenderer::kOpaque);

	}


protected:
	virtual void InitializePSO(
		const unsigned char* VS, UINT sizeVS,
		const unsigned char* PS , UINT sizePS
	) {


		//m_GBuffer.g_DepthPSO.SetVertexShader(g_pDepthViewerVS, sizeof(g_pDepthViewerVS));

		D3D12_INPUT_ELEMENT_DESC colorElem[] =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 16, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
		};

		//m_PSO = Depth;
		//--- REPLACE THE ROOT SIGNATURE
		DXGI_FORMAT formats[1] = { DXGI_FORMAT_R11G11B10_FLOAT };

		m_PSO = GraphicsPSO(L"Blend Texture PSO");
		m_PSO.SetRootSignature(m_RootSignature);
		m_PSO.SetRasterizerState(Graphics::RasterizerDefault);
		m_PSO.SetBlendState(Graphics::BlendAdditive);
		//Possible disable the depth state for this one
		m_PSO.SetDepthStencilState(Graphics::DepthStateDisabled);
		m_PSO.SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
		//m_PSO.SetRenderTargetFormats(2, formats, DepthFormat);
		//m_PSO.SetRenderTargetFormats(2, formats, depthFormat);
		m_PSO.SetRenderTargetFormats(1, formats, DXGI_FORMAT_UNKNOWN);
		m_PSO.SetInputLayout(_countof(colorElem), colorElem);
		//--- CHANGE THE DEPTH STATE ALWAYS TO DRAW ON TOP OF GEOMETRY
		//--- THIS HAS TO BE SET TO UNKNOWN FORMAT TO CONFORM TO FRAMEWORK
		//m_PSO.SetDepthTargetFormat(DXGI_FORMAT_UNKNOWN);
		//--- MAKE SURE THAT CULLING IS OFF AND BOTH SIDES ARE DRAWN
		m_PSO.SetRasterizerState(Graphics::RasterizerTwoSided);

		//-- CHANGE TO THE NEW SHADER FOR THE TRIANGLE
		m_PSO.SetVertexShader(VS, sizeVS);
		//m_PSO.SetPixelShader(PS, sizePS);
		m_PSO.SetPixelShader(g_pBlendPS, sizeof(g_pBlendPS));

		m_PSO.Finalize();

	}
	void InitializeRootSignature() {
		SamplerDesc DefaultSamplerDesc;
		DefaultSamplerDesc.MaxAnisotropy = 8;
		SamplerDesc CubeMapSamplerDesc = DefaultSamplerDesc;

		m_RootSignature.Reset(2, 3);

		m_RootSignature.InitStaticSampler(10, DefaultSamplerDesc);
		m_RootSignature.InitStaticSampler(11, Graphics::SamplerShadowDesc);
		m_RootSignature.InitStaticSampler(12, CubeMapSamplerDesc);

		m_RootSignature[0].InitAsConstantBuffer(0);
		m_RootSignature[1].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 1);
		m_RootSignature.Finalize(L"Diffuse Light Blending Pass", D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
	}

	void InitializeDescriptorHeap(ColorBuffer* blendTexture) 
	{
		m_DescriptorHear.Create(L"Uniform Hash Grid Descriptor", D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 2);
		ExtendedUtility::CopyDescriptorsToHeap(
			m_DescriptorHear,
			{
				blendTexture->GetSRV()
			},
			D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV
		);
	}

	//void SetRootParameters(GraphicsContext& gfxContext,
	//ColorBuffer& rayTracingOutColor,
	//	const Camera& camera);




};

