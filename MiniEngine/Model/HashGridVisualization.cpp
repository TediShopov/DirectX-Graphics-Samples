#include "HashGridVisualization.h"

 void HashGridVisualization::Setup(ColorBuffer* color, ColorBuffer* normal, DepthBuffer* depth,ColorBuffer* rayTracingOutColor)
{
	 m_color = color;
	 m_depth = depth;
	 m_normal = normal;
	DXGI_FORMAT ColorFormat = color->GetFormat();
	DXGI_FORMAT NormalFormat = normal->GetFormat();
	DXGI_FORMAT DepthFormat = depth->GetFormat();
	DXGI_FORMAT formats[2] = { ColorFormat, NormalFormat };

	InitializeRootSignature();
	InitializePSO(formats, DepthFormat);
	InitializeDescriptorHeap(
		rayTracingOutColor
	);
}

void HashGridVisualization::InitializePSO(DXGI_FORMAT formats[2], DXGI_FORMAT depthFormat)
{

    //m_DepthPSO.SetVertexShader(g_pDepthViewerVS, sizeof(g_pDepthViewerVS));

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
	m_TestPSO.SetRenderTargetFormats(2, formats, depthFormat);
	m_TestPSO.SetInputLayout(_countof(colorElem), colorElem);
	//--- CHANGE THE DEPTH STATE ALWAYS TO DRAW ON TOP OF GEOMETRY
	m_TestPSO.SetDepthStencilState(Graphics::DepthStateDisabled);
	//--- THIS HAS TO BE SET TO UNKNOWN FORMAT TO CONFORM TO FRAMEWORK
	m_TestPSO.SetDepthTargetFormat(DXGI_FORMAT_UNKNOWN);
	//--- MAKE SURE THAT CULLING IS OFF AND BOTH SIDES ARE DRAWN
	m_TestPSO.SetRasterizerState(Graphics::RasterizerTwoSided);

	//-- CHANGE TO THE NEW SHADER FOR THE TRIANGLE
	m_TestPSO.SetVertexShader(g_pSimpleColorVS, sizeof(g_pSimpleColorVS));
	m_TestPSO.SetPixelShader(g_pSimpleColorPS, sizeof(g_pSimpleColorPS));

	m_TestPSO.Finalize();

}

 void HashGridVisualization::InitializeRootSignature()
{
	m_UHGRootSignature.Reset(2, 3);

	SamplerDesc DefaultSamplerDesc;
	DefaultSamplerDesc.MaxAnisotropy = 8;
	SamplerDesc CubeMapSamplerDesc = DefaultSamplerDesc;

	//---INIT THE STATIC SAMPLERS--
	m_UHGRootSignature.InitStaticSampler(10, DefaultSamplerDesc, D3D12_SHADER_VISIBILITY_PIXEL);
	m_UHGRootSignature.InitStaticSampler(11, Graphics::SamplerShadowDesc, D3D12_SHADER_VISIBILITY_PIXEL);
	m_UHGRootSignature.InitStaticSampler(12, CubeMapSamplerDesc, D3D12_SHADER_VISIBILITY_PIXEL);

	m_UHGRootSignature[kUHGRoot::kProjectionResources].InitAsConstantBuffer(0,
		D3D12_SHADER_VISIBILITY_PIXEL);

	m_UHGRootSignature[kUHGRoot::kDepth].InitAsDescriptorRange(
		D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
		0, 2, D3D12_SHADER_VISIBILITY_PIXEL);
	//m_UHGRootSignature[kUHGRoot::kRayTracingOutput].InitAsBufferUAV(0, D3D12_SHADER_VISIBILITY_PIXEL);
	m_UHGRootSignature.Finalize(L"UHG Root Signature", D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
}

void HashGridVisualization::InitializeDescriptorHeap(ColorBuffer* rayTracingOutColor)
{
	m_UHGTextures.Create(L"Uniform Hash Grid Descriptor", D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 2);

	ExtendedUtility::CopyDescriptorsToHeap(
		m_UHGTextures,
		{
			(*m_depth).GetDepthSRV(),
			rayTracingOutColor->GetSRV()
		},
		D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV
	);
}

  void HashGridVisualization::SetRootParameters(GraphicsContext& gfxContext,  ColorBuffer& rayTracingOutColor, const Camera& camera)
{
	// --- TRANSITON THE DEPTH BUFFER TO BE READABLE BY THE SHADER
	gfxContext.TransitionResource((*m_depth), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, false);
	gfxContext.TransitionResource(rayTracingOutColor, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, false);

	//--- SET THE HEAP CONTAINING ALL THE TEXTURES
	gfxContext.SetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, m_UHGTextures.GetHeapPointer());



	//Correct way of obtraining the viewProjMatrix  as the math library used Column Major Order 
	//m_ViewProjMatrix = m_ProjMatrix * m_ViewMatrix;
	//		Matrix4 invViewProj = Invert(camera.GetViewProjMatrix());
	//        //Need to inversly translate by the camera translation as well to get a real world position
	//		Matrix4 invViewProj = Invert();
	Matrix4 invViewProj = Invert(camera.GetViewProjMatrix());
	Vector3 mathPos = camera.GetPosition();

	m_projectionData.invViewProjeciton = invViewProj;
	//projectionData.invViewProjeciton = Transpose(invViewProj);
	m_projectionData.depthNear = camera.GetNearClip();
	m_projectionData.depthFar = camera.GetFarClip();

	m_projectionBuffer.Create(L"Projectoin Data Buffer", 1, sizeof(UHGProjectionResources), &m_projectionData);

	gfxContext.SetConstantBuffer(kUHGRoot::kProjectionResources, m_projectionBuffer.GetGpuVirtualAddress());

	gfxContext.SetDescriptorTable(kUHGRoot::kDepth, m_UHGTextures[0]);
}

  void HashGridVisualization::SetupRenderStage(GraphicsContext& gfxContext, const D3D12_VIEWPORT& viewport, const D3D12_RECT& scissor, 
	   ColorBuffer& rayTracingOutColor, const Math::Camera& camera)
{

		gfxContext.SetPipelineState(m_TestPSO);
		gfxContext.SetRootSignature(m_UHGRootSignature);

		SetRootParameters(gfxContext, rayTracingOutColor, camera);

		D3D12_CPU_DESCRIPTOR_HANDLE rtvs[]{ m_color->GetRTV(), m_normal->GetRTV() };

		gfxContext.SetRenderTargets(ARRAYSIZE(rtvs), rtvs);

		gfxContext.SetViewportAndScissor(viewport, scissor);

		//RenderTriangleObject(gfxContext, camera.GetViewProjMatrix(), camera.GetPosition(), TestRenderer::kOpaque);
}
