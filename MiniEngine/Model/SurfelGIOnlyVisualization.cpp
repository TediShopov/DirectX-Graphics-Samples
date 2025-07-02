#include "SurfelGIOnlyVisualization.h"
#include "SurfelGI.h"

 void SurfelGIOnlyVisualization::Setup(GBufferPtrs gbuffer,ColorBuffer* rayTracingOutColor)
{
	 m_GBuffer = gbuffer;
	DXGI_FORMAT ColorFormat = m_GBuffer.g_Color->GetFormat();
	DXGI_FORMAT NormalFormat = m_GBuffer.g_Normal->GetFormat();
	DXGI_FORMAT DepthFormat = m_GBuffer.g_Depth->GetFormat();
	DXGI_FORMAT formats[2] = { ColorFormat, NormalFormat };

	InitializeRootSignature();
	InitializePSO(formats, DepthFormat);
	InitializeDescriptorHeap(
		rayTracingOutColor
	);
}

void SurfelGIOnlyVisualization::InitializePSO(DXGI_FORMAT formats[2], DXGI_FORMAT depthFormat)
{

    //m_GBuffer.g_DepthPSO.SetVertexShader(g_pDepthViewerVS, sizeof(g_pDepthViewerVS));

	D3D12_INPUT_ELEMENT_DESC colorElem[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 16, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
	};

	//m_TestPSO = Depth;
	//--- REPLACE THE ROOT SIGNATURE
	m_TestPSO.SetRootSignature(m_UHGRootSignature);

    m_TestPSO.SetRasterizerState(Graphics::RasterizerDefault);
	m_TestPSO.SetBlendState(Graphics::BlendDisable);
	m_TestPSO.SetDepthStencilState(Graphics::DepthStateTestEqual);
    m_TestPSO.SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
	//m_TestPSO.SetRenderTargetFormats(2, formats, DepthFormat);
	//m_TestPSO.SetRenderTargetFormats(2, formats, depthFormat);
	m_TestPSO.SetRenderTargetFormats(1, &formats[0], DXGI_FORMAT_UNKNOWN);
	m_TestPSO.SetInputLayout(_countof(colorElem), colorElem);
	//--- CHANGE THE DEPTH STATE ALWAYS TO DRAW ON TOP OF GEOMETRY
	m_TestPSO.SetDepthStencilState(Graphics::DepthStateDisabled);
	//--- THIS HAS TO BE SET TO UNKNOWN FORMAT TO CONFORM TO FRAMEWORK
	m_TestPSO.SetDepthTargetFormat(DXGI_FORMAT_UNKNOWN);
	//--- MAKE SURE THAT CULLING IS OFF AND BOTH SIDES ARE DRAWN
	m_TestPSO.SetRasterizerState(Graphics::RasterizerTwoSided);

	//-- CHANGE TO THE NEW SHADER FOR THE TRIANGLE
	m_TestPSO.SetVertexShader(g_pSurfelGIOnlyVS, sizeof(g_pSurfelGIOnlyVS));
	m_TestPSO.SetPixelShader(g_pSurfelGIOnlyPS, sizeof(g_pSurfelGIOnlyPS));

	m_TestPSO.Finalize();

}

 void SurfelGIOnlyVisualization::InitializeRootSignature()
{
	 SamplerDesc DefaultSamplerDesc;
	 DefaultSamplerDesc.MaxAnisotropy = 8;
	 SamplerDesc CubeMapSamplerDesc = DefaultSamplerDesc;

	 m_UHGRootSignature.Reset(1, 1);
	 m_UHGRootSignature.InitStaticSampler(10, DefaultSamplerDesc);
	 m_UHGRootSignature[0].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 1);
	 m_UHGRootSignature.Finalize(L"Surfel GI Only Viualization RS",D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);



}

void SurfelGIOnlyVisualization::InitializeDescriptorHeap(ColorBuffer* rayTracingOutColor)
{
	m_UHGTextures.Create(L"Uniform Hash Grid Descriptor", D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1);

	ExtendedUtility::CopyDescriptorsToHeap(
		m_UHGTextures,
		{
			rayTracingOutColor->GetSRV()
		},
		D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV
	);
}

void SurfelGIOnlyVisualization::SetRootParameters(GraphicsContext& gfxContext, ColorBuffer& rayTracingOutColor)
{
	gfxContext.SetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, m_UHGTextures.GetHeapPointer());
	gfxContext.SetDescriptorTable(0, m_UHGTextures[0]);

}



  void SurfelGIOnlyVisualization::SetupRenderStage(GraphicsContext& gfxContext, const D3D12_VIEWPORT& viewport, const D3D12_RECT& scissor, 
	   ColorBuffer& rayTracingOutColor, const Math::Camera& camera)
{

		gfxContext.SetPipelineState(m_TestPSO);
		gfxContext.SetRootSignature(m_UHGRootSignature);


		gfxContext.TransitionResource(*m_GBuffer.g_Depth, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, true);
		gfxContext.TransitionResource(*m_GBuffer.g_Normal, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, true);


		D3D12_CPU_DESCRIPTOR_HANDLE rtvs[]{ m_GBuffer.g_Color->GetRTV(), m_GBuffer.g_Normal->GetRTV() };

		gfxContext.SetRenderTargets(ARRAYSIZE(rtvs), rtvs);

		gfxContext.SetViewportAndScissor(viewport, scissor);

		//RenderTriangleObject(gfxContext, camera.GetViewProjMatrix(), camera.GetPosition(), TestRenderer::kOpaque);
}
