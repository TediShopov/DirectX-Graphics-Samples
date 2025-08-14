#pragma once
#include "CameraSequencer.h"
#include "math.h"
#include <algorithm>
#include "CameraController.h"
//#include "imgui.h"
#include "Imgui/imgui-master/imgui.h"
enum class SeqState { Idle, Move, Dwell, Log };

class CameraSequenceRunner : public CameraController
{
public:
    CameraSequenceRunner(Camera& camera) :CameraController(camera)
    {

            
    }

    void Start() {
        paused = false;
        state = SeqState::Move;
    }
    void Pause() {
        paused = true;
    }
    void Resume() {
        paused = false;

    }
    void Reset() {
		m_targetCameraIndex = 0;
		paused = true;
		state = SeqState::Idle;


    }

    void SetCamera(int index) 
    {
        if (index >= 0 && index < sequence->cameraStops.size())
        {
            auto camStop = sequence->cameraStops[index];

			m_TargetCamera.SetPosition(camStop.position);
			m_TargetCamera.SetRotation(camStop.rotation);
        }
    }

    float timer = 0.0f;
    void Update(float deltaTime) override {
		//if (paused || state == SeqState::Idle) return;
		if (paused ) return;
		m_TargetCamera.SetPosition(Vector3(0,0,0));
        stateTimer += deltaTime;

        SetCamera(m_targetCameraIndex);

		switch (state)
    {
    case SeqState::Move:

        {
            //float t = (float)(stateTimer / moveDuration);
		    //float t = (float)(stateTimer / 1.0f);
            //Clamp
//            if (t < 0.0f)
//                t = 0.0f;
//            if (t > 1.0f)
//                t = 1.0f;
//
//            auto pos = Lerp(startPos, targetPos, t);
//            auto rot = Slerp(startRot, targetRot, t); // or slerp quaternion
//            m_TargetCamera.SetPosition(pos);
//            m_TargetCamera.SetRotation(rot);

        float t = 2.0f;

            if (t >= 1.0f)
            {
                state = SeqState::Dwell;
                stateTimer = 0.0;
            }
        }
        break;

	case SeqState::Dwell:
        if (stateTimer >= sequence->cameraStops[m_targetCameraIndex].dwellTime)
        {
            state = SeqState::Log;
            stateTimer = 0.0;
        }
        break;

	case SeqState::Log:
        {
		//if (sequence->GetConfig().logMetrics)
		//    logger->Log(currentIndex, GetCameraPos(), GetCameraRot(), CollectMetrics());

		//if (sequence->GetConfig().captureScreenshots)
		//    CaptureSwapChainScreenshot(currentIndex);

		//currentIndex++;
		//if (currentIndex >= sequence->GetConfig().cameraStops.size())
		//    state = SeqState::Idle;
		//else
		//{
		//    startPos = GetCameraPos();
		//    startRot = GetCameraRot();
		//    targetPos = sequence->GetConfig().cameraStops[currentIndex].position;
		//    targetRot = sequence->GetConfig().cameraStops[currentIndex].rotationDeg;
		//    state = SeqState::Move;
		//    stateTimer = 0.0;
		//}
		m_targetCameraIndex++;
        if (m_targetCameraIndex >= sequence->cameraStops.size())
            Reset();

		state = SeqState::Move;
		stateTimer = 0.0;
        }
        break;
    }
        m_TargetCamera.Update();

    }
    bool IsRunning() const;

    int m_targetCameraIndex=0;

	void RenderImGui()
	{

		// Your ImGui UI code here
		ImGui::Begin("Sequence Runner");
        

        ImGui::DragInt("CameraPosition", &m_targetCameraIndex, 0.2f,0, sequence->cameraStops.size());


        bool go = false;
		if (ImGui::Checkbox("Go To Camera", &go))
		{
            SetCamera(m_targetCameraIndex);

		}
        std::string debugStateTimer = "State Timere" + std::to_string(stateTimer);
        
        ImGui::Text(debugStateTimer.c_str());
		ImGui::End();

	}
    void SetSequence(SequenceConfig* scfg)
    {
        this->sequence = scfg;

    }
private:
    SeqState state = SeqState::Idle;
    //size_t currentIndex = 0;
    double stateTimer = 0.0;
    Vector3 startPos, targetPos;
    Quaternion startRot, targetRot;
    bool paused = false;

    SequenceConfig* sequence = nullptr; // loaded config
    //MetricsLogger* logger = nullptr;
};


