#pragma once
#include "Camera.h"
#include <vector>
#include <string>
//#include "imgui.h"
#include "Imgui/imgui-master/imgui.h"
#include <sstream>
#include "json.hpp"
#include <fstream>
#include "IParameterBlock.h"

using namespace Math;

struct CameraStop
{
	Vector3 position;
	Quaternion rotation;
    int dwellTime;
};



struct SequenceConfig
{
	bool m_useAugmentedAlgorithm = false;
	int	testSamples;
    std::vector<CameraStop> cameraStops;
    bool logMetrics = true;
    bool captureScreenshots = true;
};


class X {
public:
    X(std::string& s) : s_{s} { s.resize(len_); }
    ~X() { s_.resize(strlen(s_.c_str())); }
    operator char*(){ return (char*)s_.data(); }
    static constexpr auto len() { return len_-1; }
private:
    std::string& s_;
    static constexpr auto len_=255;
};


class CameraSequencer
{
private:
	SequenceConfig m_config;
	bool m_expanded = true;
	bool m_parameterSection = true;
	bool m_save, m_load, m_reset;
public:
	const std::string TestFolder = "Tests/";
	bool m_pathLoaded = false;
	std::string m_targetName = "NONE";			// The path visualized in the ImGui that will Save/Load from
	std::string m_targetPath = "NONE";			// The path visualized in the ImGui that will Save/Load from

	std::vector<IParameterBlock*> m_parametersBlocks;
	
	CameraSequencer()
	{
		m_config.captureScreenshots = false;
		m_config.logMetrics = false;
		m_config.testSamples = 10;
		m_save = false;
		m_load = false;
		m_reset = false;
	}
	bool LoadConfigName(const std::string& name) {

		m_targetPath = TestFolder + m_targetName + ".json";
		std::ifstream f(m_targetPath);
		if (!f.is_open()) return false;

		nlohmann::json j;
		f >> j;

		m_config.cameraStops.clear();
		for (auto& s : j["cameraStops"])
		{
			CameraStop stop;
			stop.position = { s["position"][0], s["position"][1], s["position"][2] };
			stop.rotation =Quaternion(XMVectorSet(s["rotation"][0], s["rotation"][1], s["rotation"][2], s["rotation"][3]));
			stop.dwellTime = s["dwellTime"];
			m_config.cameraStops.push_back(stop);
		}
		m_config.logMetrics = j.value("logMetrics", true);
		m_config.captureScreenshots = j.value("captureScreenshots", true);
		m_config.testSamples = j.value("testSamples", m_config.testSamples);
		m_config.m_useAugmentedAlgorithm = j.value("useAugmented", m_config.m_useAugmentedAlgorithm);

		for each (auto paramBlock in m_parametersBlocks)
			paramBlock->FromJson(j);
		

		return true;
	}

