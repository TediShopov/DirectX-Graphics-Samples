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
	  context.WriteBuffer(m_SurfelData, 0, m_SurfelDataArray.data(), _SURFEL_MAX_COUNT_ * sizeof(SurfelData));
	  context.WriteBuffer(m_SurfelStack, 0, m_SurfelStackActual.data(), (_SURFEL_MAX_COUNT_ + 2) * sizeof(UINT));
	  context.WriteBuffer(m_SurfelGrid, 0, m_SurfelGridActual.data(), (_CELL_COUNT_) * sizeof(UINT));
	  context.Finish();
  }

  void SurfelGI::CreateHeaps()
  {
	  nonShaderVisibleHeap.Create(L"SURFEL SRV HEAP", D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 8,false);
	  ExtendedUtility::CopyDescriptorsToHeap(nonShaderVisibleHeap, {
	      m_GBuffer.g_Depth->GetDepthSRV(),
	      m_GBuffer.g_Normal->GetSRV(),
	      m_SurfelData.GetUAV(),
	      m_SurfelList.GetUAV(),
	      m_SurfelGrid.GetUAV(),
	      m_SurfelStack.GetUAV(),
	      m_OutputTexture.GetUAV(),
	      m_SurfelDebug.GetUAV()
	      }
	  );


	  descriptorHeap.Create(L"SURFEL SRV HEAP", D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 8);

	  ExtendedUtility::CopyDescriptorsToHeap(descriptorHeap, {

		  m_GBuffer.g_Depth->GetDepthSRV(),
		  m_GBuffer.g_Normal->GetSRV(),
		  m_SurfelData.GetUAV(),
		  m_SurfelList.GetUAV(),
		  m_SurfelGrid.GetUAV(),
		  m_SurfelStack.GetUAV(),
	      m_OutputTexture.GetUAV(),
	      m_SurfelDebug.GetUAV()
		  }
	  );


	  reduceThenScanPSHeap.Create(L"Reduce Then Scan  HEAP", D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 3);

	  ExtendedUtility::CopyDescriptorsToHeap(reduceThenScanPSHeap, {
		  m_PrefixSumInput.GetUAV(),
		  m_SurfelGrid.GetUAV(),
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
	  size_t sizeOfSurfel = sizeof(SurfelData);
	  m_SurfelData.Create(L"Surfel Data Buffer", _SURFEL_MAX_COUNT_, sizeof(SurfelData));
	  m_SurfelDataReadback.Create(L"Surfel Data Readback Buffer", _SURFEL_MAX_COUNT_, sizeof(SurfelData));

	  m_SurfelList.Create(L"Surfel List Buffer", _CELL_COUNT_, sizeof(UINT));
	  m_SurfelListReadback.Create(L"Surfel List Readback Buffer", _CELL_COUNT_, sizeof(UINT));

	  m_SurfelGrid.Create(L"Surfel Grid Buffer", _CELL_COUNT_, sizeof(UINT));
	  m_SurfelGridReadback.Create(L"Surfel List Readback Buffer", _CELL_COUNT_, sizeof(UINT));

	  m_ReductionBuffer.Create(L"ReduceThenScan Reduction Buffer", _CELL_COUNT_, sizeof(UINT));

	  m_PrefixSumInput.Create(L"ReduceThenScan Prefix Sum Input Copy", _CELL_COUNT_, sizeof(UINT));

	  m_PrefixSumBuffer.Create(L"ReduceThenScan Prefix Sum Buffer", 1, sizeof(PrefixSum));

	  //+1 for the stack pointer itself
	  m_SurfelStack.Create(L"Surfel Stack", _SURFEL_MAX_COUNT_ + 2, sizeof(UINT));
	  m_SurfelDebug.Create(L"Surfel Debug UAV", 1, sizeof(SurfelDebugData));
	  m_SurfelDebugReadback.Create(L"Surfel Debug Data Readback Buffer", 1, sizeof(SurfelDebugData));
	  CreateOutputTexture(&Graphics::g_SceneNormalBuffer);
  }

  void SurfelGI::RecreateSurfelListBuffers(int newNum)
  {
//	  m_surfelNum = newNum;
//	  m_SurfelGen.MaxSurfels = (UINT)m_surfelNum;
//	  m_SurfelData.Create(L"Surfel Data Buffer", newNum, sizeof(SurfelData));
//	  m_SurfelStack.Create(L"Surfel Stack", m_surfelNum + 2, sizeof(UINT));
//	  m_SurfelDataReadback.Create(L"Surfel Data Readback Buffer", newNum, sizeof(SurfelData));
//	  CreateHeaps();
//	  FillCPUContainers();


  }

  void SurfelGI::TransitionResourcesTo()
  {
  }

  void SurfelGI::CreateRootSig()
  {
	  SamplerDesc DefaultSamplerDesc;
	  DefaultSamplerDesc.MaxAnisotropy = 8;
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
	  m_SurfelGen.kPerCellSurfelLimit = 50;

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
	m_SurfelDataArray.clear();
	m_SurfelStackActual.clear();
	m_SurfelGridActual.clear();
	m_SurfelListActual.clear();

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
		m_SurfelDataArray.push_back(data);
	}
	// Fill data
	for (int i = 0; i < _SURFEL_MAX_COUNT_; ++i) {
		m_SurfelDataArray[i].position =
			Math::Vector4(float(i) * m_SurfelGen.UniformGrid.cellSize.GetX(), 0.0f, 0.0f, 1.0f);
		m_SurfelDataArray[i].radius = Math::Vector4(0,0,0,0);
		m_SurfelDataArray[i].normal = Math::Vector4(0.0f, 1.0f, 0.0f, 1.0f);
		m_SurfelDataArray[i].pixelPosX = 0;
		m_SurfelDataArray[i].pixelPosY = 0;
		m_SurfelDataArray[i].tilePosX = 0;
		m_SurfelDataArray[i].tilePosY = 0;
		m_SurfelDataArray[i].raySamples = 1;
		m_SurfelDataArray[i].contribution0 = 0;
		m_SurfelDataArray[i].contribution1= 0;
		m_SurfelDataArray[i].contribution3 = 0;
		m_SurfelDataArray[i].contribution2 = 0;
		m_SurfelDataArray[i].meanOne = Math::Vector4(0,0,0,0);
		m_SurfelDataArray[i].mean = Math::Vector4(0, 0, 0, 0);
	}

	//Fill the surfle stack buffer
	//The first index is the surfel stack pointer and should be set to 0
	//E.g pointer set to 0 would actually read memory adress at 1.
	for (int i = 0; i < _CELL_COUNT_; ++i) {
		m_SurfelGridActual.push_back(0);
		m_SurfelListActual.push_back(0);
	}


	//Fill the surfle stack buffer
	//The first index is the surfel stack pointer and should be set to 0
	//E.g pointer set to 0 would actually read memory adress at 1.
	for (int i = 0; i < _SURFEL_MAX_COUNT_ + 2; ++i) {
		m_SurfelStackActual.push_back(i-2);
	}
	m_SurfelStackActual[0] = 0 + 2;
	m_SurfelStackActual[1] = 0;
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
	
	gfxContext.WriteBuffer(m_SurfelGrid, 0, m_SurfelGridActual.data(), (_CELL_COUNT_) * sizeof(UINT));
//	const float groupX = 256.0f;
//	UINT dispatchX = std::ceil((float)_SURFEL_MAX_COUNT_ / groupX);
	gfxContext.InsertUAVBarrier(m_SurfelGrid);
	gfxContext.InsertUAVBarrier(m_SurfelList);

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

	  gfxContext.InsertUAVBarrier(m_SurfelGrid);
	  gfxContext.WriteBuffer(m_SurfelGrid, 0, m_SurfelGridActual.data(), (_CELL_COUNT_) * sizeof(UINT));

	  //Switch to the appropriate PSO
	  gfxContext.SetPipelineState(m_AccelerationPassSurfelCountPSO);
	  gfxContext.SetRootSignature(m_SurfelGenerationRT);

	  //Reset surfel grid buffer
	  SendParameters(gfxContext);


	  ScopedTimer _prof(L"Surfel Fill Acceleration Structures : SURFEL COUNT", gfxContext);
	  const float groupX = 256.0f;
	  UINT dispatchX = std::ceil((float)_SURFEL_MAX_COUNT_ / groupX);

	  gfxContext.InsertUAVBarrier(m_SurfelGrid);
	  gfxContext.InsertUAVBarrier(m_SurfelList);

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





	CopyPrefixInput(gfxContext, &m_SurfelGrid, &m_PrefixSumInput);






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
	  gfxContext.InsertUAVBarrier(m_SurfelGrid);
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
	  gfxContext.InsertUAVBarrier(m_SurfelGrid);
		  gfxContext.Dispatch(1, 1, 1);
	  }


	  {
		  ScopedTimer _prof(L"Surfel Fill Acceleration Structures : SURFEL PREFIX SUM : PROPAGATE INCLUSIVE", gfxContext);
		  gfxContext.SetPipelineState(m_AccelerationPassPrefixSumInclusivePropagatePSO);
		  gfxContext.SetRootSignature(m_ReduceThenScanRT);
		  gfxContext.SetDynamicConstantBufferView(0, sizeof(PrefixSum), &m_PrefixSum);
		  gfxContext.SetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,reduceThenScanPSHeap.GetHeapPointer());
		  gfxContext.SetDescriptorTable(1, reduceThenScanPSHeap[0]);
		  gfxContext.InsertUAVBarrier(m_SurfelGrid);
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
	gfxContext.InsertUAVBarrier(m_SurfelGrid);
	gfxContext.InsertUAVBarrier(m_SurfelList);
	gfxContext.Dispatch(groupX, 1, 1);

  }

  void SurfelGI::ReadbackSurfelData(GraphicsContext& gfx)
{

	gfx.CopyBuffer(m_SurfelDataReadback, m_SurfelData);

	void* mappedData = m_SurfelDataReadback.Map();
	memcpy(m_SurfelDataArray.data(), mappedData, _SURFEL_MAX_COUNT_ * sizeof(SurfelData));
	m_SurfelDataReadback.Unmap();

}


  void SurfelGI::ReadbakcSurfelDebugData(GraphicsContext& gfx)
  {

	  //CopyReadbackBuffer<SurfelDebugData>(gfx, m_SurfelDebugReadback, m_SurfelDebug, m_SurfelDebugActual);
	  CopyReadbackBuffer<SurfelDebugData>(gfx, m_SurfelDebugReadback, m_SurfelDebug, m_SurfelDebugActual);


  }

  void SurfelGI::ReadbackSurfelAccelerationStructure(GraphicsContext& gfx)
  {
	  gfx.TransitionResource(m_SurfelGrid, D3D12_RESOURCE_STATE_COPY_SOURCE);
	  gfx.TransitionResource(m_SurfelList, D3D12_RESOURCE_STATE_COPY_SOURCE);

	  gfx.InsertUAVBarrier(m_SurfelGrid);
	  gfx.InsertUAVBarrier(m_SurfelList);

	  CopyReadbackBufferMany<UINT>(gfx, m_SurfelListReadback, m_SurfelList, m_SurfelListActual, _CELL_COUNT_);
	  CopyReadbackBufferMany<UINT>(gfx, m_SurfelGridReadback, m_SurfelGrid, m_SurfelGridActual, _CELL_COUNT_);
	  for (size_t i = 0; i < m_SurfelGridActual.size(); i++)
	  {
		  if(m_SurfelGridActual[i] != 0)
		  {
			  int a = 3;
		  }

	  }

	  gfx.TransitionResource(m_SurfelGrid, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	  gfx.TransitionResource(m_SurfelList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
  }

  void SurfelGI::ApplySurfels(ComputeContext& gfxContext,const Camera& camera)
  {

	ScopedTimer _prof(L"Surfel Application Compute Shader", gfxContext);

	//Transition resources from render target to CS 
	//NON-PIXEL SHADER RESOURCE should cover the ComputeShader stage
	gfxContext.TransitionResource(*m_GBuffer.g_Depth, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, true);
	gfxContext.TransitionResource(*m_GBuffer.g_Normal, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, true);

	//Switch to the appropriate PSO
	gfxContext.SetPipelineState(m_ApplicationPassPSO);
	gfxContext.SetRootSignature(m_SurfelGenerationRT);



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

	//Switch to the appropriate PSO
	gfxContext.SetPipelineState(m_RecyclingPassPSO);
	gfxContext.SetRootSignature(m_SurfelGenerationRT);


	ID3D12DescriptorHeap* heaps[] = {
		descriptorHeap.GetHeapPointer(),  // This is your SURFEL SRV HEAP
	};

	gfxContext.GetCommandList()->SetDescriptorHeaps(1, heaps);




	// Write to GPU buffer
	//Reset surfel grid buffer
	gfxContext.WriteBuffer(m_SurfelGrid, 0, m_SurfelGridActual.data(), (_CELL_COUNT_) * sizeof(UINT));
	UpdateProjection(camera);
	SendParameters(gfxContext);

	//Bind the root parameters
	gfxContext.Dispatch(_SURFEL_MAX_COUNT_, 1, 1);

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
