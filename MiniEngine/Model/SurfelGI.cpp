#include "SurfelGI.h"
#include "CommandContext.h"
#include "Renderer.h"
#include "Math/Vector.h"
#include <limits>
#include "UniformGrid.h"
#include "ColorBuffer.h"


#include "CompiledShaders/SurfelAccelerationStructuresCS.h"
#include "CompiledShaders/SurfelGenerationCS.h"
#include "CompiledShaders/SurfelApplicationCS.h"


  void SurfelGI::UpdateProjection(const Camera& camera)
{
	Matrix4 invViewProj = Invert(camera.GetViewProjMatrix());
	Vector3 mathPos = camera.GetPosition();

	m_ProjectionData.invViewProjeciton = invViewProj;
	//projectionData.invViewProjeciton = Transpose(invViewProj);
	m_ProjectionData.depthNear = camera.GetNearClip();
	m_ProjectionData.depthFar = camera.GetFarClip();
	m_ProjectionData.fovY = camera.GetFOV();

	//m_ProjectoinBuffer.Create(L"Projectoin Data Buffer", 1, sizeof(ProjectionResources), &m_ProjectionData);

}






  void SurfelGI::Setup(GBufferPtrs gBuff)
{
	m_GBuffer = gBuff;

	CreateRootSig();

	SetDefaultCBData();

	InitializeBuffers();

	FillCPUContainers();

	CopyCPUContainersToRespectiveGPUBuffers();

	CreateHeaps();

	InitializePSOs();
}

  void SurfelGI::CopyCPUContainersToRespectiveGPUBuffers()
  {
	  GraphicsContext& context = GraphicsContext::Begin();
	  context.WriteBuffer(m_SurfelData, 0, m_SurfelDataArray.data(), _DEBUG_SURFEL_NUM * sizeof(SurfelData));
	  context.WriteBuffer(m_SurfelStack, 0, m_SurfelStackActual.data(), (_DEBUG_SURFEL_NUM + 1) * sizeof(UINT));
	  context.WriteBuffer(m_SurfelGrid, 0, m_SurfelGridActual.data(), (_CELL_COUNT_) * sizeof(UINT));
	  context.Finish();
  }

  void SurfelGI::CreateHeaps()
  {
	  nonShaderVisibleHeap.Create(L"SURFEL SRV HEAP", D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 7,false);
	  ExtendedUtility::CopyDescriptorsToHeap(nonShaderVisibleHeap, {
	      m_GBuffer.g_Depth->GetDepthSRV(),
	      m_GBuffer.g_Normal->GetSRV(),
	      m_SurfelData.GetUAV(),
	      m_SurfelList.GetUAV(),
	      m_SurfelGrid.GetUAV(),
	      m_SurfelStack.GetUAV(),
	      m_OutputTexture.GetUAV()
	      }
	  );



	  srvHeap.Create(L"SURFEL SRV HEAP", D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 7);

	  ExtendedUtility::CopyDescriptorsToHeap(srvHeap, {

		  m_GBuffer.g_Depth->GetDepthSRV(),
		  m_GBuffer.g_Normal->GetSRV(),
		  m_SurfelData.GetUAV(),
		  m_SurfelList.GetUAV(),
		  m_SurfelGrid.GetUAV(),
		  m_SurfelStack.GetUAV(),
	      m_OutputTexture.GetUAV()
		  }
	  );
  }

  void SurfelGI::InitializePSOs()
  {
	  //--- DEMO PASS FOR GENERATING SURFEL WITH COMPUTE SHADER ---
	  m_SurfelGenerationPSO.SetRootSignature(m_SurfelGenerationRT);
	  m_SurfelGenerationPSO.SetComputeShader(g_pSurfelGenerationCS, sizeof(g_pSurfelGenerationCS));
	  m_SurfelGenerationPSO.Finalize();

	  //Uses the same root signature but a different shader
	  m_SurfelApplicationPSO.SetRootSignature(m_SurfelGenerationRT);
	  m_SurfelApplicationPSO.SetComputeShader(g_pSurfelApplicationCS, sizeof(g_pSurfelApplicationCS));
	  m_SurfelApplicationPSO.Finalize();

	  m_SurfelAccelerationPassPSO.SetRootSignature(m_SurfelGenerationRT);
	  m_SurfelAccelerationPassPSO.SetComputeShader(g_pSurfelAccelerationStructuresCS, sizeof(g_pSurfelAccelerationStructuresCS));
	  m_SurfelAccelerationPassPSO.Finalize();

  }

  void SurfelGI::InitializeBuffers()
  {
	  m_ProjectoinBuffer.Create(L"Projectoin Data Buffer", 1, sizeof(ProjectionResources), &m_ProjectionData);
	  m_SufelSettingBuffer.Create(L"Surfel Gen CBV", 1, sizeof(SurfelGenCB), &m_SurfelGen);
	  //SURFEL SIZE STATIC BUFFER NUMB
	  m_SurfelData.Create(L"Surfel Data Buffer", _DEBUG_SURFEL_NUM, sizeof(SurfelData));
	  m_SurfelDataReadback.Create(L"Surfel Data Readback Buffer", _DEBUG_SURFEL_NUM, sizeof(SurfelData));
	  m_SurfelList.Create(L"Surfel List Buffer", _CELL_COUNT_, sizeof(UINT));
	  m_SurfelGrid.Create(L"Surfel Grid Buffer", _CELL_COUNT_, sizeof(UINT));
	  //+1 for the stack pointer itself
	  m_SurfelStack.Create(L"Surfel Stack", _DEBUG_SURFEL_NUM + 1, sizeof(UINT));
	  CreateOutputTexture(&Graphics::g_SceneNormalBuffer);
  }

  void SurfelGI::CreateRootSig()
  {
	  SamplerDesc DefaultSamplerDesc;
	  DefaultSamplerDesc.MaxAnisotropy = 8;
	  SamplerDesc CubeMapSamplerDesc = DefaultSamplerDesc;

	  m_SurfelGenerationRT.Reset(4, 3);


	  m_SurfelGenerationRT.InitStaticSampler(10, DefaultSamplerDesc);
	  m_SurfelGenerationRT.InitStaticSampler(11, Graphics::SamplerShadowDesc);
	  m_SurfelGenerationRT.InitStaticSampler(12, CubeMapSamplerDesc);

	  m_SurfelGenerationRT[0].InitAsConstantBuffer(0);
	  m_SurfelGenerationRT[1].InitAsConstantBuffer(1);
	  //SRVs: Position and Normal
	  m_SurfelGenerationRT[2].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 2);
	  //UAVs: 
	  m_SurfelGenerationRT[3].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 0, 5);
	  m_SurfelGenerationRT.Finalize(L"CS Surfel Root Signature");


