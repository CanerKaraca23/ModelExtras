#pragma once
#include <plugin.h>
#include "core/base.h"
#include <vector>
#include <unordered_map>



struct ColorSet
{
    CRGBA primary, secondary, tert, quart;
};

struct CarcolsData
{
    int randId = -1;
    bool m_bPri = false, m_bSec = false, m_bTer = false, m_bQuat = false;
    ColorSet m_Colors;

    CarcolsData(CVehicle *pVeh) {}
    ~CarcolsData() {}
};

class Carcols : public CVehFeature<CarcolsData>
{
private:
    
    static inline bool m_bEnabled = false;
    static inline bool m_bMultiplayer = false;
    static inline std::unordered_map<int, std::vector<ColorSet>> variations;
    static inline std::unordered_map<int, std::vector<CRGBA>> indexedPalettes;

protected:
    void Init() override;
    void ReloadConfig() override;
    void Reload(CVehicle *pVeh) override {
        ReloadConfig();
        if (pVeh) m_VehData.Get(pVeh) = CarcolsData(pVeh);
    }

public:
    Carcols() : CVehFeature<CarcolsData>("Carcols", "FEATURES", eFeatureMatrix::IVFCarcols) {}
    static void Parse(const nlohmann::json &data, int model);
    static bool GetColor(CVehicle *pVeh, RpMaterial *pMat, CRGBA &col);
};
