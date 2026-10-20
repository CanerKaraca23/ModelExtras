#pragma once
#include <plugin.h>
#include "core/base.h"
#include <vector>

enum class eWheelPos
{
    LeftFront,
    RightFront,
    LeftMiddle,
    RightMiddle,
    LeftRear,
    RightRear,
    COUNT,
};

struct ExtraWheelData
{
    FeatureFrameState frameState;
    std::vector<RwFrame*> pOriginals[static_cast<int>(eWheelPos::COUNT)];
    std::vector<RwFrame*> pExtras[static_cast<int>(eWheelPos::COUNT)];

    // Active Dashboard multi-axle steering
    std::vector<RwFrame*> pFrontSteerWheels; // wheellef, wheelrig, dwheelleft, dwheelright
    std::vector<RwFrame*> pRearSteerWheels;  // rwheellef, rwheelrig
    float curFrontSteer = 0.0f;
    float curRearSteer = 0.0f;
    float frontSteerRatio = 1.0f;
    float rearSteerRatio = -0.5f;

    ExtraWheelData(CVehicle *pVeh) {}
    ~ExtraWheelData() {}
};

class ExtraWheel : public CVehFeature<ExtraWheelData>
{
protected:
    void Init() override;
    void OnToggle(CVehicle *vehicle, bool enabled) override;

public:
    ExtraWheel() : CVehFeature<ExtraWheelData>("ExtraWheels", "FEATURES", eFeatureMatrix::ExtraWheels) {}
};
