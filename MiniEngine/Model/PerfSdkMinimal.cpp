#include "PerfSdkMinimal.h"

//bool PerfSdkMinimal::Initialize(ID3D12Device* device)
//{
//    if (initialized) return true;
//    if (!device) return false;
//    NVPW_InitializeHost_Params params;
//    params.
//
//    // Host init — returns success even if driver doesn't expose profiler features.
//    NVPA_Status hs = NVPW_InitializeHost();
//    if (!NV_OK(hs)) {
//        std::cerr << "NVPA_InitializeHost failed\n";
//        return false;
//    }
//
//    // DX12 host init (bind to your device)
//    NVPW_D3D12_InitializeHost_Params initP = { NVPW_D3D12_InitializeHost_Params_STRUCT_SIZE };
//    initP.pDevice = device;
//    NVPA_Status s = NVPW_D3D12_InitializeHost(&initP);
//    if (!NV_OK(s)) {
//        std::cerr << "NVPW_D3D12_InitializeHost failed\n";
//        // note: call NVPA_ShutdownHost() if desired
//        return false;
//    }
//
//    dev = device;
//    initialized = true;
//    return true;
//}
//
//void PerfSdkMinimal::Shutdown()
//{
//}
//
//bool PerfSdkMinimal::CreateRangeProfiler(const std::vector<std::string>& metricNames)
//{
//    return false;
//}
//
//void PerfSdkMinimal::DestroyRangeProfiler()
//{
//}
//
//void PerfSdkMinimal::BeginPass(ID3D12GraphicsCommandList* cmdList)
//{
//}
//
//void PerfSdkMinimal::EndPass(ID3D12GraphicsCommandList* cmdList)
//{
//}
//
//bool PerfSdkMinimal::Evaluate(std::map<std::string, double>& outMetrics)
//{
//    return false;
//}
