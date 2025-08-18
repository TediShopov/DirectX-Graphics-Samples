//
// Copyright (c) Microsoft. All rights reserved.
// This code is licensed under the MIT License (MIT).
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
// Developed by Minigraph
//
// Author:  James Stanard 
//

#define NV_PERF_ENABLE_INSTRUMENTATION 
#include "pch.h"
#include "SystemTime.h"
#include "Display.h"
#include "TextRenderer.h"
#include "GraphRenderer.h"
#include "GameInput.h"
#include "GpuTimeManager.h" 
#include "CommandContext.h"
#include <vector>
#include <unordered_map>
#include <array>

//#include "nvperf_host.h"
//#include "nvperf_d3d12_host.h"
#include <NvPerfReportGeneratorD3D12.h>
#ifdef NV_PERF_ENABLE_INSTRUMENTATION
#include "nvperf_host_impl.h"
#endif // !NV_PERF_ENABLE_INSTRUMENTATION

#include <NvPerfUtilities.h>
#include "NvPerfPeriodicSamplerD3D12.h"
#include "NvPerfHudDataModel.h"

#define RYML_SINGLE_HDR_DEFINE_NOW 
#include "ryml_all.hpp"

#include <locale>
#include <codecvt>

#include <wincodec.h>
#include "ScreenGrab12.h"

using namespace Graphics;
using namespace GraphRenderer;
using namespace Math;
using namespace std;

#define PERF_GRAPH_ERROR uint32_t(0xFFFFFFFF)

namespace EngineProfiling
{
    bool Paused = false;
}

class StatHistory
{
public:
    StatHistory()
    {
        for (uint32_t i = 0; i < kHistorySize; ++i)
            m_RecentHistory[i] = 0.0f;
        for (uint32_t i = 0; i < kExtendedHistorySize; ++i)
            m_ExtendedHistory[i] = 0.0f;
        m_Average = 0.0f;
        m_Minimum = 0.0f;
        m_Maximum = 0.0f;
    }

    void RecordStat( uint32_t FrameIndex, float Value )
    {
        m_RecentHistory[FrameIndex % kHistorySize] = Value;
        m_ExtendedHistory[FrameIndex % kExtendedHistorySize] = Value;
        m_Recent = Value;

        uint32_t ValidCount = 0;
        m_Minimum = FLT_MAX;
        m_Maximum = 0.0f;
        m_Average = 0.0f;

        for (float val : m_RecentHistory)
        {
            if (val > 0.0f)
            {
                ++ValidCount;
                m_Average += val;
                m_Minimum = min(val, m_Minimum);
                m_Maximum = max(val, m_Maximum);
            }
        }

        if (ValidCount > 0)
            m_Average /= (float)ValidCount;
        else
            m_Minimum = 0.0f;
    }

    float GetLast(void) const { return m_Recent; }
    float GetMax(void) const { return m_Maximum; }
    float GetMin(void) const { return m_Minimum; }
    float GetAvg(void) const { return m_Average; }

    const float* GetHistory(void) const { return m_ExtendedHistory; }
    uint32_t GetHistoryLength(void) const { return kExtendedHistorySize; }

private:
    static const uint32_t kHistorySize = 64;
    static const uint32_t kExtendedHistorySize = 256;
    float m_RecentHistory[kHistorySize];
    float m_ExtendedHistory[kExtendedHistorySize];
    float m_Recent;
    float m_Average;
    float m_Minimum;
    float m_Maximum;
};

class StatPlot
{
public:
    StatPlot(StatHistory& Data, Color Col = Color(1.0f, 1.0f, 1.0f))
        : m_StatData(Data), m_PlotColor(Col)
    {
    }

    void SetColor( Color Col )
    {
        m_PlotColor = Col;
    }

private:
    StatHistory& m_StatData;
    Color m_PlotColor;
};

class StatGraph
{
public:
    StatGraph(const wstring& Label, D3D12_RECT Window)
        : m_Label(Label), m_Window(Window), m_BGColor(0.0f, 0.0f, 0.0f, 0.2f)
    {
    }

    void SetLabel(const wstring& Label)
    {
        m_Label = Label;
    }

    void SetWindow(D3D12_RECT Window)
    {
        m_Window = Window;
    }

    uint32_t AddPlot( const StatPlot& P )
    {
        uint32_t Idx = (uint32_t)m_Stats.size();
        m_Stats.push_back(P);
        return Idx;
    }

