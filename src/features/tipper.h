#pragma once
#include <plugin.h>
#include "core/base.h"
#include <vector>

struct TipperData {
    FeatureFrameState frameState;
    RwFrame *bedFrame = nullptr;
    std::vector<RwFrame*> hydraulicFrames; // gidro1..3 or hydraulic
    float curAngle = 0.0f;
    float maxAngle = 45.0f;
    bool bInitialized = false;

    TipperData(CVehicle *pVeh) {}
    ~TipperData() {}
};

class TipperHydraulicsFeature : public CVehFeature<TipperData>
{
protected:
    void Init() override;
    void OnToggle(CVehicle *vehicle, bool enabled) override;

public:
    TipperHydraulicsFeature() : CVehFeature<TipperData>("TipperHydraulics", "FEATURES", eFeatureMatrix::TipperHydraulics) {}
};
