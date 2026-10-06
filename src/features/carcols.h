#pragma once
#include <plugin.h>
#include "core/base.h"
#include <vector>
#include <unordered_map>
#include <array>

class CVehicleModelInfo;



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
    struct ModelColorOverride {
        CVehicleModelInfo *info;
        std::array<unsigned char, 33> before, after;
    };
    static inline std::unordered_map<int, ModelColorOverride> modelColorOverrides;
    static bool ApplyModelVariations(int model, const nlohmann::json &carcols, size_t paletteSize);

protected:
    void Init() override;
    void ReloadConfig() override;
    void Reload(CVehicle *pVeh) override {
        if (pVeh) m_VehData.Get(pVeh) = CarcolsData(pVeh);
    }

public:
    Carcols() : CVehFeature<CarcolsData>("Carcols", "FEATURES", eFeatureMatrix::IVFCarcols) {}
    static void Parse(const nlohmann::json &data, int model);
    static bool GetColor(CVehicle *pVeh, RpMaterial *pMat, CRGBA &col);
    static void RestoreModelVariations();
};