    StatPlot& GetPlot( uint32_t Handle );

    void Draw( GraphicsContext& Context );

private:
    wstring m_Label;
    D3D12_RECT m_Window;
    vector<StatPlot> m_Stats;
    Color m_BGColor;
    float m_PeakValue;
};

class GraphManager
{
public:

private:
    vector<StatGraph> m_Graphs;
};

class GpuTimer
{
public:

    GpuTimer::GpuTimer()
    {
        m_TimerIndex = GpuTimeManager::NewTimer();
    }

    void Start(CommandContext& Context)
    {
        GpuTimeManager::StartTimer(Context, m_TimerIndex);
    }

    void Stop(CommandContext& Context)
    {
        GpuTimeManager::StopTimer(Context, m_TimerIndex);
    }

    float GpuTimer::GetTime(void)
    {
        return GpuTimeManager::GetTime(m_TimerIndex);
    }

    uint32_t GetTimerIndex(void)
    {
        return m_TimerIndex;
    }
private:

    uint32_t m_TimerIndex;
};

class NestedTimingTree
{
public:
    NestedTimingTree( const wstring& name, NestedTimingTree* parent = nullptr )
        : m_Name(name), m_Parent(parent), m_IsExpanded(false), m_IsGraphed(false), m_GraphHandle(PERF_GRAPH_ERROR) {}

    NestedTimingTree* GetChild( const wstring& name )
    {
        auto iter = m_LUT.find(name);
        if (iter != m_LUT.end())
            return iter->second;

        NestedTimingTree* node = new NestedTimingTree(name, this);
        m_Children.push_back(node);
        m_LUT[name] = node;
        return node;
    }

    NestedTimingTree* NextScope( void )
    {
        if (m_IsExpanded && m_Children.size() > 0)
            return m_Children[0];

        return m_Parent->NextChild(this);
    }

    NestedTimingTree* PrevScope( void )
    {
        NestedTimingTree* prev = m_Parent->PrevChild(this);
        return prev == m_Parent ? prev : prev->LastChild();
    }

    NestedTimingTree* FirstChild( void )
    {
        return m_Children.size() == 0 ? nullptr : m_Children[0];
    }

    NestedTimingTree* LastChild( void )
    {
        if (!m_IsExpanded || m_Children.size() == 0)
            return this;

        return m_Children.back()->LastChild();
    }

    NestedTimingTree* NextChild( NestedTimingTree* curChild )
    {
        ASSERT(curChild->m_Parent == this);

        for (auto iter = m_Children.begin(); iter != m_Children.end(); ++iter)
        {
            if (*iter == curChild)
            {
                auto nextChild = iter; ++nextChild;
                if (nextChild != m_Children.end())
                    return *nextChild;
            }
        }

        if (m_Parent != nullptr)
            return m_Parent->NextChild(this);
        else
            return &sm_RootScope;
    }

    NestedTimingTree* PrevChild( NestedTimingTree* curChild )
    {
        ASSERT(curChild->m_Parent == this);

        if (*m_Children.begin() == curChild)
        {
            if (this == &sm_RootScope)
                return sm_RootScope.LastChild();
            else
                return this;
        }

        for (auto iter = m_Children.begin(); iter != m_Children.end(); ++iter)
        {
            if (*iter == curChild)
            {
                auto prevChild = iter; --prevChild;
                return *prevChild;
            }
        }

        ERROR("All attempts to find a previous timing sample failed");
        return nullptr;
    }

    void StartTiming( CommandContext* Context )
    {
        m_StartTick = SystemTime::GetCurrentTick();
        if (Context == nullptr)
            return;

        m_GpuTimer.Start(*Context);

        Context->PIXBeginEvent(m_Name.c_str());
    }

    void StopTiming( CommandContext* Context )
    {
        m_EndTick = SystemTime::GetCurrentTick();
        if (Context == nullptr)
            return;

        m_GpuTimer.Stop(*Context);

        Context->PIXEndEvent();
    }

