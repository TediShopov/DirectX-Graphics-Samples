#include "HashGridVisualization.h"

  void HashGridVisualization::CreateUHGRootSignature()
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

  void HashGridVisualization::CreateUHGDescriptorHeap(const DepthBuffer& g_SceneDepthBuffer, const ColorBuffer& rayTracingOutColor)
{
	m_UHGTextures.Create(L"Uniform Hash Grid Descriptor", D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 2);

	ExtendedUtility::CopyDescriptorsToHeap(
		m_UHGTextures,
		{
			g_SceneDepthBuffer.GetDepthSRV(),
			rayTracingOutColor.GetSRV()
		},
		D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV
	);
}

  void HashGridVisualization::SetUHGRootParameters(GraphicsContext& gfxContext, DepthBuffer& g_SceneDepthBuffer, ColorBuffer& rayTracingOutColor, const Camera& camera)
{
	// --- TRANSITON THE DEPTH BUFFER TO BE READABLE BY THE SHADER
	gfxContext.TransitionResource(g_SceneDepthBuffer, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, false);
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

  void HashGridVisualization::UHGTriangleRender(GraphicsContext& gfxContext, const D3D12_VIEWPORT& viewport, const D3D12_RECT& scissor, ColorBuffer& sceneColor, ColorBuffer& sceneNormal, DepthBuffer& g_SceneDepthBuffer, ColorBuffer& rayTracingOutColor, const Math::Camera& camera)
{

		gfxContext.SetPipelineState(m_TestPSO);
		gfxContext.SetRootSignature(m_UHGRootSignature);

		SetUHGRootParameters(gfxContext, g_SceneDepthBuffer, rayTracingOutColor, camera);

		D3D12_CPU_DESCRIPTOR_HANDLE rtvs[]{ sceneColor.GetRTV(), sceneNormal.GetRTV() };

		gfxContext.SetRenderTargets(ARRAYSIZE(rtvs), rtvs);

		gfxContext.SetViewportAndScissor(viewport, scissor);

		//RenderTriangleObject(gfxContext, camera.GetViewProjMatrix(), camera.GetPosition(), TestRenderer::kOpaque);
}
