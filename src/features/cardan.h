#pragma once
#include <plugin.h>
#include "core/base.h"
#include <vector>

struct CardanJoint {
    RwFrame *frame = nullptr;
    bool isFrontShaft = false;
    float lastWheelRot = 0.0f;
    float curSpinAngle = 0.0f;
    float curTiltAngle = 0.0f;
    float maxTiltAngle = 15.0f;
};

struct CardanData {
    FeatureFrameState frameState;
    std::vector<CardanJoint> shafts;
    bool bInitialized = false;

    CardanData(CVehicle *pVeh) {}
    ~CardanData() {}
};

class CardanShaftFeature : public CVehFeature<CardanData>
{
protected:
    void Init() override;
    void OnToggle(CVehicle *vehicle, bool enabled) override;

public:
    CardanShaftFeature() : CVehFeature<CardanData>("CardanShaft", "FEATURES", eFeatureMatrix::CardanShaft) {}
};