    void GatherTimes(uint32_t FrameIndex)
    {
        if (sm_SelectedScope == this)
        {
            GraphRenderer::SetSelectedIndex(m_GpuTimer.GetTimerIndex());
        }
        if (EngineProfiling::Paused)
        {
            for (auto node : m_Children)
                node->GatherTimes(FrameIndex);
            return;
        }
        m_CpuTime.RecordStat(FrameIndex, 1000.0f * (float)SystemTime::TimeBetweenTicks(m_StartTick, m_EndTick));
        m_GpuTime.RecordStat(FrameIndex, 1000.0f * m_GpuTimer.GetTime());

        for (auto node : m_Children)
            node->GatherTimes(FrameIndex);

        m_PrevStartTick = m_StartTick;
        m_PrevEndTick = m_EndTick;


        m_StartTick = 0;
        m_EndTick = 0;
    }

    void SumInclusiveTimes(float& cpuTime, float& gpuTime)
    {
        cpuTime = 0.0f;
        gpuTime = 0.0f;
        for (auto iter = m_Children.begin(); iter != m_Children.end(); ++iter)
        {
            cpuTime += (*iter)->m_CpuTime.GetLast();
            gpuTime += (*iter)->m_GpuTime.GetLast();
        }
    }

    static void PushProfilingMarker( const wstring& name, CommandContext* Context );
    static void PopProfilingMarker( CommandContext* Context );
    static void Update( void );
    static void UpdateTimes( void )
    {
        uint32_t FrameIndex = (uint32_t)Graphics::GetFrameCount();

        GpuTimeManager::BeginReadBack();
        sm_RootScope.GatherTimes(FrameIndex);
        s_FrameDelta.RecordStat(FrameIndex, GpuTimeManager::GetTime(0));
        GpuTimeManager::EndReadBack();

        float TotalCpuTime, TotalGpuTime;
        sm_RootScope.SumInclusiveTimes(TotalCpuTime, TotalGpuTime);
        s_TotalCpuTime.RecordStat(FrameIndex, TotalCpuTime);
        s_TotalGpuTime.RecordStat(FrameIndex, TotalGpuTime);

        GraphRenderer::Update(XMFLOAT2(TotalCpuTime, TotalGpuTime), 0, GraphType::Global);
    }

    static float GetTotalCpuTime(void) { return s_TotalCpuTime.GetAvg(); }
    static float GetTotalGpuTime(void) { return s_TotalGpuTime.GetAvg(); }
    static float GetFrameDelta(void) { return s_FrameDelta.GetAvg(); }

    static void Display( TextContext& Text, float x )
    {
        float curX = Text.GetCursorX();
        Text.DrawString("  ");
        float indent = Text.GetCursorX() - curX;
        Text.SetCursorX(curX);
        sm_RootScope.DisplayNode( Text, x - indent, indent );
        sm_RootScope.StoreToGraph();
    }

    void Toggle()
    { 
        //if (m_GraphHandle == PERF_GRAPH_ERROR)
        //    m_GraphHandle = GraphRenderer::InitGraph(GraphType::Profile);
        //m_IsGraphed = GraphRenderer::ManageGraphs(m_GraphHandle, GraphType::Profile);
    }
    bool IsGraphed(){ return m_IsGraphed;}

    StatHistory m_CpuTime;
    StatHistory m_GpuTime;
    static StatHistory s_TotalCpuTime;
    static StatHistory s_TotalGpuTime;
    static StatHistory s_FrameDelta;
    static NestedTimingTree sm_RootScope;

    // In NestedTimingTree (public):
	 const std::wstring& GetName() const  { return m_Name; }
	 float GetGpuHistoryValueAt(uint32_t frameIndex) const { return m_GpuTime.GetLast(); }
	 float GetCpuHistoryValueAt(uint32_t frameIndex) const { return m_CpuTime.GetLast(); }
	 float GetLastStartTick() const { return m_PrevStartTick; }
	 float GetLastEndTick() const { return m_PrevEndTick; }
	 const std::vector<NestedTimingTree*>& GetChildren() const  { return m_Children; }
private:

    void DisplayNode( TextContext& Text, float x, float indent );
    void StoreToGraph(void);
    void DeleteChildren( void )
    {
        for (auto node : m_Children)
            delete node;
        m_Children.clear();
    }

    wstring m_Name;
    NestedTimingTree* m_Parent;
    vector<NestedTimingTree*> m_Children;
    unordered_map<wstring, NestedTimingTree*> m_LUT;
    int64_t m_StartTick;
    int64_t m_EndTick;

    //Updated after gather times
    int64_t m_PrevStartTick;
    int64_t m_PrevEndTick;

    bool m_IsExpanded;
    GpuTimer m_GpuTimer;
    bool m_IsGraphed;
    GraphHandle m_GraphHandle;
    static NestedTimingTree* sm_CurrentNode;
    static NestedTimingTree* sm_SelectedScope;

