#pragma once
#include <string>
#include "imgui.h"
#include "CameraSequencer.h"


//class X {
//public:
//    X(std::string& s) : s_{s} { s.resize(len_); }
//    ~X() { s_.resize(strlen(s_.c_str())); }
//    operator char*(){ return (char*)s_.data(); }
//    static constexpr auto len() { return len_-1; }
//private:
//    std::string& s_;
//    static constexpr auto len_=255;
//};



class TestSuiteRunner
{
public:
    TestSuiteRunner()
        : m_baseName(""), m_count(0), m_index(0), m_active(false)
    {}

    // Start a new test suite with a base name and total number of tests
    void Start( std::string baseName, int count)
    {
        m_baseName = baseName;
        m_count = count;
        m_index = 0;
        m_active = (count > 0);
    }

    // Advance to the next test and return its name
    std::string GetName()
    {
        if (!m_active || m_index >= m_count)
        {
            m_active = false;
            return "";
        }

        return m_baseName + std::to_string(m_index);
    }
    void MoveNext()
    {
        ++m_index;

    }

    // Is the test suite still running?
    bool IsActive() const { return m_active; }
    void RenderImGui()
    {

		ImGui::Begin("Test Suite Runner");
        bool start = false;
        ImGui::Checkbox("Start", &start);
        ImGui::InputText("Target Base Name", X(m_baseName), X::len());
        ImGui::DragInt("Test Count",&m_count,0.2f,0,15);
        if (start)
        {
            this->Start(m_baseName, m_count);

        }
        

        if (m_active)
        {
			std::string peekNextName = m_baseName + std::to_string(m_index);
			ImGui::Text(peekNextName.c_str());
        }
		ImGui::End();


    }
    void SetActive(bool b)
    {
        m_active = b;

    }

private:
    std::string m_baseName;
    int m_count;
    int m_index;
    bool m_active;
};