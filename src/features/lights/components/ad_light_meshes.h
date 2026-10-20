#pragma once
#include <plugin.h>
#include "features/core/base.h"
#include "features/lights/data.h"
#include "features/lights/manager.h"
#include "utils/frame.h"
#include <vector>

struct ADLightMeshesData {
    RwFrame *llighton = nullptr;
    RwFrame *llightoff = nullptr;
    RwFrame *rlighton = nullptr;
    RwFrame *rlightoff = nullptr;
    RwFrame *backlion = nullptr;
    RwFrame *backlioff = nullptr;
    std::vector<RwFrame*> povorotLeft;   // 1povorot
    std::vector<RwFrame*> povorotRight;  // 2povorot
    std::vector<RwFrame*> rearLights;    // rearlight, 3povorot
    bool bInitialized = false;

    ADLightMeshesData(CVehicle *pVeh = nullptr) {}
    ~ADLightMeshesData() {}
};

class ADLightMeshesManager {
public:
    static inline VehicleExtendedData<ADLightMeshesData> m_VehData;

    static void RegisterNode(CVehicle *pVeh, RwFrame *pFrame, const std::string_view name) {
        if (!pVeh || !pFrame) return;

        bool matched = false;
        ADLightMeshesData &data = m_VehData.Get(pVeh);

        if (name == "llighton") { data.llighton = pFrame; matched = true; }
        else if (name == "llightoff") { data.llightoff = pFrame; matched = true; }
        else if (name == "rlighton") { data.rlighton = pFrame; matched = true; }
        else if (name == "rlightoff") { data.rlightoff = pFrame; matched = true; }
        else if (name == "backlion") { data.backlion = pFrame; matched = true; }
        else if (name == "backlioff") { data.backlioff = pFrame; matched = true; }
        else if (name.contains("1povorot")) { data.povorotLeft.push_back(pFrame); matched = true; }
        else if (name.contains("2povorot")) { data.povorotRight.push_back(pFrame); matched = true; }
        else if (name.contains("rearlight") || name.contains("3povorot")) { data.rearLights.push_back(pFrame); matched = true; }

        if (matched) {
            data.bInitialized = true;
        }
    }

    static void Update(CVehicle *pVeh) {
        if (!pVeh || !pVeh->GetIsOnScreen()) return;
        ADLightMeshesData &data = m_VehData.Get(pVeh);
        if (!data.bInitialized) return;

        VehLightData &lightData = LightManager::m_VehData.Get(pVeh);
        bool blinkDelay = BlinkerState::Get().bIndicatorsDelay;

        bool leftBlink = (lightData.nIndicatorState == eIndicatorState::LeftOn || lightData.nIndicatorState == eIndicatorState::BothOn) && blinkDelay;
        bool rightBlink = (lightData.nIndicatorState == eIndicatorState::RightOn || lightData.nIndicatorState == eIndicatorState::BothOn) && blinkDelay;
        bool isReverse = (pVeh->m_nCurrentGear == 0 && pVeh->bEngineOn);

        // Left indicator meshes
        if (data.llighton && data.llightoff) {
            if (leftBlink) {
                FrameUtil::ShowAllAtomics(data.llighton);
                FrameUtil::HideAllAtomics(data.llightoff);
            } else {
                FrameUtil::HideAllAtomics(data.llighton);
                FrameUtil::ShowAllAtomics(data.llightoff);
            }
        }
        for (RwFrame *f : data.povorotLeft) {
            if (leftBlink) FrameUtil::ShowAllAtomics(f); else FrameUtil::HideAllAtomics(f);
        }

        // Right indicator meshes
        if (data.rlighton && data.rlightoff) {
            if (rightBlink) {
                FrameUtil::ShowAllAtomics(data.rlighton);
                FrameUtil::HideAllAtomics(data.rlightoff);
            } else {
                FrameUtil::HideAllAtomics(data.rlighton);
                FrameUtil::ShowAllAtomics(data.rlightoff);
            }
        }
        for (RwFrame *f : data.povorotRight) {
            if (rightBlink) FrameUtil::ShowAllAtomics(f); else FrameUtil::HideAllAtomics(f);
        }

        // Reverse light meshes
        if (data.backlion && data.backlioff) {
            if (isReverse) {
                FrameUtil::ShowAllAtomics(data.backlion);
                FrameUtil::HideAllAtomics(data.backlioff);
            } else {
                FrameUtil::HideAllAtomics(data.backlion);
                FrameUtil::ShowAllAtomics(data.backlioff);
            }
        }
        for (RwFrame *f : data.rearLights) {
            if (isReverse) FrameUtil::ShowAllAtomics(f); else FrameUtil::HideAllAtomics(f);
        }
    }
};
