#pragma once
#include "Camera.h"
#include <vector>
#include <string>
#include "imgui.h"
#include <sstream>

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
		m_config.captureScreenshots = false;
		m_config.logMetrics = false;
	}

	bool LoadConfig(const std::string& path) {
		m_loadedPath = path;
		m_pathLoaded = true;
		m_config.captureScreenshots = false;
		m_config.logMetrics = false;
		return true;
	}
	bool SaveConfig(const std::string& path)
	{
		return false;

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

			bool a, b, c;
			ImGui::Checkbox("A", &a);
			ImGui::Checkbox("B", &b);
			ImGui::Checkbox("C", &c);
		}
		ImGui::End();

	}

	SequenceConfig& GetMutableConfig() { return m_config; }
private:
	SequenceConfig m_config;


};