//	  //Appliacatoin Root Signature
//
//	  m_SurfelGenerationRT.Reset(4, 3);
//
//	  m_SurfelGenerationRT.InitStaticSampler(10, DefaultSamplerDesc);
//	  m_SurfelGenerationRT.InitStaticSampler(11, Graphics::SamplerShadowDesc);
//	  m_SurfelGenerationRT.InitStaticSampler(12, CubeMapSamplerDesc);
//
//	  m_SurfelGenerationRT[0].InitAsConstantBuffer(0);
//	  m_SurfelGenerationRT[1].InitAsConstantBuffer(1);
//	  //SRVs: Position and Normal
//	  m_SurfelGenerationRT[2].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 3);
//	  //UAVs: 
//	  m_SurfelGenerationRT[3].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 0, 4);
//	  m_SurfelGenerationRT.Finalize(L"CS Surfel Root Signature");
  }

  void SurfelGI::SetDefaultCBData()
  {
	  m_SurfelGen.DepthThreshold = 0.1;
	  m_SurfelGen.FrameIndex = 0;
	  m_SurfelGen.MaxSurfels = 1000;
	  m_SurfelGen.NormalThreshold = 0.5;
	  m_SurfelGen.ViewDistThreshold = 0.75;

	  m_SurfelGen.UniformGrid.cellSize = Vector4(100, 100, 100, 100);
	  m_SurfelGen.UniformGrid.gridOrigin = Vector4(-2000, -2000, -2000, -2000);
	  //m_SurfelGen.UniformGrid.dimensions = Vector4(+2000, +2000, +2000, +2000);
	  m_SurfelGen.UniformGrid.dimensions = Vector4(4000,4000,4000,4000);

	UINT grdCells[3] = {
		m_SurfelGen.UniformGrid.dimensions.GetX() / m_SurfelGen.UniformGrid.cellSize.GetX(),
		m_SurfelGen.UniformGrid.dimensions.GetY() / m_SurfelGen.UniformGrid.cellSize.GetY(),
		m_SurfelGen.UniformGrid.dimensions.GetZ() / m_SurfelGen.UniformGrid.cellSize.GetZ()
	};
	auto grid = m_SurfelGen.UniformGrid;

	_CELL_COUNT_ = grdCells[0] * grdCells[1] * grdCells[2];

  }

