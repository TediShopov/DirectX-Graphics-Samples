#pragma once
#include "HashGridVisualization.h"

#include "CompiledShaders/SurfelSpawningVisualizationPS.h"

class SurfelSpawnChanceVisualization :
    public HashGridVisualization
{
	__declspec(align(16)) struct DebugModeCB {
		UINT  indexX;
		UINT  indexY;
		UINT  indexZ;
		UINT  indexW;
	} m_DebugMode;
	RootSignature m_Test;
	void CreateExtraRootSignature()
	{
	 SamplerDesc DefaultSamplerDesc;
	 DefaultSamplerDesc.MaxAnisotropy = 8;
	 SamplerDesc CubeMapSamplerDesc = DefaultSamplerDesc;

	 m_Test.Reset(6, 3);


	 m_Test.InitStaticSampler(10, DefaultSamplerDesc);
	 m_Test.InitStaticSampler(11, Graphics::SamplerShadowDesc);
	 m_Test.InitStaticSampler(12, CubeMapSamplerDesc);

	 m_Test[0].InitAsConstantBuffer(0);
	 m_Test[1].InitAsConstantBuffer(1);
	 //SRVs: Position and Normal
	 m_Test[2].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 4);
	 //UAVs: 
	 m_Test[3].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 0, 4);
	 //For Debug
	 m_Test[4].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 0, 1,D3D12_SHADER_VISIBILITY_ALL,1);
	 m_Test[5].InitAsConstantBuffer(2);

	 m_Test.Finalize(L"Surfel Informed Spanwing Debug Overlay",D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);



	}


	virtual void InitializePSO( DXGI_FORMAT formats[2], DXGI_FORMAT depthFormat) override
	{
		HashGridVisualization::InitializePSO(formats, depthFormat);

		CreateExtraRootSignature();


		//m_GBuffer.g_DepthPSO.SetVertexShader(g_pDepthViewerVS, sizeof(g_pDepthViewerVS));

		D3D12_INPUT_ELEMENT_DESC colorElem[] =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 16, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
		};

		//m_TestPSO = Depth;
		//--- REPLACE THE ROOT SIGNATURE
		m_TestPSO.SetRootSignature(m_Test);

		m_TestPSO.SetRasterizerState(Graphics::RasterizerDefault);
		m_TestPSO.SetBlendState(Graphics::BlendDisable);
		m_TestPSO.SetDepthStencilState(Graphics::DepthStateTestEqual);
		m_TestPSO.SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
		//m_TestPSO.SetRenderTargetFormats(2, formats, DepthFormat);
		//m_TestPSO.SetRenderTargetFormats(2, formats, depthFormat);
		m_TestPSO.SetRenderTargetFormats(1, &formats[0], DXGI_FORMAT_UNKNOWN);
		m_TestPSO.SetInputLayout(_countof(colorElem), colorElem); //--- CHANGE THE DEPTH STATE ALWAYS TO DRAW ON TOP OF GEOMETRY
		m_TestPSO.SetDepthStencilState(Graphics::DepthStateDisabled);
		//--- THIS HAS TO BE SET TO UNKNOWN FORMAT TO CONFORM TO FRAMEWORK
		//m_TestPSO.SetDepthTargetFormat(DXGI_FORMAT_UNKNOWN);
		//--- MAKE SURE THAT CULLING IS OFF AND BOTH SIDES ARE DRAWN
		m_TestPSO.SetRasterizerState(Graphics::RasterizerTwoSided);

		//-- CHANGE TO THE NEW SHADER FOR THE TRIANGLE
		m_TestPSO.SetVertexShader(g_pSimpleColorVS, sizeof(g_pSimpleColorVS));
		m_TestPSO.SetPixelShader(g_pSurfelSpawningVisualizationPS, sizeof(g_pSurfelSpawningVisualizationPS));

		m_TestPSO.Finalize();


	}
	

public:
	void SetDebugMode(GraphicsContext& gfx,UINT debugModeIndex) 
	{
		m_DebugMode.indexX = debugModeIndex;
		

		//gfx.SetDynamicConstantBufferView(5, sizeof(DebugModeCB), &m_DebugMode);
		gfx.SetDynamicConstantBufferView(5, sizeof(DebugModeCB), &m_DebugMode);
		
	}

	void SetupRenderStage(GraphicsContext& gfxContext,
		const D3D12_VIEWPORT& viewport,
		const D3D12_RECT& scissor,
		ColorBuffer& rayTracingOutColor,
		const Math::Camera& camera) override {


		gfxContext.SetPipelineState(m_TestPSO);
		gfxContext.SetRootSignature(m_Test);


		gfxContext.TransitionResource(*m_GBuffer.g_Depth, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, true);
		gfxContext.TransitionResource(*m_GBuffer.g_Normal, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, true);


		D3D12_CPU_DESCRIPTOR_HANDLE rtvs[]{ m_GBuffer.g_Color->GetRTV(), m_GBuffer.g_Normal->GetRTV() };

		gfxContext.SetRenderTargets(ARRAYSIZE(rtvs), rtvs);

		gfxContext.SetViewportAndScissor(viewport, scissor);

		//RenderTriangleObject(gfxContext, camera.GetViewProjMatrix(), camera.GetPosition(), TestRenderer::kOpaque);
	 }
};

