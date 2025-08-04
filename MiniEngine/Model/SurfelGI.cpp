#include "SurfelGI.h"
#include "CommandContext.h"
#include "Renderer.h"
#include "Math/Vector.h"
#include <limits>
#include "UniformGrid.h"
#include "ColorBuffer.h"
#include <cmath>
#include <d3dcompiler.h>


#include "CompiledShaders/SurfelAccelerationStructuresCS.h"
#include "CompiledShaders/SurfelGenerationCS.h"
#include "CompiledShaders/SurfelApplicationCS.h"
#include "CompiledShaders/SurfelRecyclingCS.h"
//Reduce step of of Reduce-Scan prefix sum
#include "CompiledShaders/SurfelFASSurfelInclusivePrefixSumRCS.h"
//Scan step of of Reduce-Scan prefix sum
#include "CompiledShaders/SurfelFASSurfelInclusivePrefixSumSCS.h"
//Inclusive Propagate step of of Reduce-Scan prefix sum
#include "CompiledShaders/SurfelFASSurfelInclusivePrefixSumIPCS.h"
#include "CompiledShaders/SurfelFASSurfelInsertionCS.h"
#include "CompiledShaders/SurfelFASSurfelCountCS.h"


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
	  m_SurfelList.Create();
	  m_SurfelGrid.Create();
	  m_SurfelData.Create();
	  m_SurfelStack.Create();
	  //context.WriteBuffer(m_SurfelData.m_GPUBuffer, 0, m_SurfelData.m_Actual.data(), _SURFEL_MAX_COUNT_ * sizeof(SurfelData));
	  //context.WriteBuffer(m_SurfelData.m_GPUBuffer, 0, m_SurfelStack.m_Actual.data(), (_SURFEL_MAX_COUNT_ + 2) * sizeof(UINT));
	  //context.WriteBuffer(m_SurfelGrid.m_GPUBuffer, 0, m_SurfelGrid.m_Actual.data(), (_CELL_COUNT_) * sizeof(UINT));

	  context.Finish();
  }

  void SurfelGI::CreateHeaps()
  {
	  nonShaderVisibleHeap.Create(L"SURFEL SRV HEAP", D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 8,false);
	  ExtendedUtility::CopyDescriptorsToHeap(nonShaderVisibleHeap, {
	      m_GBuffer.g_Depth->GetDepthSRV(),
	      m_GBuffer.g_Normal->GetSRV(),
	      m_SurfelData.m_GPUBuffer.GetUAV(),
	      m_SurfelList.m_GPUBuffer.GetUAV(),
	      m_SurfelGrid.m_GPUBuffer.GetUAV(),
	      m_SurfelStack.m_GPUBuffer.GetUAV(),
	      m_OutputTexture.GetUAV(),
	      m_SurfelDebug.GetUAV()
	      }
	  );


	  descriptorHeap.Create(L"SURFEL SRV HEAP", D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 8);

	  ExtendedUtility::CopyDescriptorsToHeap(descriptorHeap, {

		  m_GBuffer.g_Depth->GetDepthSRV(),
		  m_GBuffer.g_Normal->GetSRV(),
		  m_SurfelData.m_GPUBuffer.GetUAV(),
		  m_SurfelList.m_GPUBuffer.GetUAV(),
		  m_SurfelGrid.m_GPUBuffer.GetUAV(),
		  m_SurfelStack.m_GPUBuffer.GetUAV(),
	      m_OutputTexture.GetUAV(),
	      m_SurfelDebug.GetUAV()
		  }
	  );


	  reduceThenScanPSHeap.Create(L"Reduce Then Scan  HEAP", D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 3);

	  ExtendedUtility::CopyDescriptorsToHeap(reduceThenScanPSHeap, {
		  m_PrefixSumInput.GetUAV(),
		  m_SurfelGrid.m_GPUBuffer.GetUAV(),
		  m_ReductionBuffer.GetUAV()
		  }
	  );


  }

  void SurfelGI::InitializePSOs()
  {
	  //--- DEMO PASS FOR GENERATING SURFEL WITH COMPUTE SHADER ---
	  m_GenerationPassPSO.SetRootSignature(m_SurfelGenerationRT);
	  m_GenerationPassPSO.SetComputeShader(g_pSurfelGenerationCS, sizeof(g_pSurfelGenerationCS));
	  m_GenerationPassPSO.Finalize();

	  //Uses the same root signature but a different shader
	  m_ApplicationPassPSO.SetRootSignature(m_SurfelGenerationRT);
	  m_ApplicationPassPSO.SetComputeShader(g_pSurfelApplicationCS, sizeof(g_pSurfelApplicationCS));
	  m_ApplicationPassPSO.Finalize();

	  m_AccelerationPassPSO.SetRootSignature(m_SurfelGenerationRT);
	  m_AccelerationPassPSO.SetComputeShader(g_pSurfelAccelerationStructuresCS, sizeof(g_pSurfelAccelerationStructuresCS));
	  m_AccelerationPassPSO.Finalize();



	  m_AccelerationPassSurfelCountPSO.SetRootSignature(m_SurfelGenerationRT);
	  m_AccelerationPassSurfelCountPSO.SetComputeShader(g_pSurfelFASSurfelCountCS, sizeof(g_pSurfelFASSurfelCountCS));
	  m_AccelerationPassSurfelCountPSO.Finalize();

	  m_AccelerationPassPrefixSumReducePSO.SetRootSignature(m_ReduceThenScanRT);
	  m_AccelerationPassPrefixSumReducePSO.SetComputeShader(g_pSurfelFASSurfelInclusivePrefixSumRCS, sizeof(g_pSurfelFASSurfelInclusivePrefixSumRCS));
	  m_AccelerationPassPrefixSumReducePSO.Finalize();

	  m_AccelerationPassPrefixSumScanPSO.SetRootSignature(m_ReduceThenScanRT);
	  m_AccelerationPassPrefixSumScanPSO.SetComputeShader(g_pSurfelFASSurfelInclusivePrefixSumSCS, sizeof(g_pSurfelFASSurfelInclusivePrefixSumSCS));
	  m_AccelerationPassPrefixSumScanPSO.Finalize();

	  m_AccelerationPassPrefixSumInclusivePropagatePSO.SetRootSignature(m_ReduceThenScanRT);
	  m_AccelerationPassPrefixSumInclusivePropagatePSO.SetComputeShader(g_pSurfelFASSurfelInclusivePrefixSumIPCS, sizeof(g_pSurfelFASSurfelInclusivePrefixSumIPCS));
	  m_AccelerationPassPrefixSumInclusivePropagatePSO.Finalize();

	  m_AccelerationPassSurfelInsertionPSO.SetRootSignature(m_SurfelGenerationRT);
	  m_AccelerationPassSurfelInsertionPSO.SetComputeShader(g_pSurfelFASSurfelInsertionCS, sizeof(g_pSurfelFASSurfelInsertionCS));
	  m_AccelerationPassSurfelInsertionPSO.Finalize();

	  m_RecyclingPassPSO.SetRootSignature(m_SurfelGenerationRT);
	  m_RecyclingPassPSO.SetComputeShader(g_pSurfelRecyclingCS, sizeof(g_pSurfelRecyclingCS));
	  m_RecyclingPassPSO.Finalize();

  }

  void SurfelGI::InitializeBuffers()
  {
	  m_ProjectoinBuffer.Create(L"Projectoin Data Buffer", 1, sizeof(ProjectionResources), &m_ProjectionData);
	  m_SufelSettingBuffer.Create(L"Surfel Gen CBV", 1, sizeof(SurfelGenCB), &m_SurfelGen);
	  //SURFEL SIZE STATIC BUFFER NUMB
	  m_SurfelData = MultiElementCommunicationBuffer<SurfelData>(L"Surfel Data Buffer", _SURFEL_MAX_COUNT_);

//	  m_SurfelData.m_GPUBuffer.Create(L"Surfel Data Buffer", _SURFEL_MAX_COUNT_, sizeof(SurfelData));
//	  m_SurfelDataReadback.Create(L"Surfel Data Readback Buffer", _SURFEL_MAX_COUNT_, sizeof(SurfelData));

	  //Size should be handling the worst case scenario _CELL_COUNT_ * kSurfelPerCells
	  const UINT surfelListSize = _CELL_COUNT_ * _SURFEL_PER_CELL;
	  m_SurfelList = MultiElementCommunicationBuffer<UINT>(L"Surfel List Buffer", surfelListSize);
	  m_SurfelGrid = MultiElementCommunicationBuffer<UINT>(L"Surfel Grid Buffer", _CELL_COUNT_);

	  m_SurfelGridResetValues = std::vector<UINT>(_CELL_COUNT_, 0);


	  m_ReductionBuffer.Create(L"ReduceThenScan Reduction Buffer", _CELL_COUNT_, sizeof(UINT));
	  m_PrefixSumInput.Create(L"ReduceThenScan Prefix Sum Input Copy", _CELL_COUNT_, sizeof(UINT));
	  m_PrefixSumBuffer.Create(L"ReduceThenScan Prefix Sum Buffer", 1, sizeof(PrefixSum));

	  //+1 for the stack pointer itself
	  m_SurfelStack = MultiElementCommunicationBuffer<UINT>(L"Surfel Stack", _SURFEL_MAX_COUNT_ + 2);
	  //m_SurfelData.m_GPUBuffer.Create(L"Surfel Stack", _SURFEL_MAX_COUNT_ + 2, sizeof(UINT));
	  m_SurfelDebug.Create(L"Surfel Debug UAV", 1, sizeof(SurfelDebugData));
	  m_SurfelDebugReadback.Create(L"Surfel Debug Data Readback Buffer", 1, sizeof(SurfelDebugData));
	  CreateOutputTexture(&Graphics::g_SceneNormalBuffer);
  }

  void SurfelGI::RecreateSurfelListBuffers(int newNum)
  {
//	  m_surfelNum = newNum;
//	  m_SurfelGen.MaxSurfels = (UINT)m_surfelNum;
//	  m_SurfelData.m_GPUBuffer.Create(L"Surfel Data Buffer", newNum, sizeof(SurfelData));
//	  m_SurfelData.m_GPUBuffer.Create(L"Surfel Stack", m_surfelNum + 2, sizeof(UINT));
//	  m_SurfelDataReadback.Create(L"Surfel Data Readback Buffer", newNum, sizeof(SurfelData));
//	  CreateHeaps();
//	  FillCPUContainers();


  }

  void SurfelGI::TransitionResourcesTo()
  {
  }

  void SurfelGI::ResetSurfels(GraphicsContext& gfx)
  {

	  for (int i = 0; i < _SURFEL_MAX_COUNT_ + 2; ++i) {
		  //The surfel indices must be in a sequential order
		  m_SurfelStack.m_Actual[i] = i - 2;
	  }
	  //Make sure pointer is set to the second element of stack
	  m_SurfelStack.m_Actual[0] = 0 + 2;
	  //Size of generated surfels must be zero
	  m_SurfelStack.m_Actual[1] = 0;

	for (int i = 0; i < _SURFEL_MAX_COUNT_; ++i) {
		m_SurfelData.m_Actual[i].position = Math::Vector4(-99999, -99999, -99999, -99999);
		m_SurfelData.m_Actual[i].radius = Math::Vector4(0, 0, 0, 0);
		m_SurfelData.m_Actual[i].normal = Math::Vector4(0.0f, 1.0f, 0.0f, 1.0f);
		m_SurfelData.m_Actual[i].pixelPosX = 0;
		m_SurfelData.m_Actual[i].pixelPosY = 0;
		m_SurfelData.m_Actual[i].tilePosX = 0;
		m_SurfelData.m_Actual[i].tilePosY = 0;
		m_SurfelData.m_Actual[i].raySamples = 0;
		m_SurfelData.m_Actual[i].contribution0 = 0;
		m_SurfelData.m_Actual[i].contribution1 = 0;
		m_SurfelData.m_Actual[i].contribution3 = 0;
		m_SurfelData.m_Actual[i].contribution2 = 0;
		m_SurfelData.m_Actual[i].meanOne = Math::Vector4(0, 0, 0, 0);
		m_SurfelData.m_Actual[i].mean = Math::Vector4(0, 0, 0, 0);
	}
	m_SurfelData.Write(gfx, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	m_SurfelStack.Write(gfx, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);



  }

  void SurfelGI::ResetSurfelsIrradiance(GraphicsContext& gfx)
  {
	for (int i = 0; i < _SURFEL_MAX_COUNT_; ++i) {
		m_SurfelData.m_Actual[i].color = Math::Vector4(0, 0, 0, 0);
		m_SurfelData.m_Actual[i].varianceAndInconsistency = Math::Vector4(0, 0, 0, 1);
	}
	m_SurfelData.Write(gfx, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
  }

  void SurfelGI::CreateRootSig()
  {
	  SamplerDesc DefaultSamplerDesc;
	  DefaultSamplerDesc.MaxAnisotropy = 1;
	  DefaultSamplerDesc.Filter = D3D12_FILTER_MAXIMUM_MIN_MAG_MIP_POINT;
	  SamplerDesc CubeMapSamplerDesc = DefaultSamplerDesc;

	  m_SurfelGenerationRT.Reset(5, 3);


	  m_SurfelGenerationRT.InitStaticSampler(10, DefaultSamplerDesc);
	  m_SurfelGenerationRT.InitStaticSampler(11, Graphics::SamplerShadowDesc);
	  m_SurfelGenerationRT.InitStaticSampler(12, CubeMapSamplerDesc);

	  m_SurfelGenerationRT[0].InitAsConstantBuffer(0);
	  m_SurfelGenerationRT[1].InitAsConstantBuffer(1);
	  //SRVs: Position and Normal
	  m_SurfelGenerationRT[2].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 2);
	  //UAVs: 
	  m_SurfelGenerationRT[3].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 0, 5);
	  //Bind Debug UAVS at space 1
	  m_SurfelGenerationRT[4].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 0, 1,D3D12_SHADER_VISIBILITY_ALL,1);


	  m_SurfelGenerationRT.Finalize(L"CS Surfel Root Signature");


	  m_ReduceThenScanRT.Reset(2, 0);
	  m_ReduceThenScanRT[0].InitAsConstantBuffer(0);
	  //SRVs: Position and Normal
	  m_ReduceThenScanRT[1].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 0, 3);
	  m_ReduceThenScanRT.Finalize(L"CS Reduthe Then Scan Root Signature");


  }
 void SurfelGI::SetDefaultCBData()
  {
	  m_SurfelGen.DepthThreshold = 0.1;
	  m_SurfelGen.FrameIndex = 0;
	  m_SurfelGen.MaxSurfels = _SURFEL_MAX_COUNT_;
	  m_SurfelGen.CurrentSurfelCount = 0;
	  m_SurfelGen.NormalThreshold = 0.5;
	  m_SurfelGen.kPerCellSurfelLimit = _SURFEL_PER_CELL;
	  m_SurfelGen.minRadius = 15;
	  m_SurfelGen.maxRadius = 90;

	  m_SurfelGen.UniformGrid.cellSize = Vector4(100, 100, 100, 100);
	  m_SurfelGen.UniformGrid.gridOrigin = Vector4(-2000, -2000, -2000, -2000);
	  m_SurfelGen.UniformGrid.dimensions = Vector4(4000,4000,4000,4000);

	UINT grdCells[3] = {
		m_SurfelGen.UniformGrid.dimensions.GetX() / m_SurfelGen.UniformGrid.cellSize.GetX(),
		m_SurfelGen.UniformGrid.dimensions.GetY() / m_SurfelGen.UniformGrid.cellSize.GetY(),
		m_SurfelGen.UniformGrid.dimensions.GetZ() / m_SurfelGen.UniformGrid.cellSize.GetZ()
	};
	auto grid = m_SurfelGen.UniformGrid;

	_CELL_COUNT_ = grdCells[0] * grdCells[1] * grdCells[2];
	_CELL_COUNT_ = GetVectorizedSize(_CELL_COUNT_, 4) * 4;

  }

