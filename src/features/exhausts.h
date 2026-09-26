#pragma once
#include "plugin.h"
#include "core/base.h"
#include <unordered_map>
#include <CParticle.h>

using namespace plugin;

struct ME_ExhaustInfo;

struct ExhaustData
{
    std::string sName;
    RwFrame *pFrame = nullptr;
    CRGBA Color = {150, 150, 150, 200}; // Dark grey default
    float fLifeTime = 0.4f;             // Default smoke lifetime (shortened to prevent excessive rising)
    float fSpeedMul = 1.0f;             // Speed multiplier
    float fSizeMul = 1.0f;
    bool bNitroEffect = true;
};

struct ExhaustVehData {
    bool isUsed = false;
    bool bNodesSearched = false;
    size_t reloadCount = 0;
    unsigned int lastNitroFrame = 0;
    std::vector<std::pair<std::string, ExhaustData>> m_pDummies;
    ExhaustVehData(CVehicle *pVeh) { isUsed = false; bNodesSearched = false; }

    ~ExhaustVehData() {
        m_pDummies.clear();
    }
};

class ExhaustFx : public CVehFeature<ExhaustVehData>
{
private:
    static inline bool bEnabled = false;
    static inline size_t nReloadCount = 0;

    static void RenderSmokeFx(CVehicle *pVeh, const ExhaustData &info);
    static void RenderNitroFx(CVehicle *pVeh, float power);
    static ExhaustData LoadData(CVehicle *pVeh, RwFrame *pFrame);
    static void FindNodes(CVehicle *pVeh, RwFrame *frame);
    static void ProcessPointLights(CVehicle *pVeh);

protected:
    void Init() override;

public:
    ExhaustFx() : CVehFeature<ExhaustVehData>("ExhaustFx", "FEATURES", eFeatureMatrix::ExhaustFx) {}
    void Reload(CVehicle* pVeh) override;
};