	bool LoadConfig(const std::string& path) {


		m_targetPath =  path;
		std::ifstream f(path);
		if (!f.is_open()) return false;

		nlohmann::json j;
		f >> j;

		m_config.cameraStops.clear();
		for (auto& s : j["cameraStops"])
		{
			CameraStop stop;
			stop.position = { s["position"][0], s["position"][1], s["position"][2] };
			stop.rotation =Quaternion(XMVectorSet(s["rotation"][0], s["rotation"][1], s["rotation"][2], s["rotation"][3]));
			stop.dwellTime = s["dwellTime"];
			m_config.cameraStops.push_back(stop);
		}
		m_config.logMetrics = j.value("logMetrics", true);
		m_config.captureScreenshots = j.value("captureScreenshots", true);
		m_config.testSamples = j.value("testSamples", m_config.testSamples);
		m_config.m_useAugmentedAlgorithm = j.value("useAugmented", m_config.m_useAugmentedAlgorithm);

		for each (auto paramBlock in m_parametersBlocks)
			paramBlock->FromJson(j);
		

		return true;
	}
	bool SaveConfig(const std::string& path)
	{

		nlohmann::json j;
		j["testSamples"] = m_config.testSamples;
		j["logMetrics"] = m_config.logMetrics;
		j["captureScreenshots"] = m_config.captureScreenshots;
		j["useAugmented"] = m_config.m_useAugmentedAlgorithm;
		j["cameraStops"] = nlohmann::json::array();


		XMFLOAT3 pos;
		XMFLOAT4 rot;
		for (auto& s : m_config.cameraStops)
		{

			XMStoreFloat3(&pos, (XMVECTOR)s.position);
			XMStoreFloat4(&rot, (XMVECTOR)s.rotation);
			nlohmann::json stop;
			stop["position"] = {pos.x,pos.y,pos.z};
			stop["rotation"] = {rot.x, rot.y, rot.z, rot.w};
			stop["dwellTime"] = s.dwellTime;

			j["cameraStops"].push_back(stop);
		}

		for each (auto paramBlock in m_parametersBlocks)
			paramBlock->ToJson(j);

		std::ofstream f(path);
		if (!f.is_open()) return false;
		f << j.dump(4);
		return true;

	}
	void AddStop(const DirectX::XMFLOAT3& pos,
		const DirectX::XMFLOAT3& rotDeg,
		float dwell = 2.0f) {


	}
	void AddStop(const Math::Camera& Camera, int dwell = 60) {
		CameraStop stop;
		stop.position = Camera.GetPosition();
		stop.rotation = Camera.GetRotation();
		stop.dwellTime = dwell;

		m_config.cameraStops.push_back(stop);




	}
	void Reset()
	{
		m_config.cameraStops.clear();
		m_config.logMetrics= false;
		m_config.captureScreenshots= false;
	}

	void RenderImGui()
	{

		// Your ImGui UI code here
		ImGui::Begin("Camera Sequence Debug View");


		if (ImGui::InputText("Target Test File", X(m_targetName), X::len()))
		{
			int a = 3;
		}
		m_targetPath = TestFolder + m_targetName + ".json";

		for each (auto paramBlock in m_parametersBlocks)
		{
			paramBlock->RenderImGui();
		}


		std::ostringstream displayNameSection;
		displayNameSection << "Camera Sequence: " << m_targetPath;

		ImGui::DragInt("Test Samples", &m_config.testSamples, 0.2, 0, 100);
		ImGui::Checkbox("SSGI+HBIL OR SBGI", &m_config.m_useAugmentedAlgorithm);

		if (ImGui::CollapsingHeader("TestParamtere", &m_parameterSection))
		{
			//Surfel GI Parameters



		}
		if (ImGui::CollapsingHeader(displayNameSection.str().c_str(), &m_expanded))
		{
			std::ostringstream debugStringCameraStops;
			for (size_t i = 0; i < m_config.cameraStops.size(); i++)
			{

				CameraStop& cameraStop = m_config.cameraStops[i];
				XMFLOAT4 rot;
				XMStoreFloat4(&rot, (XMVECTOR)cameraStop.rotation);
				debugStringCameraStops
					<< "Position: " << cameraStop.position.GetX() << "	" << cameraStop.position.GetY() << "	" << cameraStop.position.GetZ()
					<< "Rotation: " << rot.x << "	" << rot.y << "		" << rot.z
					<< "Dwell: " << cameraStop.dwellTime
					<< "\n";

			}

			// Display all the camera stops for debugging
			ImGui::Text(debugStringCameraStops.str().c_str());


			if (ImGui::Checkbox("Save", &m_save))
			{
				SaveConfig(m_targetPath);
				m_save = false;
			}
			;
			if (ImGui::Checkbox("Load", &m_load))
			{
				LoadConfig(m_targetPath);
				m_load = false;
			}
			if (ImGui::Checkbox("Reset", &m_reset))
			{
				Reset();
				m_reset = false;

			}

		}
		ImGui::End();

	}

	SequenceConfig& GetMutableConfig() { return m_config; }
	std::string GetName() { return m_targetName; }


};

