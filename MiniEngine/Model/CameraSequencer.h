#pragma once
#include "Camera.h"
#include <vector>
#include <string>
#include "imgui.h"
#include <sstream>
#include "json.hpp"
#include <fstream>

using namespace Math;

struct CameraStop
{
	Vector3 position;
	Quaternion rotation;
    float dwellTime;
};

struct SequenceConfig
{
    std::vector<CameraStop> cameraStops;
    bool logMetrics = true;
    bool captureScreenshots = true;
};

class CameraSequencer
{

public:
	bool m_pathLoaded = false;
	std::string m_loadedPath = "NONE";
	CameraSequencer()
	{
		m_loadedPath = "DebugConfigPath.json";
		m_config.captureScreenshots = false;
		m_config.logMetrics = false;
	}

	bool LoadConfig(const std::string& path) {


		m_loadedPath = path;
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

		return true;
	}
	bool SaveConfig(const std::string& path)
	{

		nlohmann::json j;
		j["logMetrics"] = m_config.logMetrics;
		j["captureScreenshots"] = m_config.captureScreenshots;
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

		std::ofstream f(path);
		if (!f.is_open()) return false;
		f << j.dump(4);
		return true;

	}
	void AddStop(const DirectX::XMFLOAT3& pos,
		const DirectX::XMFLOAT3& rotDeg,
		float dwell = 2.0f) {


	}
	void AddStop(const Math::Camera& Camera, float dwell = 2.0f) {
		CameraStop stop;
		stop.position = Camera.GetPosition();
		stop.rotation = Camera.GetRotation();
		stop.dwellTime = dwell;

		m_config.cameraStops.push_back(stop);




	}

	bool m_expanded = true;
	void RenderImGui()
	{
		std::ostringstream displayNameSection;
		displayNameSection << "Camera Sequcen: " << m_loadedPath;

		// Your ImGui UI code here
		ImGui::Begin("Camera Sequence Debug View");

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

			bool save, load, c;
			ImGui::Checkbox("Save", &save);

			if (save)
			{
				SaveConfig(m_loadedPath);
			}
			ImGui::Checkbox("Load", &load);
			if (load)
			{
				LoadConfig(m_loadedPath);
			}
			ImGui::Checkbox("C", &c);
		}
		ImGui::End();

	}

	SequenceConfig& GetMutableConfig() { return m_config; }
private:
	SequenceConfig m_config;


};

