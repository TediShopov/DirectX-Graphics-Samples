#include "SurfelSSRMIrradianceAccumulation.h"
#include "SurfelGI.h"
#include "Geometry/Transform.h"

#include "CompiledShaders/SurfelIrradianceSSRMCS.h"
 void SurfelSSRMIrradianceAccumulation::CreateRootSig()
{

	SamplerDesc DefaultSamplerDesc;
	//DefaultSamplerDesc.MaxAnisotropy = 8;
	DefaultSamplerDesc.MaxAnisotropy = 1;
	DefaultSamplerDesc.Filter = D3D12_FILTER_MAXIMUM_MIN_MAG_MIP_POINT;
	SamplerDesc CubeMapSamplerDesc = DefaultSamplerDesc;

	m_rootSignature.Reset(6, 3);

	m_rootSignature.InitStaticSampler(10, DefaultSamplerDesc);
	m_rootSignature.InitStaticSampler(11, Graphics::SamplerShadowDesc);
	m_rootSignature.InitStaticSampler(12, CubeMapSamplerDesc);

	m_rootSignature[0].InitAsConstantBuffer(0);
	m_rootSignature[1].InitAsConstantBuffer(1);
	m_rootSignature[2].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 3);
	m_rootSignature[3].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 0, 5);
	//Here the color buffer would be passed
	m_rootSignature[4].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 1, D3D12_SHADER_VISIBILITY_ALL, 1);
	//The constant buffer for screen space 
	m_rootSignature[5].InitAsConstantBuffer(2);

	m_rootSignature.Finalize(L"CS Surfel SSRM Irradiance Signature");
}

 void SurfelSSRMIrradianceAccumulation::CreateHeaps(ColorBuffer colorBuff,DescriptorHeap nonShaderVisibleSurfelHeap)
{
	 width = colorBuff.GetWidth();
	 height = colorBuff.GetHeight();

		//Foreach texture referene found in the model
		std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> handles;
		for (size_t i = 0; i < 8; i++)
		{
				handles.push_back(nonShaderVisibleSurfelHeap[i]);

		}
		//On the 3rd position pass the color copy buffer
		handles.insert(handles.begin() + 2, colorBuff.GetSRV());

		this->SRVHeap.Create(L"Surfel SSRMI Heap", D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 9);
		ExtendedUtility::CopyDescriptorsToHeap(this->SRVHeap, handles);
}

 void SurfelSSRMIrradianceAccumulation::InitializePSOs()
{
	m_SSRMIrradiancePSO.SetRootSignature(m_rootSignature);
	m_SSRMIrradiancePSO.SetComputeShader(g_pSurfelIrradianceSSRMCS, sizeof(g_pSurfelIrradianceSSRMCS));
	m_SSRMIrradiancePSO.Finalize();
}

 void SurfelSSRMIrradianceAccumulation::Setup(ColorBuffer colorBuff,DescriptorHeap nonShaderVisibleSurfelHeap)
{
	CreateRootSig();

	//SetDefaultCBData();

	//InitializeBuffers();

	//FillCPUContainers();

	//CopyCPUContainersToRespectiveGPUBuffers();

	CreateHeaps( colorBuff, nonShaderVisibleSurfelHeap);

	InitializePSOs();

}

 void SurfelSSRMIrradianceAccumulation::Dispatch(ComputeContext& cfxA,const Camera& camera,SurfelGenCB* genCB)
 {
	 Transform camTempT;


	 commonSSR.cameraData.cameraProjMatrix = camera.GetProjMatrix();
	 commonSSR.cameraData.cameraViewMatrix = camera.GetViewMatrix();
	 commonSSR.cameraData.cameraWorldMatrix = Matrix4(camTempT.getTransformMatrix());

	 commonSSR.cameraData.inverseProjMatrix = Matrix4(XMMatrixInverse(nullptr, camera.GetProjMatrix()));
	 commonSSR.cameraData.inverseViewMatrix = Matrix4(XMMatrixInverse(nullptr, camera.GetViewMatrix()));
	 commonSSR.cameraData.CameraPosition = camera.GetPosition();

	 //Transpose all matrices if necessary
	 commonSSR.cameraData.cameraProjMatrix = Matrix4(XMMatrixTranspose(commonSSR.cameraData.cameraProjMatrix));
	 commonSSR.cameraData.cameraViewMatrix = Matrix4(XMMatrixTranspose(commonSSR.cameraData.cameraViewMatrix));
	 commonSSR.cameraData.cameraWorldMatrix = Matrix4(XMMatrixTranspose(commonSSR.cameraData.cameraWorldMatrix));
	 commonSSR.cameraData.inverseProjMatrix = Matrix4(XMMatrixTranspose(commonSSR.cameraData.inverseProjMatrix));
	 commonSSR.cameraData.inverseViewMatrix = Matrix4(XMMatrixTranspose(commonSSR.cameraData.inverseViewMatrix));

	 commonSSR.ssrParameters.useSSR = true;
	 commonSSR.ssrParameters.width = width;
	 commonSSR.ssrParameters.height = height;
	 commonSSR.ssrParameters.maxSteps = 1000;
	 commonSSR.ssrParameters.maxLengthInWorldUnits = 1000;
	 commonSSR.ssrParameters.thicknessInUnits = 10;
	 commonSSR.ssrParameters.resolution = 1;

	 ComputeContext& cfx = ComputeContext::Begin(L"Surfel SSRM");
	 cfx.SetPipelineState(this->m_SSRMIrradiancePSO);
	 auto commandList = cfx.GetCommandList();
	 commandList->SetComputeRootSignature(this->m_rootSignature.GetSignature());
	 cfx.SetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, this->SRVHeap.GetHeapPointer());

	 cfx.SetDynamicConstantBufferView(0, sizeof(SurfelGenCB),genCB);
	 //gfxContext.SetDynamicConstantBufferView(1, sizeof(ProjectionResources),&m_ProjectionData);
	 cfx.SetDescriptorTable(2, SRVHeap[0]);
	 cfx.SetDescriptorTable(3, SRVHeap[3]);
	 cfx.SetDynamicConstantBufferView(5, sizeof(CommonSSR), &commonSSR);
	 //commandList->SetComputeRootShaderResourceView(1, m_topLevelAccelerationStructure->GetGPUVirtualAddress());

	 cfx.Dispatch1D(SurfelGI::_SURFEL_MAX_COUNT_,32);
	 cfx.Finish(true);
	//For now dispatch thread count equal to surfel count 
	//Group thread 64x64x1 to efficiently utilize warp space

	//For each surfel check if it is "on-screen"
	//The "on-screen" property would be written by the application shader is the surfel "contributes"
	//TODO this may be improves as not every surfel contributor will be on the screen, although most of them will
}
