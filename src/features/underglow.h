#pragma once
#include <plugin.h>
#include "core/base.h"
#include "utils/propershaders.h"
#include <vector>

struct UnderglowData
{
    std::vector<RwFrame*> pDummies;
    std::vector<PS_LightHandle> hLights;
    CRGBA color{0, 200, 255, 255};
    float fIntensity = 1.2f;
    std::string mode = "solid";
    bool bConfigured = false;
    bool bHasJsonc = false;

    UnderglowData(CVehicle* pVeh) {}
    ~UnderglowData()
    {
        for (auto& h : hLights)
        {
            ProperShadersMgr::DestroyLight(h);
        }
        hLights.clear();
    }
};

class Underglow : public CVehFeature<UnderglowData>
{
protected:
    void Init() override;

public:
    Underglow() : CVehFeature<UnderglowData>("Underglow", "FEATURES", eFeatureMatrix::Underglow) {}

    static void Process(CVehicle* pVeh);
    void ReloadConfig() override;
    void Reload(CVehicle* pVeh) override;
};