    static bool sm_CursorOnGraph;



};

StatHistory NestedTimingTree::s_TotalCpuTime;
StatHistory NestedTimingTree::s_TotalGpuTime;
StatHistory NestedTimingTree::s_FrameDelta;
NestedTimingTree NestedTimingTree::sm_RootScope(L"");
NestedTimingTree* NestedTimingTree::sm_CurrentNode = &NestedTimingTree::sm_RootScope;
NestedTimingTree* NestedTimingTree::sm_SelectedScope = &NestedTimingTree::sm_RootScope;
bool NestedTimingTree::sm_CursorOnGraph = false;

namespace EngineProfiling
{
    bool captureFrame =false;
	using namespace nv::perf;
	profiler::ReportGeneratorD3D12 m_nvperf;

	sampler::PeriodicSamplerTimeHistoryD3D12 m_sampler;
	hud::HudDataModel m_hudDataModel;
	MetricsEvaluator m_metricEvaluator;
	CounterConfiguration m_counterConfiguration;
	uint64_t frameIndex;

	uint32_t frameStageStampsCollected;
	std::vector<StageStamp> stageStamps;

	using convert_type = std::codecvt_utf8<wchar_t>;
	std::wstring_convert<convert_type, wchar_t> converter;
    

	std::vector<NVPW_MetricEvalRequest> m_metricEvalRequests; // This is used in both scheduling and subsequently evaluating the values.
	//	hud::HudImPlotRenderer m_hudRenderer;

	BoolVar DrawFrameRate("Display Frame Rate", true);
	BoolVar DrawProfiler("Display Profiler", false);
	//BoolVar DrawPerfGraph("Display Performance Graph", false);
	const bool DrawPerfGraph = false;
	const char* Metrics[] = {
		"gpc__cycles_elapsed.avg.per_second",
		"sys__cycles_elapsed.avg.per_second",
		"lts__cycles_elapsed.avg.per_second",
	};


	void ThrowIfFalse(bool result, const char* pMessage)
	{
		if (!result)
		{
			NV_PERF_LOG_ERR(10, "%s\n", pMessage);
			throw std::runtime_error(pMessage);
		}
	}

