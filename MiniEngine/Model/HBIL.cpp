#include "HBIL.h"
#include "GBufferDownsample.h"

 HBIL::HBIL()
{
	m_HBILExtraCB._gatherSphereMaxRadius_m = 50;
	m_HBILExtraCB._gatherSphereMaxRadius_p = 80;


}

 void HBIL::CreateHBILRootSignatue()
{
	//Todo initialize the samplers correctly
	SamplerDesc LinearClamp;
	LinearClamp.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	LinearClamp.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	LinearClamp.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	LinearClamp.MaxAnisotropy = 0;

	SamplerDesc LinearWrap = LinearClamp;
	LinearWrap.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	LinearWrap.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	LinearWrap.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;

	m_HBILRenderRS.Reset(6, 2);
	//Todo check the correct positioning of the static samplers
	m_HBILRenderRS.InitStaticSampler(0, LinearClamp);
	m_HBILRenderRS.InitStaticSampler(1, LinearWrap);

	//Initializing the constant buffer
	//This CBs are for the HBIL's global utility file
	m_HBILRenderRS[0].InitAsConstantBuffer(0);
	m_HBILRenderRS[1].InitAsConstantBuffer(1);
	m_HBILRenderRS[2].InitAsConstantBuffer(2);
	//This CBs are the HBIL's application files
	m_HBILRenderRS[3].InitAsConstantBuffer(3);
	//Supplying The Textures
	m_HBILRenderRS[4].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 4);
	m_HBILRenderRS[5].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 0, 1);

	m_HBILRenderRS.Finalize(L"HBIL Root Signature", D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

}

 void HBIL::CreateHBILPSO(GraphicsPSO quadRenderingPSO)
{
	m_HBILRenderPass = quadRenderingPSO;

	m_HBILRenderPass.SetRootSignature(m_HBILRenderRS);
	DXGI_FORMAT formats[2] = { DXGI_FORMAT_R11G11B10_FLOAT,DXGI_FORMAT_R16G16B16A16_FLOAT };
	m_HBILRenderPass.SetRenderTargetFormats(2, formats,DXGI_FORMAT_UNKNOWN);
	m_HBILRenderPass.SetPixelShader(g_pComputeHBIL_BruteForce, sizeof(g_pComputeHBIL_BruteForce));

	m_HBILRenderPass.Finalize();

}

 void HBIL::CreateHBILHeap(ColorBuffer* quarterResGBuffer) {
	m_HBILHeap.Create(L"HBIL HEAP", D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 5);

	ExtendedUtility::CopyDescriptorsToHeap(m_HBILHeap, {
		quarterResGBuffer[DEPTH].GetSRV(),
		quarterResGBuffer[NORMAL].GetSRV(),
		quarterResGBuffer[DIFFUSE].GetSRV(),
		//m_GBuffer.g_Diffuse->GetSRV(),
		m_BlueNoiseTexture.GetSRV(),
		m_DebugHBIL.GetUAV()
		}
	);

}


 void HBIL::ReadDebugHBIL(GraphicsContext& gfx, D3D12_RESOURCE_STATES endState, bool flushImmediate)
{
	if (m_debugReadingEnabled)
	{
		gfx.TransitionResource(m_DebugHBIL, D3D12_RESOURCE_STATE_COPY_DEST, flushImmediate);
		gfx.CopyBuffer(m_DebugHBILReadback, m_DebugHBIL);
		void* mappedData = m_DebugHBILReadback.Map();
		memcpy(m_DebugHBILActual.data(), mappedData, sizeof(DebugHBILData) * m_debugAngles);
		m_DebugHBILReadback.Unmap();
		gfx.TransitionResource(m_DebugHBIL, endState, flushImmediate);

	}



}

 void HBIL::Setup(GBufferPtrs gbuffer, ColorBuffer* downsampledGBuffers, GraphicsPSO quadPSO)
{
	this->m_GBuffer = gbuffer;
	m_inputBuffers = downsampledGBuffers;

	//LOAD BLUE NOISE TEXTURE
	m_BlueNoiseTexture = TextureManager::LoadDDSFromFile(L"Textures/blueNoise_HDR_LA0.dds");
	//m_DebugHBILActual.reserve(m_debugAngles);
	m_DebugHBILActual = std::vector<DebugHBILData>(m_debugAngles);
	m_DebugHBIL.Create(L"Debug HBIL Buffer", m_debugAngles, sizeof(DebugHBILData));
	m_DebugHBILReadback.Create(L"Debug HBIL Buffer", m_debugAngles, sizeof(DebugHBILData));

	CreateHBILHeap(downsampledGBuffers);

	CreateHBILRootSignatue();

	CreateHBILPSO(quadPSO);

	//Create the render targets
	m_OutputIrradiance.Create(L"HBIL Irradiance Render Target", gbuffer.g_Color->GetWidth(), gbuffer.g_Color->GetHeight(),0,DXGI_FORMAT_R11G11B10_FLOAT);
	m_OutputBentCone.Create(L"HBIL Bent Cones Render Target", gbuffer.g_Color->GetWidth(), gbuffer.g_Color->GetHeight(),0,DXGI_FORMAT_R16G16B16A16_FLOAT);
}


 void HBIL::UpdateCameraCBufferLH(const Camera& camera, CB_Camera& cameraCBuffer)
 {
	//Get the camera position up, right and forward vectors
	// Construct left-handed view matrix
	Matrix4 viewMatrix = ExtendedUtility::GetLHViewMatrix(camera);

	// Construct LH perspective projection matrix
	float fovY = camera.GetFOV(); // example FOV
	float aspect = (float)m_GBuffer.g_Color->GetWidth() / (float)m_GBuffer.g_Color->GetHeight();
	float nearZ = camera.GetNearClip();
	float farZ = camera.GetFarClip();

	Matrix4 projMatrix = ExtendedUtility::CreatePerspectiveFovLH(fovY, aspect, nearZ, farZ);
	Matrix4 originalProjectionMatrxi = camera.GetProjMatrix();
	Matrix4 viewProjMatrix = projMatrix * viewMatrix;





	cameraCBuffer._world2Camera = Matrix4(XMMatrixTranspose(viewMatrix));
	cameraCBuffer._camera2Proj = Matrix4(XMMatrixTranspose(projMatrix));
	cameraCBuffer._world2Proj = Matrix4(XMMatrixTranspose(viewProjMatrix));

	cameraCBuffer._camera2World = Matrix4(XMMatrixTranspose(XMMatrixInverse(nullptr, viewMatrix)));
	cameraCBuffer._proj2Camera = Matrix4(XMMatrixTranspose(XMMatrixInverse(nullptr, projMatrix)));
	cameraCBuffer._proj2World = Matrix4(XMMatrixTranspose(XMMatrixInverse(nullptr, viewProjMatrix)));

	//Try without the transpose

	//		cameraCBuffer._world2Camera = Matrix4((viewMatrix));
	//		cameraCBuffer._camera2Proj = Matrix4((projMatrix));
	//		cameraCBuffer._world2Proj = Matrix4((viewProjMatrix));
	//
	//		cameraCBuffer._camera2World = Matrix4((XMMatrixInverse(nullptr, viewMatrix)));
	//		cameraCBuffer._proj2Camera = Matrix4((XMMatrixInverse(nullptr, projMatrix)));
	//		cameraCBuffer._proj2World = Matrix4((XMMatrixInverse(nullptr, viewProjMatrix)));




	float Q = camera.GetFarClip() / (camera.GetFarClip() - camera.GetNearClip());
	cameraCBuffer._ZNearFar_Q_Z = XMFLOAT4(camera.GetNearClip(), camera.GetFarClip(), Q, 0);

 }

 void HBIL::RenderHBIL(GraphicsContext& gfx, const Camera& camera)
{
	ScopedTimer _prof(L"Render HBIL", gfx);

	m_MainHBILCB._deltaTime = 0.1;
	m_MainHBILCB._framesCount = 0;
	m_MainHBILCB._resolution.x = m_GBuffer.g_Color->GetWidth();
	m_MainHBILCB._resolution.y = m_GBuffer.g_Color->GetHeight();
	m_MainHBILCB._coneAngleBias = 0.1f;
	m_MainHBILCB._framesCount = framesCount;
	m_MainHBILCB._flags = 0;



	UpdateCameraCBufferLH(camera, m_HBILCameraCB);

	m_HBILExtraCB._bilateralValues = XMFLOAT4(1, 1, 1, 1);
	m_HBILExtraCB._temporalAttenuationFactor = 0.5f;


	gfx.SetPipelineState(m_HBILRenderPass);
	gfx.SetRootSignature(m_HBILRenderRS);
	gfx.SetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, m_HBILHeap.GetHeapPointer());

	gfx.SetDynamicConstantBufferView(0, sizeof(HBIL_MAIN), &m_MainHBILCB);
	gfx.SetDynamicConstantBufferView(1, sizeof(CB_Camera), &m_HBILCameraCB);
	gfx.SetDynamicConstantBufferView(2, sizeof(CBSH), &m_CBSH);
	gfx.SetDynamicConstantBufferView(3, sizeof(CB_HBIL), &m_HBILExtraCB);
	gfx.SetDescriptorTable(4, m_HBILHeap[0]);
	gfx.SetDescriptorTable(5, m_HBILHeap[4]);


	D3D12_CPU_DESCRIPTOR_HANDLE rtvs[2] = {
		m_OutputIrradiance.GetRTV(),
		m_OutputBentCone.GetRTV()
	};

	if (m_renderAtColorBuffer)
	{
		rtvs[0] =
			Graphics::g_SceneColorBuffer.GetRTV();

	}

	gfx.SetRenderTargets(2, rtvs);

	framesCount++;

}
