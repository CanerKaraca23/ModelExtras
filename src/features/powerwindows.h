#pragma once
#include <plugin.h>
#include "core/base.h"

struct WindowPane {
    RwFrame *frame = nullptr;
    float curOffset = 0.0f; // 0.0 = fully closed, 1.0 = fully open
    float maxDrop = 0.40f;  // meters
};

struct PowerWindowsData {
    FeatureFrameState frameState;
    WindowPane leftWindow;
    WindowPane rightWindow;
    bool bInitialized = false;

    PowerWindowsData(CVehicle *pVeh) {}
    ~PowerWindowsData() {}
};

class PowerWindowsFeature : public CVehFeature<PowerWindowsData>
{
protected:
    void Init() override;
    void OnToggle(CVehicle *vehicle, bool enabled) override;

public:
    PowerWindowsFeature() : CVehFeature<PowerWindowsData>("PowerWindows", "FEATURES", eFeatureMatrix::PowerWindows) {}
};
