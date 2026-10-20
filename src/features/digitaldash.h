#pragma once
#include <plugin.h>
#include "core/base.h"
#include <vector>

struct DigitalDashData {
    FeatureFrameState frameState;
    std::vector<RwFrame*> speedDigits;   // dsp_speedo1..3
    std::vector<RwFrame*> odoDigits;     // dsp_odometr1..10
    std::vector<RwFrame*> clockHourDigits; // dsp_hour1..2
    std::vector<RwFrame*> clockMinDigits;  // dsp_min1..2
    double totalOdo = 0.0;
    float lastWheelRot = 0.0f;
    bool bInitialized = false;

    DigitalDashData(CVehicle *pVeh) {}
    ~DigitalDashData() {}
};

class DigitalDashboardFeature : public CVehFeature<DigitalDashData>
{
protected:
    void Init() override;
    void OnToggle(CVehicle *vehicle, bool enabled) override;

public:
    DigitalDashboardFeature() : CVehFeature<DigitalDashData>("DigitalDashboard", "FEATURES", eFeatureMatrix::DigitalDashboard) {}
};
