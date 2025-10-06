#pragma once
#include "CameraSequencer.h"
#include "math.h"
#include <algorithm>
#include "CameraController.h"
//#include "imgui.h"
#include "Imgui/imgui-master/imgui.h"
#include "fstream"
#include "GraphicsCore.h"

enum class SeqState { Idle, Move, Dwell, Finalize };

class CameraSequenceRunner : public CameraController
{
protected:
	bool InRange(int index)
	{
		return index >= 0 && index < sequence->cameraStops.size();
	}

	void SetCamera(int index)
	{
		if (InRange(index))
		{
			auto camStop = sequence->cameraStops[index];

			m_TargetCamera.SetPosition(camStop.position);
			m_TargetCamera.SetRotation(camStop.rotation);
		}
	}

	void WriteCSVHeader(std::ostream& outStream, std::vector<StageStamp>& stamps)
	{
		outStream << "Test" << ", ";
		outStream << "Sample" << ", ";
		outStream << "Frame" << ", ";
		outStream << "CameraIndex" << ", ";
		for each (StageStamp s in stamps)
		{
			outStream << s.name << "_CPU" << ", ";
		}
		for each (StageStamp s in stamps)
		{
			outStream << s.name << "_GPU" << ", ";
		}
		outStream << "\n";
	}
	void WriteCSVPerStageValues(std::ostream& outStream, std::vector<StageStamp> stamps, uint32_t frameIndex)
	{
		outStream << name << ", ";
		outStream << m_targetRunIndex << ", ";
		outStream << frameIndex << ", ";
		outStream << m_targetCameraIndex << ", ";
		for each (StageStamp s in stamps)
		{
			outStream << s.cpuMs << ", ";
		}
		for each (StageStamp s in stamps)
		{
			outStream << s.gpuMs << ", ";
		}
		outStream << "\n";
	}

public:
	bool m_NeedResetAlgorithm = false;
	bool m_FullyFinishedTests = false;
	bool UseAugmentedAlgorithm()
	{
		return sequence->m_useAugmentedAlgorithm;
	}
	CameraSequenceRunner(Camera& camera) :CameraController(camera)
	{
		paused = true;
		stampRingBufferSize = 5;
	}

	bool OpenCSVStream(std::string path)
	{
		//EngineProfiling::BeginSamplerSession();
		csvFileOutput = std::ofstream(path);
		if (!csvFileOutput.is_open())
			return false;
	}

	void Start() {
		m_NeedResetAlgorithm = true;
		m_FullyFinishedTests = false;
		paused = false;

		state = SeqState::Move;

		//OpenCSVStream("metrics.csv");
		std::string ouputFilename = "Tests/" + name + ".csv";
		OpenCSVStream(ouputFilename);

		EngineProfiling::BeginSamplerSession();
	}
	void Pause() {
		csvFileOutput.close();
		paused = true;
	}
	void Resume() {
		paused = false;
	}
	void Reset() {
		m_NeedResetAlgorithm = true;
		paused = true;
		m_FullyFinishedTests = true;
		hasWrittenHeader = false;

		m_targetCameraIndex = 0;
		m_targetRunIndex = 0;

		csvFileOutput.close();

		state = SeqState::Idle;
	}
	float timer = 0.0f;
	int testFrameCounter = 0;
	int stateFrameCounter = 0;

	void WriteRingBufferToCSV(std::ostream& outStrea)
	{
		int maxStampCount = 0;
		for each (auto stamps in stampRingBuffer)
		{
			if (stamps.size() > maxStampCount)
				maxStampCount = stamps.size();
		}

		int inner = stampRingBuffer.size();
		for each (auto stamps in stampRingBuffer)
		{
			if (stamps.size() == maxStampCount)
			{
				if (this->hasWrittenHeader == false)
				{
					WriteCSVHeader(csvFileOutput, stamps);
		//if (paused || state == SeqState::Idle) return;
					this->hasWrittenHeader = true;
				}
				WriteCSVPerStageValues(csvFileOutput, stamps, testFrameCounter-inner);
			}
			inner--;
		}
	}

