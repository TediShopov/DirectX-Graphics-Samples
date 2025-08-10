#include "HashGridVisualization.h"
#include "SurfelGI.h"

 void HashGridVisualization::Setup(GBufferPtrs gbuffer,ColorBuffer* rayTracingOutColor)
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

void HashGridVisualization::InitializePSO(DXGI_FORMAT formatsA[2], DXGI_FORMAT depthFormat)
{

    //m_GBuffer.g_DepthPSO.SetVertexShader(g_pDepthViewerVS, sizeof(g_pDepthViewerVS));

	D3D12_INPUT_ELEMENT_DESC colorElem[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 16, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
	};

	//m_TestPSO = Depth;
	//--- REPLACE THE ROOT SIGNATURE
	DXGI_FORMAT formats[1] = { DXGI_FORMAT_R11G11B10_FLOAT };

	m_TestPSO = GraphicsPSO(L"New Test PSO AA");

	m_TestPSO.SetRootSignature(m_UHGRootSignature);

    m_TestPSO.SetRasterizerState(Graphics::RasterizerDefault);
	m_TestPSO.SetBlendState(Graphics::BlendDisable);
	m_TestPSO.SetDepthStencilState(Graphics::DepthStateTestEqual);
    m_TestPSO.SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
	//m_TestPSO.SetRenderTargetFormats(2, formats, DepthFormat);
	//m_TestPSO.SetRenderTargetFormats(2, formats, depthFormat);
	m_TestPSO.SetRenderTargetFormats(1, formats, DXGI_FORMAT_UNKNOWN);
	m_TestPSO.SetInputLayout(_countof(colorElem), colorElem);
	//--- CHANGE THE DEPTH STATE ALWAYS TO DRAW ON TOP OF GEOMETRY
	m_TestPSO.SetDepthStencilState(Graphics::DepthStateDisabled);
	//--- THIS HAS TO BE SET TO UNKNOWN FORMAT TO CONFORM TO FRAMEWORK
	//m_TestPSO.SetDepthTargetFormat(DXGI_FORMAT_UNKNOWN);
	//--- MAKE SURE THAT CULLING IS OFF AND BOTH SIDES ARE DRAWN
	m_TestPSO.SetRasterizerState(Graphics::RasterizerTwoSided);

	//-- CHANGE TO THE NEW SHADER FOR THE TRIANGLE
	m_TestPSO.SetVertexShader(g_pSimpleColorVS, sizeof(g_pSimpleColorVS));
	m_TestPSO.SetPixelShader(g_pSimpleColorPS, sizeof(g_pSimpleColorPS));

	m_TestPSO.Finalize();

}

 void HashGridVisualization::InitializeRootSignature()
{
	 SamplerDesc DefaultSamplerDesc;
	 DefaultSamplerDesc.MaxAnisotropy = 8;
	 SamplerDesc CubeMapSamplerDesc = DefaultSamplerDesc;

	 m_UHGRootSignature.Reset(6, 3);


	 m_UHGRootSignature.InitStaticSampler(10, DefaultSamplerDesc);
	 m_UHGRootSignature.InitStaticSampler(11, Graphics::SamplerShadowDesc);
	 m_UHGRootSignature.InitStaticSampler(12, CubeMapSamplerDesc);

	 m_UHGRootSignature[0].InitAsConstantBuffer(0);
	 m_UHGRootSignature[1].InitAsConstantBuffer(1);
	 //SRVs: Position and Normal
	 m_UHGRootSignature[2].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 2);
	 //UAVs: 
	 m_UHGRootSignature[3].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 0, 4);
	 //For Debug
	 m_UHGRootSignature[4].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 0, 1,D3D12_SHADER_VISIBILITY_ALL,1);
	 m_UHGRootSignature[5].InitAsConstantBuffer(2);

	 m_UHGRootSignature.Finalize(L"Hash Grid Visualiztion RT",D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);



}

void HashGridVisualization::InitializeDescriptorHeap(ColorBuffer* rayTracingOutColor)
{
	m_UHGTextures.Create(L"Uniform Hash Grid Descriptor", D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 2);

	ExtendedUtility::CopyDescriptorsToHeap(
		m_UHGTextures,
		{
			(*m_GBuffer.g_Depth).GetDepthSRV(),
			rayTracingOutColor->GetSRV()
		},
		D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV
	);
}



  void HashGridVisualization::SetupRenderStage(GraphicsContext& gfxContext, const D3D12_VIEWPORT& viewport, const D3D12_RECT& scissor, 
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