void SurfelGI::FillCPUContainers()
{

	const SurfelData data{
		Vector4(0, 0, 1,1),
		Vector4(0, 0, 1,1),
		150,
	}
	;
	for (size_t i = 0; i < _DEBUG_SURFEL_NUM; i++)
	{
		m_SurfelDataArray.push_back(data);
	}
	// Fill data
	for (int i = 0; i < _DEBUG_SURFEL_NUM; ++i) {
		m_SurfelDataArray[i].position =
			Math::Vector4(float(i) * m_SurfelGen.UniformGrid.cellSize.GetX(), 0.0f, 0.0f, 1.0f);
		m_SurfelDataArray[i].radius = Math::Vector4(0.5f, 0.5f, 0.5f, 0.5f);
		m_SurfelDataArray[i].normal = Math::Vector4(0.0f, 1.0f, 0.0f, 1.0f);
		m_SurfelDataArray[i].pixelPosX = 0;
		m_SurfelDataArray[i].pixelPosY = 0;
		m_SurfelDataArray[i].tilePosX = 0;
		m_SurfelDataArray[i].tilePosY = 0;
		m_SurfelDataArray[i].raySamples = 1;
		m_SurfelDataArray[i].covarianceRow0 = XMFLOAT4(1,0,0,0);
		m_SurfelDataArray[i].covarianceRow1 = XMFLOAT4(0,2,0,0);
		m_SurfelDataArray[i].covarianceRow2 = XMFLOAT4(0,0,3,0);
		m_SurfelDataArray[i].mean = Math::Vector4(0,1,2,3);
	}

	//Fill the surfle stack buffer
	//The first index is the surfel stack pointer and should be set to 0
	//E.g pointer set to 0 would actually read memory adress at 1.
	for (int i = 0; i < _CELL_COUNT_; ++i) {
		m_SurfelGridActual.push_back(0);
	}


	//Fill the surfle stack buffer
	//The first index is the surfel stack pointer and should be set to 0
	//E.g pointer set to 0 would actually read memory adress at 1.
	for (int i = 0; i < _DEBUG_SURFEL_NUM + 1; ++i) {
		m_SurfelStackActual.push_back(i);
	}
}

