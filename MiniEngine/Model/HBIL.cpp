#include "HBIL.h"
#include "GBufferDownsample.h"

 HBIL::HBIL()
{
	m_HBILExtraCB._gatherSphereMaxRadius_m = 1000;
	m_HBILExtraCB._gatherSphereMaxRadius_p = 1500;

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
	//DXGI_FORMAT formats[2] = { DXGI_FORMAT_R11G11B10_FLOAT,DXGI_FORMAT_R11G11B10_FLOAT };
	//m_HBILRenderPass.SetRenderTargetFormats(2, formats,DXGI_FORMAT_UNKNOWN);
	m_HBILRenderPass.SetPixelShader(g_pComputeHBIL_BruteForce, sizeof(g_pComputeHBIL_BruteForce));

	m_HBILRenderPass.Finalize();

}

 void HBIL::CreateHBILHeap(ColorBuffer* quarterResGBuffer) {
	m_HBILHeap.Create(L"HBIL HEAP", D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 5);

	ExtendedUtility::CopyDescriptorsToHeap(m_HBILHeap, {
		quarterResGBuffer[DEPTH].GetSRV(),
		quarterResGBuffer[NORMAL].GetSRV(),
		quarterResGBuffer[DIFFUSE].GetSRV(),
		m_BlueNoiseTexture.GetSRV(),
		m_DebugHBIL.GetUAV()
		}
	);

}

 Matrix4 HBIL::GetLHViewMatrix(const Camera& camera)
{
	//Get the camera position up, right and forward vectors
	Vector3 position = camera.GetPosition();
	Vector3 forward = camera.GetForwardVec();
	Vector3 right = camera.GetRightVec();
	Vector3 up = camera.GetUpVec();

	const float lhViewMatrixData[16] = {
		(float)right.GetX(), (float)up.GetX(), (float)forward.GetX(), 0.0f,
		(float)right.GetY(), (float)up.GetY(), (float)forward.GetY(), 0.0f,
		(float)right.GetZ(), (float)up.GetZ(), (float)forward.GetZ(), 0.0f,
		-(float)Dot(right, position), -(float)Dot(up, position), -(float)Dot(forward, position), 1.0f
	};

	// Construct left-handed view matrix
	Matrix4 viewMatrix = Matrix4(lhViewMatrixData);
	return viewMatrix;

}

 void HBIL::ReadDebugHBIL(GraphicsContext& gfx, D3D12_RESOURCE_STATES endState, bool flushImmediate)
{
	if (m_debugReadingEnabled)
	{
		gfx.TransitionResource(m_DebugHBIL, D3D12_RESOURCE_STATE_COPY_DEST, flushImmediate);
		gfx.CopyBuffer(m_DebugHBILReadback, m_DebugHBIL);
		void* mappedData = m_DebugHBILReadback.Map();
		memcpy(&m_DebugHBILActual, mappedData, sizeof(m_DebugHBILActual));
		m_DebugHBILReadback.Unmap();
		gfx.TransitionResource(m_DebugHBIL, endState, flushImmediate);

	}



}

 void HBIL::Setup(GBufferPtrs gbuffer, ColorBuffer* downsampledGBuffers, GraphicsPSO quadPSO)
{
	this->m_GBuffer = gbuffer;
	m_downsampledGBuffers = downsampledGBuffers;

	//LOAD BLUE NOISE TEXTURE
	m_BlueNoiseTexture = TextureManager::LoadDDSFromFile(L"Textures/blueNoise_HDR_LA0.dds");
	m_DebugHBIL.Create(L"Debug HBIL Buffer", 1, sizeof(DebugHBILData));
	m_DebugHBILReadback.Create(L"Debug HBIL Buffer", 1, sizeof(DebugHBILData));

	CreateHBILHeap(downsampledGBuffers);

	CreateHBILRootSignatue();

	CreateHBILPSO(quadPSO);

	//Create the render targets
	//m_IrradianceRenderTarget.Create(L"HBIL Irradiance Render Target", gbuffer.g_Color->GetWidth(), gbuffer.g_Color->GetHeight(),0,DXGI_FORMAT_R11G11B10_FLOAT);
	//m_BentConesRenderTarget.Create(L"HBIL Bent Cones Render Target", gbuffer.g_Color->GetWidth(), gbuffer.g_Color->GetHeight(),0,DXGI_FORMAT_R11G11B10_FLOAT);
}

 Matrix4 HBIL::CreatePerspectiveFovLH(float fovY, float aspect, float nearZ, float farZ)
{
	float yScale = 1.0f / tanf(fovY * 0.5f);
	float xScale = yScale / aspect;
	float zRange = farZ - nearZ;
	const float lhPerspectiveData[16] =
	{

		xScale, 0.0f,    0.0f,                     0.0f,
		0.0f,   yScale,  0.0f,                     0.0f,
		0.0f,   0.0f,    farZ / zRange,            1.0f,
		0.0f,   0.0f,    -nearZ * farZ / zRange,   0.0f
	};

	return Matrix4(lhPerspectiveData);
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



	//Get the camera position up, right and forward vectors
	// Construct left-handed view matrix
	Matrix4 viewMatrix = GetLHViewMatrix(camera);

	// Construct LH perspective projection matrix
	float fovY = camera.GetFOV(); // example FOV
	float aspect = m_GBuffer.g_Color->GetWidth() / m_GBuffer.g_Color->GetHeight();
	float nearZ = camera.GetNearClip();
	float farZ = camera.GetFarClip();

	Matrix4 projMatrix = CreatePerspectiveFovLH(fovY, aspect, nearZ, farZ);
	Matrix4 viewProjMatrix = projMatrix * viewMatrix;





	m_HBILCameraCB._world2Camera = Matrix4(XMMatrixTranspose(viewMatrix));
	m_HBILCameraCB._camera2Proj = Matrix4(XMMatrixTranspose(projMatrix));
	m_HBILCameraCB._world2Proj = Matrix4(XMMatrixTranspose(viewProjMatrix));

	m_HBILCameraCB._camera2World = Matrix4(XMMatrixTranspose(XMMatrixInverse(nullptr, viewMatrix)));
	m_HBILCameraCB._proj2Camera = Matrix4(XMMatrixTranspose(XMMatrixInverse(nullptr, projMatrix)));
	m_HBILCameraCB._proj2World = Matrix4(XMMatrixTranspose(XMMatrixInverse(nullptr, viewProjMatrix)));

	//Try without the transpose

	//		m_HBILCameraCB._world2Camera = Matrix4((viewMatrix));
	//		m_HBILCameraCB._camera2Proj = Matrix4((projMatrix));
	//		m_HBILCameraCB._world2Proj = Matrix4((viewProjMatrix));
	//
	//		m_HBILCameraCB._camera2World = Matrix4((XMMatrixInverse(nullptr, viewMatrix)));
	//		m_HBILCameraCB._proj2Camera = Matrix4((XMMatrixInverse(nullptr, projMatrix)));
	//		m_HBILCameraCB._proj2World = Matrix4((XMMatrixInverse(nullptr, viewProjMatrix)));




	float Q = camera.GetFarClip() / (camera.GetFarClip() - camera.GetNearClip());
	m_HBILCameraCB._ZNearFar_Q_Z = XMFLOAT4(camera.GetNearClip(), camera.GetFarClip(), Q, 0);

	m_HBILExtraCB._bilateralValues = XMFLOAT4(0, 0, 0, 0);
	m_HBILExtraCB._temporalAttenuationFactor = 0.5f;


	//Resource barrier
	gfx.TransitionResource(m_downsampledGBuffers[0], D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, true);
	gfx.TransitionResource(m_downsampledGBuffers[1], D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, true);
	gfx.TransitionResource(m_downsampledGBuffers[2], D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, true);


	//Temporaily transition the full resolution resources from the GBuffer
	//		gfx.TransitionResource(*m_GBuffer.g_Depth, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	//		gfx.TransitionResource(*m_GBuffer.g_Normal, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	//		gfx.TransitionResource(*m_GBuffer.g_Color, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,true);


	gfx.SetPipelineState(m_HBILRenderPass);
	gfx.SetRootSignature(m_HBILRenderRS);
	gfx.SetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, m_HBILHeap.GetHeapPointer());

	gfx.SetDynamicConstantBufferView(0, sizeof(HBIL_MAIN), &m_MainHBILCB);
	gfx.SetDynamicConstantBufferView(1, sizeof(CB_Camera), &m_HBILCameraCB);
	gfx.SetDynamicConstantBufferView(2, sizeof(CBSH), &m_CBSH);
	gfx.SetDynamicConstantBufferView(3, sizeof(CB_HBIL), &m_HBILExtraCB);
	gfx.SetDescriptorTable(4, m_HBILHeap[0]);
	gfx.SetDescriptorTable(5, m_HBILHeap[4]);

	//		gfx.TransitionResource(m_IrradianceRenderTarget, D3D12_RESOURCE_STATE_RENDER_TARGET, true);
	//		gfx.TransitionResource(m_BentConesRenderTarget, D3D12_RESOURCE_STATE_RENDER_TARGET, true);

	//		const D3D12_CPU_DESCRIPTOR_HANDLE handles[2]
	//		{
	//			m_IrradianceRenderTarget.GetRTV(),
	//			m_BentConesRenderTarget.GetRTV()
	//		};
	//		gfx.SetRenderTargets(2, handles);






	framesCount++;

}