	void Update(float deltaTime) override {

		if (paused) return;
		if (InRange(m_targetCameraIndex) == false) {
			Reset();
		}
		stateTimer += deltaTime;

		//Output per-stage nested timestamps
		auto frameIndex = EngineProfiling::GetFrameStampCollected();
		auto stamps = EngineProfiling::GetStageStamps();

		//Collected stamps
		if (stampRingBuffer.size() < stampRingBufferSize)
		{
			//Keep addiing to the buffer
			stampRingBuffer.push_back(stamps);
		}
		else
		{
			//Once ring buffer filled ouptput values to csv
			//It is possible that one of the stamp buffers has less stamps -- ignore it and write the rest

			int ringBufferIndex = (testFrameCounter % stampRingBufferSize);
			if (ringBufferIndex == 0)
			{
				WriteRingBufferToCSV(csvFileOutput);
			}
			stampRingBuffer[ringBufferIndex] = stamps;
		}

		//Consume stamp buffer

		switch (state)
		{
		case SeqState::Move:

		{
			//For now simply set the camera state
			SetCamera(m_targetCameraIndex);
			//And go to Dwell stage
			state = SeqState::Dwell;
			stateTimer = 0.0;
			stateFrameCounter = 0.0;
		}
		break;

		case SeqState::Dwell:
			//Wait for the specific time
			//if (stateTimer >= sequence->cameraStops[m_targetCameraIndex].dwellTime)
			if (stateFrameCounter >= sequence->cameraStops[m_targetCameraIndex].dwellTime)
			{
				state = SeqState::Finalize;
				stateFrameCounter = 0.0;
			}
			break;
		case SeqState::Finalize:
		{
			//Increment the camera state to be used
			m_targetCameraIndex++;
			//Reset if the cameras states are exhaused
			if (m_targetCameraIndex >= sequence->cameraStops.size())
			{
				m_targetRunIndex++;
				m_targetCameraIndex = 0;
				m_NeedResetAlgorithm = true;
				//Require that header is written every new run
				//hasWrittenHeader = false;
				if (m_targetRunIndex >= sequence->testSamples)
				{
					Reset();

				}
			}



			state = SeqState::Move;
			stateTimer = 0.0;
			stateFrameCounter = 0.0;
		}
		break;
		}
		m_TargetCamera.Update();
		testFrameCounter++;
		stateFrameCounter++;
	}
	bool IsRunning() const {
		return paused == false;
	}

	void RenderImGui()
	{
		// Your ImGui UI code here
		ImGui::Begin("Sequence Runner");

		ImGui::DragInt("Run Index", &m_targetRunIndex, 0.2f, 0,sequence->testSamples);
		ImGui::DragInt("CameraPosition", &m_targetCameraIndex, 0.2f, 0, sequence->cameraStops.size());
		ImGui::Checkbox("Fully Finished Test", &m_FullyFinishedTests);

		bool go = false;
		if (ImGui::Checkbox("Go To Camera", &go))
		{
			SetCamera(m_targetCameraIndex);
		}
		std::string debugStateTimer = "State Timere" + std::to_string(stateTimer);

		ImGui::Text(debugStateTimer.c_str());
		if (this->IsRunning())
			ImGui::Text("Running");
		else
			ImGui::Text("Paused");
		if (EngineProfiling::IsPaused())
			ImGui::Text("Not Profiling");
		else
			ImGui::Text("Profiling");

		ImGui::End();
	}
	void SetSequence(SequenceConfig* scfg, std::string testName)
	{
		this->sequence = scfg;
		this->name = testName;
	}
private:
	SeqState state = SeqState::Idle;
	double stateTimer = 0.0;
	Vector3 startPos, targetPos;
	Quaternion startRot, targetRot;
	bool paused = false;

	SequenceConfig* sequence = nullptr; // loaded config
	std::string name; // loaded config
	std::ofstream csvFileOutput;
	std::vector<std::vector<StageStamp>> stampRingBuffer;
	int stampRingBufferSize;
	bool hasWrittenHeader = false;
	int m_targetCameraIndex = 0;
	int m_targetRunIndex = 0;
	//MetricsLogger* logger = nullptr;
};
