#pragma once
#include <plugin.h>
#include "core/base.h"
#include <vector>

struct ArticulatedData {
    FeatureFrameState frameState;
    RwFrame *turntableDisc = nullptr;  // povrama
    RwFrame *tractorPivot = nullptr;   // dozer_dummy
    std::vector<RwFrame*> trailerSteerWheels; // twheelleft, twheelright...

    float curDiscAngle = 0.0f;
    float curPivotAngle = 0.0f;
    float curTrailerSteerAngle = 0.0f;
    bool bInitialized = false;

    ArticulatedData(CVehicle *pVeh) {}
    ~ArticulatedData() {}
};

class ArticulatedVehicleFeature : public CVehFeature<ArticulatedData>
{
protected:
    void Init() override;
    void OnToggle(CVehicle *vehicle, bool enabled) override;

public:
    ArticulatedVehicleFeature() : CVehFeature<ArticulatedData>("ArticulatedVehicle", "FEATURES", eFeatureMatrix::ArticulatedVehicle) {}
};