void SurfelGI::FillCPUContainers()
{
//	m_SurfelData.m_Actual.clear();
//	m_SurfelStack.m_Actual.clear();
//	m_SurfelGrid.m_Actual.clear();
//	m_SurfelList.m_Actual.clear();

	m_SurfelDebugActual.PointedCellX = 1;
	m_SurfelDebugActual.PointedCellY = 2;
	m_SurfelDebugActual.PointedCellZ = 3;
	m_SurfelDebugActual.PointedCellW = 4;

	const SurfelData data{
		Vector4(0, 0, 1,1),
		Vector4(0, 0, 1,1),
		150,
	}
	;
	for (size_t i = 0; i < _SURFEL_MAX_COUNT_; i++)
	{
		//m_SurfelData.m_Actual.push_back(data);
		m_SurfelData.m_Actual[i]= data;
	}
	// Fill data
	for (int i = 0; i < _SURFEL_MAX_COUNT_; ++i) {
//		m_SurfelData.m_Actual[i].position =
//			Math::Vector4(float(i) * m_SurfelGen.UniformGrid.cellSize.GetX(), 0.0f, 0.0f, 1.0f);
		//m_SurfelData.m_Actual[i].position =Math::Vector4(0,0,0,0);
		m_SurfelData.m_Actual[i].position =Math::Vector4(-99999,-99999,-99999,-99999);
		m_SurfelData.m_Actual[i].radius = Math::Vector4(0,0,0,0);
		m_SurfelData.m_Actual[i].normal = Math::Vector4(0.0f, 1.0f, 0.0f, 1.0f);
		m_SurfelData.m_Actual[i].pixelPosX = 0;
		m_SurfelData.m_Actual[i].pixelPosY = 0;
		m_SurfelData.m_Actual[i].tilePosX = 0;
		m_SurfelData.m_Actual[i].tilePosY = 0;
		m_SurfelData.m_Actual[i].raySamples = 0;
		m_SurfelData.m_Actual[i].contribution0 = 0;
		m_SurfelData.m_Actual[i].contribution1= 0;
		m_SurfelData.m_Actual[i].contribution3 = 0;
		m_SurfelData.m_Actual[i].contribution2 = 0;
		m_SurfelData.m_Actual[i].meanOne = Math::Vector4(0,0,0,0);
		m_SurfelData.m_Actual[i].mean = Math::Vector4(0, 0, 0, 0);
	}

	//Fill the surfle stack buffer
	//The first index is the surfel stack pointer and should be set to 0
	//E.g pointer set to 0 would actually read memory adress at 1.
	for (int i = 0; i < m_SurfelGrid.m_Actual.size(); ++i) {
		m_SurfelGrid.m_Actual[i]=	0;
		//m_SurfelGrid.m_Actual.push_back(0);
		//m_SurfelList.m_Actual.push_back(0);
	}
	for (int i = 0; i < m_SurfelList.m_Actual.size(); ++i) {
		m_SurfelList.m_Actual[i]=	0;
		//m_SurfelGrid.m_Actual.push_back(0);
		//m_SurfelList.m_Actual.push_back(0);
	}


	//Fill the surfle stack buffer
	//The first index is the surfel stack pointer and should be set to 0
	//E.g pointer set to 0 would actually read memory adress at 1.
	for (int i = 0; i < _SURFEL_MAX_COUNT_ + 2; ++i) {
		m_SurfelStack.m_Actual[i]=i-2;
	}
	m_SurfelStack.m_Actual[0] = 0 + 2;
	m_SurfelStack.m_Actual[1] = 0;
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

	gfxContext.InsertUAVBarrier(this->m_SurfelGrid.m_GPUBuffer);
	gfxContext.InsertUAVBarrier(this->m_SurfelList.m_GPUBuffer);
	gfxContext.InsertUAVBarrier(this->m_SurfelData.m_GPUBuffer);
	gfxContext.InsertUAVBarrier(this->m_SurfelData.m_GPUBuffer);

	//Switch to the appropriate PSO
	gfxContext.SetPipelineState(m_GenerationPassPSO);
	gfxContext.SetRootSignature(m_SurfelGenerationRT);
	UpdateProjection( camera);
	SendParameters(gfxContext);

	//Dispatch grid number
	const UINT TEX_SIZE_X = m_GBuffer.g_Normal->GetWidth();
	const UINT TEX_SIZE_Y = m_GBuffer.g_Normal->GetHeight();

	const UINT THREAD_GROUP_X = 16;
	const UINT THREAD_GROUP_Y = 16;
	//Mini Engine Internally uses ceilign division to supply enoug threads
	gfxContext.Dispatch2D(TEX_SIZE_X,TEX_SIZE_Y,THREAD_GROUP_X,THREAD_GROUP_Y);
}

  void SurfelGI::SendParametersGraphics(GraphicsContext& gfxContext)
  {

	ID3D12DescriptorHeap* heaps[] = {
		descriptorHeap.GetHeapPointer()
	};

	gfxContext.GetCommandList()->SetDescriptorHeaps(1, heaps);
	//gfxContext.SetConstantBuffer(0, m_SufelSettingBuffer.GetGpuVirtualAddress());
	gfxContext.SetDynamicConstantBufferView(0, sizeof(SurfelGenCB),&m_SurfelGen);
	gfxContext.SetDynamicConstantBufferView(1, sizeof(ProjectionResources),&m_ProjectionData);
	//gfxContext.SetConstantBuffer(1, m_ProjectoinBuffer.GetGpuVirtualAddress());
	gfxContext.SetDescriptorTable(2, descriptorHeap[0]);
	gfxContext.SetDescriptorTable(3, descriptorHeap[2]);
	gfxContext.SetDescriptorTable(4, descriptorHeap[7]);
  }

  void SurfelGI::SendParameters(ComputeContext& gfxContext)
  {

	ID3D12DescriptorHeap* heaps[] = {
		descriptorHeap.GetHeapPointer()
	};


	gfxContext.GetCommandList()->SetDescriptorHeaps(1, heaps);
	gfxContext.SetDynamicConstantBufferView(0, sizeof(SurfelGenCB),&m_SurfelGen);
	gfxContext.SetDynamicConstantBufferView(1, sizeof(ProjectionResources),&m_ProjectionData);
	gfxContext.SetDescriptorTable(2, descriptorHeap[0]);
	gfxContext.SetDescriptorTable(3, descriptorHeap[2]);
	gfxContext.SetDescriptorTable(4, descriptorHeap[7]);
  }

  void SurfelGI::FillAccelerationStructures(ComputeContext& gfxContext)
{

	ScopedTimer _prof(L"Disaptch Surfel Fill Acceleration Structures", gfxContext);

	//Transition resources from render target to CS 
	//NON-PIXEL SHADER RESOURCE should cover the ComputeShader stage
	gfxContext.TransitionResource(*m_GBuffer.g_Depth, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, false);
	gfxContext.TransitionResource(*m_GBuffer.g_Normal, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, true);


	gfxContext.SetRootSignature(m_SurfelGenerationRT);
	gfxContext.SetPipelineState(m_AccelerationPassPSO);

	SendParameters(gfxContext);
	
	  m_SurfelGrid.WriteFromExternal(gfxContext.GetGraphicsContext(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, m_SurfelGridResetValues,true);
	//gfxContext.WriteBuffer(m_SurfelGrid.m_GPUBuffer, 0, m_SurfelGrid.m_Actual.data(), (_CELL_COUNT_) * sizeof(UINT));
//	const float groupX = 256.0f;
//	UINT dispatchX = std::ceil((float)_SURFEL_MAX_COUNT_ / groupX);
	gfxContext.InsertUAVBarrier(m_SurfelGrid.m_GPUBuffer);
	gfxContext.InsertUAVBarrier(m_SurfelList.m_GPUBuffer);

	gfxContext.Dispatch(1, 1, 1);


}

  void SurfelGI::FillAccelerationStructuresReduceThenScan(ComputeContext& gfxContext)
  {

	ScopedTimer _prof(L"Disaptch Surfel Fill Acceleration Structures", gfxContext);

	//Transition resources from render target to CS 
	//NON-PIXEL SHADER RESOURCE should cover the ComputeShader stage
	gfxContext.TransitionResource(*m_GBuffer.g_Depth, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, false);
	gfxContext.TransitionResource(*m_GBuffer.g_Normal, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, true);

	FASSurfelCount(gfxContext);
	 FASInclusivePrefixSum(gfxContext);
	FASSurfelInsertion(gfxContext);
//	const float groupX = 256.0f;
//	UINT dispatchX = std::ceil((float)_SURFEL_MAX_COUNT_ / groupX);

//	gfxContext.Dispatch(1, 1, 1);

  }

  void SurfelGI::CopyPrefixInput(ComputeContext& context, StructuredBuffer* srcBuffer,StructuredBuffer* destBuffer)
  {
	  //GraphicsContext& context = gfxContext::Begin(L"Copy Structured Buffer");
	  ScopedTimer _prof(L"Surfel Copy Grid To Input Buffer", context);

	  context.TransitionResource(*srcBuffer, D3D12_RESOURCE_STATE_COPY_SOURCE,true);
	  context.TransitionResource(*destBuffer, D3D12_RESOURCE_STATE_COPY_DEST,true);

	  // Issue the copy
	  context.GetCommandList()->CopyBufferRegion(
		  destBuffer->GetResource(),
		  0,
		  srcBuffer->GetResource(),
		  0,
		  srcBuffer->GetBufferSize()
	  );

	  context.TransitionResource(*srcBuffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS,true);
	  context.TransitionResource(*destBuffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS,true);

	  // Close and execute
	  //context.Finish(true);
  }

  void SurfelGI::FASSurfelCount(ComputeContext& gfxContext)
  {

	  //gfxContext.InsertUAVBarrier(m_SurfelGrid.m_GPUBuffer);
	  //This should always be setting it to zeroes and the current actual "readback"  data;
	  m_SurfelGrid.WriteFromExternal(gfxContext.GetGraphicsContext(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, m_SurfelGridResetValues,true);
	  //gfxContext.WriteBuffer(m_SurfelGrid.m_GPUBuffer, 0, m_SurfelGridResetValues.data(), (_CELL_COUNT_) * sizeof(UINT));

	  //Switch to the appropriate PSO
	  gfxContext.SetPipelineState(m_AccelerationPassSurfelCountPSO);
	  gfxContext.SetRootSignature(m_SurfelGenerationRT);

	  //Reset surfel grid buffer
	  SendParameters(gfxContext);


	  ScopedTimer _prof(L"Surfel Fill Acceleration Structures : SURFEL COUNT", gfxContext);
	  const float groupX = 256.0f;
	  UINT dispatchX = std::ceil((float)_SURFEL_MAX_COUNT_ / groupX);

	  gfxContext.InsertUAVBarrier(m_SurfelGrid.m_GPUBuffer,true);
	  gfxContext.InsertUAVBarrier(m_SurfelList.m_GPUBuffer,true);

	  gfxContext.Dispatch(groupX, 1, 1);

  }

  void SurfelGI::FASInclusivePrefixSum(ComputeContext& gfxContext)
  {

	 static UINT kmaxDim = 65535;
	const float groupX = 256.0f;
	const float blockDim = 256.0f;
	
	//Should the dispatch count be on surlfe max count or ????
	//UINT dispatchX = std::ceil((float)_SURFEL_MAX_COUNT_ / groupX);

	UINT threadBlocks = std::ceil((float)_CELL_COUNT_ / blockDim);


	//k_maxDim = 65535 statically set to the maximum value of unsigned 16-bit integer

	//Full Block (a.k.a non-partial) can fit a full kmaxDim.
	//If the dispatch count of thread blocks is bigger than kmaxDim.
	//The dispatch call would look like (kmaxDim, fullBlockCount,1)
	//where fullBlockCount = threadBlocks / kmaxDim. (Essentially, how many kmaxDim blocks can be dispatched)

	//Partial Block - when threadBlocks < kmaxFim
	//Is Full Block is dispatchX / kmaxDim  -> 16000/65535 <  0 -> partial
	const uint32_t fullBlocks = threadBlocks / kmaxDim;
	const uint32_t partialBlocks = threadBlocks - fullBlocks * kmaxDim;


	m_PrefixSum.e_vectorizedSize = GetVectorizedSize(_CELL_COUNT_,4);
	m_PrefixSum.e_threadBlocks = threadBlocks;
	m_PrefixSum.e_isPartial = 1;
	m_PrefixSum.e_fullDispatches = 0;





	CopyPrefixInput(gfxContext, &m_SurfelGrid.m_GPUBuffer, &m_PrefixSumInput);






	  //A GPU multi-threaded prefix sum.
	  //Reduce then scan approach

	  {




		  ScopedTimer _prof(L"Surfel Fill Acceleration Structures : SURFEL PREFIX SUM : REDUCE", gfxContext);
		  gfxContext.SetPipelineState(m_AccelerationPassPrefixSumReducePSO);
		  gfxContext.SetRootSignature(m_ReduceThenScanRT);
		  gfxContext.SetDynamicConstantBufferView(0, sizeof(PrefixSum), &m_PrefixSum);
		  gfxContext.SetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,reduceThenScanPSHeap.GetHeapPointer());
		  gfxContext.SetDescriptorTable(1, reduceThenScanPSHeap[0]);

		  gfxContext.InsertUAVBarrier(m_ReductionBuffer);
	  gfxContext.InsertUAVBarrier(m_SurfelGrid.m_GPUBuffer);
	  gfxContext.InsertUAVBarrier(m_SurfelList.m_GPUBuffer,true);
		  //gfxContext.Dispatch(kmaxDim, dispatchX, 1);
		  gfxContext.Dispatch(partialBlocks, 1, 1);

	  }



	  {
		  ScopedTimer _prof(L"Surfel Fill Acceleration Structures : SURFEL PREFIX SUM : SCAN", gfxContext);
		  gfxContext.SetPipelineState(m_AccelerationPassPrefixSumScanPSO);
		  gfxContext.SetRootSignature(m_ReduceThenScanRT);
		  gfxContext.SetDynamicConstantBufferView(0, sizeof(PrefixSum), &m_PrefixSum);
		  gfxContext.SetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,reduceThenScanPSHeap.GetHeapPointer());
		  gfxContext.SetDescriptorTable(1, reduceThenScanPSHeap[0]);
	  gfxContext.InsertUAVBarrier(m_PrefixSumInput);
	  gfxContext.InsertUAVBarrier(m_ReductionBuffer);
	  gfxContext.InsertUAVBarrier(m_SurfelGrid.m_GPUBuffer);
		  gfxContext.Dispatch(1, 1, 1);
	  }


	  {
		  ScopedTimer _prof(L"Surfel Fill Acceleration Structures : SURFEL PREFIX SUM : PROPAGATE INCLUSIVE", gfxContext);
		  gfxContext.SetPipelineState(m_AccelerationPassPrefixSumInclusivePropagatePSO);
		  gfxContext.SetRootSignature(m_ReduceThenScanRT);
		  gfxContext.SetDynamicConstantBufferView(0, sizeof(PrefixSum), &m_PrefixSum);
		  gfxContext.SetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,reduceThenScanPSHeap.GetHeapPointer());
		  gfxContext.SetDescriptorTable(1, reduceThenScanPSHeap[0]);
		  gfxContext.InsertUAVBarrier(m_SurfelGrid.m_GPUBuffer);
	  gfxContext.InsertUAVBarrier(m_SurfelList.m_GPUBuffer,true);
		  gfxContext.Dispatch(partialBlocks, 1, 1);
	  }







  }

  void SurfelGI::FASSurfelInsertion(ComputeContext& gfxContext)
  {

	//Switch to the appropriate PSO
	gfxContext.SetPipelineState(m_AccelerationPassSurfelInsertionPSO);
	gfxContext.SetRootSignature(m_SurfelGenerationRT);

	//Reset surfel grid buffer
	SendParameters(gfxContext);
	  
	ScopedTimer _prof(L"Surfel Fill Acceleration Structures : SURFEL INSERTION", gfxContext);
	const float groupX = 256.0f;
	UINT dispatchX = std::ceil((float)_SURFEL_MAX_COUNT_ / groupX);
	gfxContext.InsertUAVBarrier(m_SurfelGrid.m_GPUBuffer,true);
	gfxContext.InsertUAVBarrier(m_SurfelList.m_GPUBuffer,true);
	gfxContext.Dispatch(groupX, 1, 1);

  }

  void SurfelGI::ReadbackSurfelData(GraphicsContext& gfx)
{

	//gfx.InsertUAVBarrier(m_SurfelDataReadback);
	  m_SurfelData.Read(gfx,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
//	  gfx.TransitionResource(m_SurfelData.m_GPUBuffer, D3D12_RESOURCE_STATE_COPY_SOURCE, true);
//	//gfx.InsertUAVBarrier(m_SurfelData.m_GPUBuffer);
//	gfx.CopyBuffer(m_SurfelDataReadback, m_SurfelData.m_GPUBuffer);
//
//	void* mappedData = m_SurfelDataReadback.Map();
//
//	memcpy(m_SurfelData.m_Actual.data(), mappedData, _SURFEL_MAX_COUNT_ * sizeof(SurfelData));
//	m_SurfelDataReadback.Unmap();

}


  void SurfelGI::ReadbakcSurfelDebugData(GraphicsContext& gfx)
  {

	  //CopyReadbackBuffer<SurfelDebugData>(gfx, m_SurfelDebugReadback, m_SurfelDebug, m_SurfelDebugActual);
	  CopyReadbackBuffer<SurfelDebugData>(gfx, m_SurfelDebugReadback, m_SurfelDebug, m_SurfelDebugActual);


  }

  void SurfelGI::ReadbackSurfelAccelerationStructure(GraphicsContext& gfx)
  {
	  m_SurfelList.Read(gfx, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,true);

	  //Try with UAV barrier for surfel grid even though there is already transition resource barriers
	  


	  m_SurfelGrid.Read(gfx, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,true);
  }

  void SurfelGI::ApplySurfels(ComputeContext& gfxContext,const Camera& camera)
  {

	ScopedTimer _prof(L"Surfel Application Compute Shader", gfxContext);



	//Transition resources from render target to CS 
	//NON-PIXEL SHADER RESOURCE should cover the ComputeShader stage
	gfxContext.TransitionResource(*m_GBuffer.g_Depth, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	gfxContext.TransitionResource(*m_GBuffer.g_Normal, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, true);
	gfxContext.TransitionResource(this->m_OutputTexture, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,true);

	//Switch to the appropriate PSO
	gfxContext.SetPipelineState(m_ApplicationPassPSO);
	gfxContext.SetRootSignature(m_SurfelGenerationRT);

	gfxContext.InsertUAVBarrier(m_SurfelGrid.m_GPUBuffer,true);
	gfxContext.InsertUAVBarrier(m_SurfelList.m_GPUBuffer,true);
	gfxContext.InsertUAVBarrier(m_SurfelData.m_GPUBuffer,true);
	gfxContext.InsertUAVBarrier(m_SurfelStack.m_GPUBuffer,true);


	UpdateProjection(camera);
	SendParameters(gfxContext);

	//Dispatch grid number
	const UINT TEX_SIZE_X = m_GBuffer.g_Normal->GetWidth();
	const UINT TEX_SIZE_Y = m_GBuffer.g_Normal->GetHeight();

	const UINT THREAD_GROUP_X = 32;
	const UINT THREAD_GROUP_Y = 32;
	//Mini Engine Internally uses ceilign division to supply enoug threads
	gfxContext.Dispatch2D(TEX_SIZE_X,TEX_SIZE_Y,THREAD_GROUP_X,THREAD_GROUP_Y);

  }

  void SurfelGI::RecycleSurfels(ComputeContext& gfxContext, const Camera& camera)
  {

	ScopedTimer _prof(L"Recycle Surfels Pass", gfxContext);

	//Transition resources from render target to CS 
	//NON-PIXEL SHADER RESOURCE should cover the ComputeShader stage
	gfxContext.TransitionResource(*m_GBuffer.g_Depth, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, false);
	gfxContext.TransitionResource(*m_GBuffer.g_Normal, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, true);


	gfxContext.InsertUAVBarrier(this->m_SurfelGrid.m_GPUBuffer);
	gfxContext.InsertUAVBarrier(this->m_SurfelList.m_GPUBuffer);
	gfxContext.InsertUAVBarrier(this->m_SurfelData.m_GPUBuffer);
	gfxContext.InsertUAVBarrier(this->m_SurfelData.m_GPUBuffer);

	//Switch to the appropriate PSO
	gfxContext.SetPipelineState(m_RecyclingPassPSO);
	gfxContext.SetRootSignature(m_SurfelGenerationRT);


	ID3D12DescriptorHeap* heaps[] = {
		descriptorHeap.GetHeapPointer(),  // This is your SURFEL SRV HEAP
	};

	gfxContext.GetCommandList()->SetDescriptorHeaps(1, heaps);
	  //gfx.TransitionResource(m_SurfelData.m_GPUBuffer, D3D12_RESOURCE_STATE_COPY_SOURCE, true);




	// Write to GPU buffer
	//Reset surfel grid buffer
	gfxContext.WriteBuffer(m_SurfelGrid.m_GPUBuffer, 0, m_SurfelGrid.m_Actual.data(), (_CELL_COUNT_) * sizeof(UINT));
	UpdateProjection(camera);
	SendParameters(gfxContext);

	//Bind the root parameters
	gfxContext.Dispatch(_SURFEL_MAX_COUNT_, 1, 1);

  }

  int SurfelGI::GetClosestSurfelToPosition(Vector3 worldPos)
{
	float minDistSq = (std::numeric_limits<float>::max)();
	int closestIndex = -1;

	for (int i = 2; i < m_SurfelData.m_Actual.size(); ++i)
	{
		const SurfelData& s = m_SurfelData.m_Actual[i];
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