void SurfelGI::CreateOutputTexture(ColorBuffer* outputBuffer)
{
	m_OutputTexture.Create(L"RayTracingOutput", outputBuffer->GetWidth(), outputBuffer->GetHeight(), 1, DXGI_FORMAT_R8G8B8A8_UNORM);
}

  void SurfelGI::SpawnSurfels(ComputeContext& gfxContext,  const Camera& camera)
{

	ScopedTimer _prof(L"Dispact Compute Shader", gfxContext);

	//Transition resources from render target to CS 
	//NON-PIXEL SHADER RESOURCE should cover the ComputeShader stage
	gfxContext.TransitionResource(*m_GBuffer.g_Depth, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, true);
	gfxContext.TransitionResource(*m_GBuffer.g_Normal, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, true);

	//Switch to the appropriate PSO
	gfxContext.SetPipelineState(m_SurfelGenerationPSO);
	gfxContext.SetRootSignature(m_SurfelGenerationRT);
	SendParameters(gfxContext, camera);

	//Dispatch grid number
	const UINT TEX_SIZE_X = m_GBuffer.g_Normal->GetWidth();
	const UINT TEX_SIZE_Y = m_GBuffer.g_Normal->GetHeight();

	const UINT THREAD_GROUP_X = 16;
	const UINT THREAD_GROUP_Y = 16;
	//Mini Engine Internally uses ceilign division to supply enoug threads
	gfxContext.Dispatch2D(TEX_SIZE_X,TEX_SIZE_Y,THREAD_GROUP_X,THREAD_GROUP_Y);
}

  void SurfelGI::SendParametersGraphics(GraphicsContext& gfxContext,const Camera& camera)
  {

	ID3D12DescriptorHeap* heaps[] = {
		srvHeap.GetHeapPointer()
	};
	//Update the projection from camera
	UpdateProjection(camera);

	gfxContext.GetCommandList()->SetDescriptorHeaps(1, heaps);
	//gfxContext.SetConstantBuffer(0, m_SufelSettingBuffer.GetGpuVirtualAddress());
	gfxContext.SetDynamicConstantBufferView(0, sizeof(SurfelGenCB),&m_SurfelGen);
	gfxContext.SetDynamicConstantBufferView(1, sizeof(ProjectionResources),&m_ProjectionData);
	//gfxContext.SetConstantBuffer(1, m_ProjectoinBuffer.GetGpuVirtualAddress());
	gfxContext.SetDescriptorTable(2, srvHeap[0]);
	gfxContext.SetDescriptorTable(3, srvHeap[2]);
  }

  void SurfelGI::SendParameters(ComputeContext& gfxContext,const Camera& camera)
  {

	ID3D12DescriptorHeap* heaps[] = {
		srvHeap.GetHeapPointer()
	};
	//TODO make use of dynamic upload buffers for per-frame data changes
//	ByteAddressBuffer uploadBuffer;
//	uploadBuffer.Create(L"Surfel GEN UPLAOD", 1, sizeof(SurfelGenCB), &m_SurfelGen);
//	// Update the surfel setting buffer with the new frame index
//	CommandContext& context = CommandContext::Begin(L"Update Surfel Generation CB");
	//context.CopyBuffer(m_SufelSettingBuffer, uploadBuffer);
	//context.Finish(true);
	//Update the projection from camera
	UpdateProjection(camera);

	gfxContext.GetCommandList()->SetDescriptorHeaps(1, heaps);
	//gfxContext.SetConstantBuffer(0, m_SufelSettingBuffer.GetGpuVirtualAddress());
	//gfxContext.SetConstantBuffer(1, m_ProjectoinBuffer.GetGpuVirtualAddress());
	gfxContext.SetDynamicConstantBufferView(0, sizeof(SurfelGenCB),&m_SurfelGen);
	gfxContext.SetDynamicConstantBufferView(1, sizeof(ProjectionResources),&m_ProjectionData);
	gfxContext.SetDescriptorTable(2, srvHeap[0]);
	gfxContext.SetDescriptorTable(3, srvHeap[2]);
  }

  void SurfelGI::FillAccelerationStructures(ComputeContext& gfxContext)
{

	ScopedTimer _prof(L"Disaptch Surfel Fill Acceleration Structures", gfxContext);

	//Transition resources from render target to CS 
	//NON-PIXEL SHADER RESOURCE should cover the ComputeShader stage
	gfxContext.TransitionResource(*m_GBuffer.g_Depth, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, false);
	gfxContext.TransitionResource(*m_GBuffer.g_Normal, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, true);

	//Switch to the appropriate PSO
	gfxContext.SetPipelineState(m_SurfelAccelerationPassPSO);
	gfxContext.SetRootSignature(m_SurfelGenerationRT);


	ID3D12DescriptorHeap* heaps[] = {
		srvHeap.GetHeapPointer(),  // This is your SURFEL SRV HEAP
	};

	gfxContext.GetCommandList()->SetDescriptorHeaps(1, heaps);




	// Write to GPU buffer
	//Reset surfel grid buffer
	gfxContext.WriteBuffer(m_SurfelGrid, 0, m_SurfelGridActual.data(), (_CELL_COUNT_) * sizeof(UINT));

	//Bind the root parameters
	gfxContext.SetConstantBuffer(0, m_SufelSettingBuffer.GetGpuVirtualAddress());
	gfxContext.SetConstantBuffer(1, m_ProjectoinBuffer.GetGpuVirtualAddress());
	gfxContext.SetDescriptorTable(2, srvHeap[0]);
	gfxContext.SetDescriptorTable(3, srvHeap[2]);
	gfxContext.Dispatch(1, 1, 1);

}

  void SurfelGI::ReadbackSurfelData(GraphicsContext& gfx)
{

	gfx.CopyBuffer(m_SurfelDataReadback, m_SurfelData);

	void* mappedData = m_SurfelDataReadback.Map();
	memcpy(m_SurfelDataArray.data(), mappedData, _DEBUG_SURFEL_NUM * sizeof(SurfelData));
	m_SurfelDataReadback.Unmap();

}

  void SurfelGI::ApplySurfels(ComputeContext& gfxContext,const Camera& camera)
  {

	ScopedTimer _prof(L"Surfel Application Compute Shader", gfxContext);

	//Transition resources from render target to CS 
	//NON-PIXEL SHADER RESOURCE should cover the ComputeShader stage
	gfxContext.TransitionResource(*m_GBuffer.g_Depth, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, true);
	gfxContext.TransitionResource(*m_GBuffer.g_Normal, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, true);

	//Switch to the appropriate PSO
	gfxContext.SetPipelineState(m_SurfelApplicationPSO);
	gfxContext.SetRootSignature(m_SurfelGenerationRT);
	SendParameters(gfxContext, camera);

	//Dispatch grid number
	const UINT TEX_SIZE_X = m_GBuffer.g_Normal->GetWidth();
	const UINT TEX_SIZE_Y = m_GBuffer.g_Normal->GetHeight();

	const UINT THREAD_GROUP_X = 1;
	const UINT THREAD_GROUP_Y = 1;
	//Mini Engine Internally uses ceilign division to supply enoug threads
	gfxContext.Dispatch2D(TEX_SIZE_X,TEX_SIZE_Y,THREAD_GROUP_X,THREAD_GROUP_Y);

  }

  int SurfelGI::GetClosestSurfelToPosition(Vector3 worldPos)
{
	float minDistSq = (std::numeric_limits<float>::max)();
	int closestIndex = -1;

	for (int i = 2; i < m_SurfelDataArray.size(); ++i)
	{
		const SurfelData& s = m_SurfelDataArray[i];
		Vector3 diff = Vector3(s.position) - worldPos;
		float distSq = Math::LengthSquare(diff);

		if (distSq < minDistSq)
		{
			minDistSq = distSq;
			closestIndex = i;
		}
	}

	return closestIndex;
}
