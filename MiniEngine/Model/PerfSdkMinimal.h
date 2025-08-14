//// PerfSdkMinimal.h (concept)
//#pragma once
//#include <map>
//#include <string>
//#include <vector>
//#include <d3d12.h>
//
//#include <nvperf_host.h>        // host init
//#include "NvPerfD3D12.h"
//#include "NvPerfRangeProfilerD3D12.h"
//
//class PerfSdkMinimal
//{
//public:
//    PerfSdkMinimal() = default;
//    ~PerfSdkMinimal() { Shutdown(); }
//
//    bool Initialize(ID3D12Device* device);
//    void Shutdown();
//
//    bool CreateRangeProfiler(const std::vector<std::string>& metricNames);
//    void DestroyRangeProfiler();
//
//    void BeginPass(ID3D12GraphicsCommandList* cmdList);
//    void EndPass(ID3D12GraphicsCommandList* cmdList);
//
//    bool Evaluate(std::map<std::string,double>& outMetrics); // call after GPU work is finished
//
//    bool IsAvailable() const { return initialized; }
//
//private:
//    bool initialized = false;
//    ID3D12Device* dev = nullptr;
//    //NVPW_D3D12_RangeProfiler* rangeProfiler = nullptr;
//    std::vector<std::string> activeMetrics;
//};
//
//