    bool SetCaptureFrame(bool a ) {

        captureFrame = a;
        return a;

    }
    bool GetCaptureFrame() {
        return captureFrame;

    }
    bool CaptureRenderTarget(ID3D12Resource* tex) {

        HRESULT res = SaveWICTextureToFile(
            Graphics::g_CommandManager.GetGraphicsQueue().GetCommandQueue(),
            tex,
            GUID_ContainerFormatPng,
            L"screenshot.png",
            D3D12_RESOURCE_STATE_RENDER_TARGET,
            D3D12_RESOURCE_STATE_RENDER_TARGET);
        captureFrame = false;
        return true;

    }
	void Initialize()
	{
		//m_nvperf.additionalMetrics = { "crop__write_throughput" };
		// Initialize
		m_nvperf.InitializeReportGenerator(g_Device);
		auto initStatus = m_nvperf.GetInitStatus();

		// Optional: set frame range name (so reports have a “Frame” top-level marker)
		m_nvperf.SetFrameLevelRangeName("Frame");

		// Depth of nested PushRange calls you might use
		//m_nvperf.SetNumNestingLevels(10); // heuristic default
		m_nvperf.SetNumNestingLevels(10); // heuristic default
		m_nvperf.SetMaxNumRanges(100); // heuristic default
		//m_nvperf.outputOptions.directoryName = "D3D12Test";



		//printf("NvPerf requires %u configuration passes\n", numPasses);
		//SetLogVolumeLevel(LogSeverity::Inf,100);
		// --- INITIALIZE THE PERIODIC SAMPLER ---
		m_sampler.Initialize(Graphics::g_Device);
		std::vector<uint8_t> metricsEvalScratchBuffer;
		const DeviceIdentifiers devId = m_sampler.GetGpuDeviceIdentifiers();
		NVPW_MetricsEvaluator* pMetricsEvaluator = sampler::DeviceCreateMetricsEvaluator(metricsEvalScratchBuffer, devId.pChipName);
		m_metricEvaluator = MetricsEvaluator(pMetricsEvaluator, std::move(metricsEvalScratchBuffer)); // transfer ownership to m_metricsEvaluator
		//m_metricEvaluator = MetricsEvaluator()

	// Create the config builder, this is used to create a counter configuration
		MetricsConfigBuilder configBuilder;
		{
			NVPW_RawCounterConfig* pRawCounterConfig = sampler::DeviceCreateRawCounterConfig(devId.pChipName);
			ThrowIfFalse(pRawCounterConfig, "Failed to create the raw counter config.");
			ThrowIfFalse(configBuilder.Initialize(m_metricEvaluator, pRawCounterConfig, devId.pChipName), "Failed to initialize the config builder."); // transfer pRawCounterConfig's ownership to configBuilder
		}

		// Add metrics into config builder
		for (size_t ii = 0; ii < sizeof(Metrics) / sizeof(Metrics[0]); ++ii)
		{
			const char* const pMetric = Metrics[ii];
			NVPW_MetricEvalRequest request{};
			ThrowIfFalse(ToMetricEvalRequest(m_metricEvaluator, pMetric, request), "Failed to convert the metric to its NVPW_MetricEvalRequest.");
			// By setting "keepInstances" to false, the counter data will only store GPU-level values, reducing its size and improving the performance of metric evaluation.
			// However, this option has the drawback of making max/min submetrics non-evaluable.
			const bool keepInstances = false;
			ThrowIfFalse(configBuilder.AddMetrics(&request, 1, keepInstances), "Failed to add the metric into the config build.");
			m_metricEvalRequests.emplace_back(std::move(request));
		}

		// Create the counter configuration out of the config builder.
		ThrowIfFalse(CreateConfiguration(configBuilder, m_counterConfiguration), "Failed CreateConfiguration().");
		// Periodic sampler supports only single-pass configurations, meaning that all scheduled metrics must be collectable in a single pass.
		ThrowIfFalse(m_counterConfiguration.numPasses == 1u, "The scheduled config is not a single-pass configuration, so it is not compatible with the periodic sampler.");






		//use converter (.to_bytes: wstr->str, .from_bytes: str->wstr)

		std::wstring string_to_convert = L"Assam";
		std::string converted_str = converter.to_bytes(string_to_convert);



	}
	bool BeginSamplerSession()
	{
		//start a session
		uint32_t samplingFrequencyInHz = 60;
		uint32_t samplingIntervalInNs = 1000000000 / samplingFrequencyInHz;
		uint32_t maxDecodeLatencyInNs = 1000000000;
		uint32_t maxFrameLatency = 10;
        frameIndex = 0;


		m_sampler.BeginSession(
			Graphics::g_CommandManager.GetGraphicsQueue().GetCommandQueue(),
			samplingIntervalInNs,
			maxDecodeLatencyInNs,
			maxFrameLatency);

		hud::HudPresets hudPresets;
		auto deviceIdentifiers = m_sampler.GetGpuDeviceIdentifiers();
		hudPresets.Initialize(deviceIdentifiers.pChipName);
		//m_hudDataModel.Load(hudPresets.GetPreset("Graphics General Triage"));
		m_hudDataModel.Load(hudPresets.GetPreset("Graphics High Speed Triage"));

		//auto conifigs = m_hudDataModel.GetConfigurations();


		m_sampler.SetConfig(&m_counterConfiguration);

		ThrowIfFalse(MetricsEvaluatorSetDeviceAttributes(m_metricEvaluator, m_sampler.GetCounterData().data(), m_sampler.GetCounterData().size()), "Failed MetricsEvaluatorSetDeviceAttributes().");

		double plotTimeWidthInSeconds = 4.0;
		//m_hudDataModel.Initialize(1.0 / samplingFrequencyInHz,
			//plotTimeWidthInSeconds);

		//--- Attemp to create custom counter configuratoin 
//        MetricsConfigBuilder configBuilder;
//        configBuilder.AddMetric("");
//
//        CounterConfiguration customCounterConfiguration;
//        customCounterConfiguration.

//		m_sampler.SetConfig(&m_hudDataModel.GetCounterConfiguration());
//		m_hudDataModel.PrepareSampleProcessing(m_sampler.GetCounterData());

		return true;


	}
	bool OutputMetricsHeader(std::ostream& OutStream)
	{
        OutStream
            << "Frame" << ", "
            << "Frame Start" << ", "
            << "Frame End" << ", "
            << "Frame Duration" << ", ";

        for (size_t ii = 0; ii < sizeof(Metrics) / sizeof(Metrics[0]); ++ii)
        {
            OutStream << Metrics[ii] << ", ";
        }
        OutStream << "\n";
        return true;
	}

// Depth-first traversal skipping the root’s empty label
static void CollectPerStageStamps(uint32_t frameIndex,
                                  const NestedTimingTree* node,
                                  std::vector<StageStamp>& out)
{
	if (!node) return;

	if (!node->GetName().empty()) {
		StageStamp s;

		std::wstring string_to_convert = node->GetName();
		std::string converted_str = converter.to_bytes(string_to_convert);
		s.name = converted_str;
		s.gpuMs = node->GetGpuHistoryValueAt(frameIndex);
		s.cpuMs = node->GetCpuHistoryValueAt(frameIndex);
		s.tickStart = node->GetLastStartTick();
		s.tickEnd = node->GetLastEndTick();
		out.emplace_back(std::move(s));
	}
	for (auto* c : node->GetChildren())
		CollectPerStageStamps(frameIndex, c, out);
}
	bool ConsumeSampler(std::ostream& OutStream)
	{

        frameIndex++;

		std::vector<double> metricValues(m_metricEvalRequests.size());
		m_sampler.DecodeCounters();
		auto frameDelims = m_sampler.GetFrameDelimiters();
		m_sampler.ConsumeSamples([&](const uint8_t* pCounterDataImage,
			size_t counterDataImageSize, uint32_t rangeIndex, bool& stop) {
				stop = false;
				sampler::SampleTimestamp timestamp{};
				if (!CounterDataGetSampleTime(pCounterDataImage, rangeIndex, timestamp))
				{
					return false;
				}


				if (!EvaluateToGpuValues(
					m_metricEvaluator,
					pCounterDataImage,
					counterDataImageSize,
					rangeIndex,
					m_metricEvalRequests.size(),
					m_metricEvalRequests.data(),
					metricValues.data()))
				{
					return false;
				}
				uint64_t frameOffset = 0;
				// Scan delimiters in order and assign index
				for (size_t i = 0; i < frameDelims.size(); ++i)
				{
					if (timestamp.end < frameDelims[i].frameEndTime)
					{
						frameOffset = i; // this is your frame number
						break;
					}
				}

				//OutStream << frameIndex << ", " << pPerfMarker << ", ";
				//OutStream << "Frame: "<< frameIndex << ", " << "Perf Marker would go here" << ", ";
				OutStream << frameIndex+frameOffset << ", ";
				OutStream << std::fixed << std::setprecision(0) << timestamp.start << ", " << timestamp.end << ", " << (timestamp.end - timestamp.start);
				for (double metricValue : metricValues)
				{
					OutStream << ", " << metricValue;
				}
				OutStream << "\n";
				//Some condition on when to stop iterating over markers
				// Maybe if no markers are present we could stop immeidately ...
//                if(true)
//                    stop = true;


				// THIS USES CPU MARKER COUNTER TO ASSOCIATE PASSES
//				                {
//				                    const size_t frameIndex = *markers.pUserData;
//				                    const char* const pPerfMarker = markers.pBegin[markerIdx].pName;
//				                    OutStream << frameIndex << ", " << pPerfMarker << ", ";
//				                    OutStream << std::fixed << std::setprecision(0) << timestamp.start << ", " << timestamp.end << ", " << (timestamp.end - timestamp.start);
//				                    for (double metricValue : metricValues)
//				                    {
//				                        OutStream << ", " << metricValue;
//				                    }
//				                    OutStream << "\n";
//				                }
//				                if (++markerIdx == markers.validMarkerCount)
//				                {
//				                    stop = true; // Inform counter data to stop iterating because all this frame's data has been consumed.
//				                }
				return true;

				return m_hudDataModel.AddSample(pCounterDataImage,
					counterDataImageSize, rangeIndex);
			}
		);
		//		for (auto& frameDelimiter : m_sampler.GetFrameDelimiters())
		//		{
		//			m_hudDataModel.AddFrameDelimiter(frameDelimiter.frameEndTime);
		//		}
		//        //Indent for a .csv file
		//        m_hudDataModel.Print(outStream, ",");
		return true;





	}




