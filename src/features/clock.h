#pragma once
#include <plugin.h>
#include "core/base.h"
#include <vector>

struct ClockData
{
    FeatureFrameState frameState;
    RwFrame *m_pRootFrame = nullptr;
    RwFrame *m_pDigitsRoot = nullptr;
    RwFrame *m_pDigitPos[4] = {nullptr, nullptr, nullptr, nullptr};
    bool m_b12HourFormat = false;

    // Analog clock hands (Active Dashboard compatibility)
    RwFrame *m_pHourHand = nullptr;
    RwFrame *m_pMinuteHand = nullptr;
    float curHourAngle = 0.0f;
    float curMinAngle = 0.0f;

    ClockData(CVehicle *pVeh) {}
    ~ClockData() {}
};

class DigitalClockFeature : public CVehFeature<ClockData>
{
protected:
    void Init() override;
    void OnToggle(CVehicle *vehicle, bool enabled) override;

public:
    DigitalClockFeature() : CVehFeature<ClockData>("DigitalClock", "FEATURES", eFeatureMatrix::Clock) {}
};
