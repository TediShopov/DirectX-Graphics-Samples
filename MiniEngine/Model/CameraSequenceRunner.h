#pragma once
#include "CameraSequencer.h"
#include "math.h"
#include <algorithm>
//#include "imgui.h"
#include "Imgui/imgui-master/imgui.h"
enum class SeqState { Idle, Move, Dwell, Log };

class CameraSequenceRunner
{
public:
	Camera* m_controlledCamera;
    CameraSequenceRunner(Camera* camera) : m_controlledCamera(camera)
    {

            
    }
//    void Start();
//    void Pause();
//    void Resume();

    void SetCamera(int index) 
    {
        if (index >= 0 && index < sequence->cameraStops.size())
        {
            auto camStop = sequence->cameraStops[index];

			m_controlledCamera->SetPosition(camStop.position);
			m_controlledCamera->SetRotation(camStop.rotation);
        }
    }

    void Update(float deltaTime) {
		if (paused || state == SeqState::Idle) return;

		stateTimer += deltaTime;

		switch (state)
    {
    case SeqState::Move:

        {
            //float t = (float)(stateTimer / moveDuration);
		    float t = (float)(stateTimer / 1.0f);
            //Clamp
            if (t < 0.0f)
                t = 0.0f;
            if (t > 1.0f)
                t = 1.0f;

            auto pos = Lerp(startPos, targetPos, t);
            auto rot = Slerp(startRot, targetRot, t); // or slerp quaternion
            m_controlledCamera->SetPosition(pos);
            m_controlledCamera->SetRotation(rot);

            if (t >= 1.0f)
            {
                state = SeqState::Dwell;
                stateTimer = 0.0;
            }
        }
        break;

	case SeqState::Dwell:
        if (stateTimer >= sequence->cameraStops[currentIndex].dwellTime)
        {
            state = SeqState::Log;
            stateTimer = 0.0;
        }
        break;

	//case SeqState::Log:
    //    {
    //        if (sequence->GetConfig().logMetrics)
    //            logger->Log(currentIndex, GetCameraPos(), GetCameraRot(), CollectMetrics());

    //        if (sequence->GetConfig().captureScreenshots)
    //            CaptureSwapChainScreenshot(currentIndex);

    //        currentIndex++;
    //        if (currentIndex >= sequence->GetConfig().cameraStops.size())
    //            state = SeqState::Idle;
    //        else
    //        {
    //            startPos = GetCameraPos();
    //            startRot = GetCameraRot();
    //            targetPos = sequence->GetConfig().cameraStops[currentIndex].position;
    //            targetRot = sequence->GetConfig().cameraStops[currentIndex].rotationDeg;
    //            state = SeqState::Move;
    //            stateTimer = 0.0;
    //        }
    //    }
    //    break;
    }

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
		ImGui::End();

	}
    void SetSequence(SequenceConfig* scfg)
    {
        this->sequence = scfg;

    }
private:
    SeqState state = SeqState::Idle;
    size_t currentIndex = 0;
    double stateTimer = 0.0;
    Vector3 startPos, targetPos;
    Quaternion startRot, targetRot;
    bool paused = false;

    SequenceConfig* sequence = nullptr; // loaded config
    //MetricsLogger* logger = nullptr;
};