	void TestGpuDial()
	{

	}
	void Update(void)
	{
		if (GameInput::IsFirstPressed(GameInput::kStartButton)
			|| GameInput::IsFirstPressed(GameInput::kKey_space))
		{
			Paused = !Paused;
		}
		NestedTimingTree::UpdateTimes();
		{
			frameStageStampsCollected = (uint32_t)Graphics::GetFrameCount();
            stageStamps.clear();
			CollectPerStageStamps(frameStageStampsCollected, &NestedTimingTree::sm_RootScope, stageStamps);

		}

	}

    uint32_t GetFrameStampCollected() 
    {
        return frameStageStampsCollected;
    }
    std::vector<StageStamp> GetStageStamps() {
        return stageStamps;

    }

	void BeginBlock(const wstring& name, CommandContext* Context)
	{
		NestedTimingTree::PushProfilingMarker(name, Context);
		std::wstring string_to_convert;

		//OnFrameStart();
		//setup converter
		using convert_type = std::codecvt_utf8<wchar_t>;
		std::wstring_convert<convert_type, wchar_t> converter;
		//use converter (.to_bytes: wstr->str, .from_bytes: str->wstr)
		std::string converted_str = converter.to_bytes(string_to_convert);


		if (Context != nullptr)
		{
			m_nvperf.rangeCommands.PushRange(Context->GetCommandList(), converted_str.c_str());
		}
	}

