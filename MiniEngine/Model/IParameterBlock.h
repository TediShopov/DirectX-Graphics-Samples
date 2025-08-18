#pragma once
#include "json.hpp"

class IParameterBlock {
public:
    virtual ~IParameterBlock() = default;

    // Renders UI using ImGui and allows editing parameters
    virtual void RenderImGui() = 0;

    // Populates a JSON object with parameter data
    virtual void ToJson(nlohmann::json& outJson) const = 0;

    // Loads parameter data from a JSON object
    virtual void FromJson(const nlohmann::json& inJson) = 0;
};