	void EndBlock(CommandContext* Context)
	{
		NestedTimingTree::PopProfilingMarker(Context);
		if (Context != nullptr)
			m_nvperf.rangeCommands.PopRange(Context->GetCommandList());

	}

	bool IsPaused()
	{
		return Paused;
	}

	void DisplayFrameRate(TextContext& Text)
	{
		if (!DrawFrameRate)
			return;

		float cpuTime = NestedTimingTree::GetTotalCpuTime();
		float gpuTime = NestedTimingTree::GetTotalGpuTime();
		float frameRate = 1.0f / NestedTimingTree::GetFrameDelta();

		Text.DrawFormattedString("CPU %7.3f ms, GPU %7.3f ms, %3u Hz\n",
			cpuTime, gpuTime, (uint32_t)(frameRate + 0.5f));
	}

	void DisplayPerfGraph(GraphicsContext& Context)
	{
		if (DrawPerfGraph)
			GraphRenderer::RenderGraphs(Context, GraphType::Global);
	}

	void Display(TextContext& Text, float x, float y, float /*w*/, float /*h*/)
	{
		Text.ResetCursor(x, y);

		if (DrawProfiler)
		{
			//Text.GetCommandContext().SetScissor((uint32_t)Floor(x), (uint32_t)Floor(y), (uint32_t)Ceiling(w), (uint32_t)Ceiling(h));

			NestedTimingTree::Update();

			Text.SetColor(Color(0.5f, 1.0f, 1.0f));
			Text.DrawString("Engine Profiling");
			Text.SetColor(Color(0.8f, 0.8f, 0.8f));
			Text.SetTextSize(20.0f);
			Text.DrawString("           CPU    GPU");
			Text.SetTextSize(24.0f);
			Text.NewLine();
			Text.SetTextSize(20.0f);
			Text.SetColor(Color(1.0f, 1.0f, 1.0f));

			NestedTimingTree::Display(Text, x);
		}

		Text.GetCommandContext().SetScissor(0, 0, g_DisplayWidth, g_DisplayHeight);
	}
	void OnFrameEnd()
	{
		//m_nvperf.OnFrameStart

		bool res = m_nvperf.OnFrameEnd();
		m_sampler.OnFrameEnd();
		//m_nvperf.Reset();
		//m_nvperf.StartCollectionOnNextFrame();

	}
	void BeginSession()
	{
		m_nvperf.BeginSession(Graphics::g_CommandManager.GetGraphicsQueue().GetCommandQueue());
	}
	void EndSessoin()
	{
		m_nvperf.Reset();
	}
	void CollectReport()
	{
		bool a = m_nvperf.StartCollectionOnNextFrame();
	}
	//    void BeginBlockPerfSDK(const std::wstring& name, CommandContext* Context)
	//    {
	//        if (Context != nullptr)
	//        {
	//			m_nvperf.rangeCommands.PushRange(Context->GetCommandList(), converted_str.c_str());
	//			//m_nvperf.rangeCommands.PushRange(Context->GetCommandList(), "A");
	//
	//        }
	//    }
	//    void EndBlockPerfSDK(CommandContext* Context)
	//    {
	//		m_nvperf.rangeCommands.PopRange(Context->GetCommandList());
	//    }
	void OnFrameStart()
	{
		bool res = m_nvperf.OnFrameStart(Graphics::g_CommandManager.GetGraphicsQueue().GetCommandQueue());
		//bool res = m_nvperf.OnFrameStart(Graphics::g_CommandManager.GetCommandQueue());
	}

} // EngineProfiling

void NestedTimingTree::PushProfilingMarker( const wstring& name, CommandContext* Context )
{
    sm_CurrentNode = sm_CurrentNode->GetChild(name);
    sm_CurrentNode->StartTiming(Context);
}

void NestedTimingTree::PopProfilingMarker( CommandContext* Context )
{
    sm_CurrentNode->StopTiming(Context);
    sm_CurrentNode = sm_CurrentNode->m_Parent;
}

void NestedTimingTree::Update( void )
{
    ASSERT(sm_SelectedScope != nullptr, "Corrupted profiling data structure");

    if (sm_SelectedScope == &sm_RootScope)
    {
        sm_SelectedScope = sm_RootScope.FirstChild();
        if (sm_SelectedScope == &sm_RootScope)
            return;
    }

    if (GameInput::IsFirstPressed( GameInput::kDPadLeft )
        || GameInput::IsFirstPressed( GameInput::kKey_left ))
    {
        //if still on graphs go back to text
        if (sm_CursorOnGraph)
            sm_CursorOnGraph = !sm_CursorOnGraph;
        else
            sm_SelectedScope->m_IsExpanded = false;
    }
    else if (GameInput::IsFirstPressed( GameInput::kDPadRight )
        || GameInput::IsFirstPressed( GameInput::kKey_right ))
    {
        if (sm_SelectedScope->m_IsExpanded == true && !sm_CursorOnGraph)
            sm_CursorOnGraph = true;
        else
            sm_SelectedScope->m_IsExpanded = true;
        //if already expanded go over to graphs

    }
    else if (GameInput::IsFirstPressed( GameInput::kDPadDown )
        || GameInput::IsFirstPressed( GameInput::kKey_down ))
    {
        sm_SelectedScope = sm_SelectedScope ? sm_SelectedScope->NextScope() : nullptr;
    }
    else if (GameInput::IsFirstPressed( GameInput::kDPadUp )
        || GameInput::IsFirstPressed( GameInput::kKey_up ))
    {
        sm_SelectedScope = sm_SelectedScope ? sm_SelectedScope->PrevScope() : nullptr;
    }
    else if (GameInput::IsFirstPressed( GameInput::kAButton ) 
        || GameInput::IsFirstPressed( GameInput::kKey_return ))
    {
        sm_SelectedScope->Toggle();
    }

}

void NestedTimingTree::DisplayNode( TextContext& Text, float leftMargin, float indent )
{
    if (this == &sm_RootScope)
    {
        m_IsExpanded = true;
        sm_RootScope.FirstChild()->m_IsExpanded = true;
    }
    else
    {
        if (sm_SelectedScope == this && !sm_CursorOnGraph)
            Text.SetColor( Color(1.0f, 1.0f, 0.5f) );
        else
            Text.SetColor( Color(1.0f, 1.0f, 1.0f) );
    

        Text.SetLeftMargin(leftMargin);
        Text.SetCursorX(leftMargin);

        if (m_Children.size() == 0)
            Text.DrawString("  ");
        else if (m_IsExpanded)
            Text.DrawString("- ");
        else
            Text.DrawString("+ ");

        Text.DrawString(m_Name.c_str());
        Text.SetCursorX(leftMargin + 300.0f);
        Text.DrawFormattedString("%6.3f %6.3f   ", m_CpuTime.GetAvg(), m_GpuTime.GetAvg());

        if (IsGraphed())
        {
            Text.SetColor(GraphRenderer::GetGraphColor(m_GraphHandle, GraphType::Profile));
            Text.DrawString("  []\n");
        }
        else
            Text.DrawString("\n");
    }

    if (!m_IsExpanded)
        return;

    for (auto node : m_Children)
        node->DisplayNode(Text, leftMargin + indent, indent);
}

void NestedTimingTree::StoreToGraph(void)
{
    if (m_GraphHandle != PERF_GRAPH_ERROR)
        GraphRenderer::Update( XMFLOAT2(m_CpuTime.GetLast(), m_GpuTime.GetLast()), m_GraphHandle, GraphType::Profile);

    for (auto node : m_Children)
        node->StoreToGraph();
}
