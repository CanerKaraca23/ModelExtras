#include "pch.h"
#include "springs.h"
#include "utils/modelinfomgr.h"
#include <CAutomobile.h>
#include <algorithm>

static int GetSpringWheelIndex(const std::string_view name)
{
    // Front Left (0)
    if (name.starts_with("prush1") || name.starts_with("1prush") || name.starts_with("spring1") ||
        name.starts_with("lf_sprin") || name.contains("spring_fl") || name.contains("spring_lf")) return 0;

    // Front Right (1)
    if (name.starts_with("prush2") || name.starts_with("2prush") || name.starts_with("spring2") ||
        name.starts_with("rf_sprin") || name.contains("spring_fr") || name.contains("spring_rf")) return 1;

    // Rear Left (2)
    if (name.starts_with("prush3") || name.starts_with("3prush") || name.starts_with("spring3") ||
        name.starts_with("lb_sprin") || name.starts_with("lr_sprin") || name.contains("spring_rl") || name.contains("spring_lr")) return 2;

    // Rear Right (3)
    if (name.starts_with("prush4") || name.starts_with("4prush") || name.starts_with("spring4") ||
        name.starts_with("rb_sprin") || name.starts_with("rr_sprin") || name.contains("spring_rr") || name.contains("spring_rb")) return 3;

    return -1;
}

void SuspensionSpringsFeature::Init()
{
    LOG_VERBOSE("Init {}", __FUNCTION__);
    ModelInfoMgr::RegisterDummy([](CVehicle *pVeh, RwFrame *pFrame, const std::string_view nodeName)
    {
        if (!pVeh || !pFrame) return;

        int idx = GetSpringWheelIndex(nodeName);
        if (idx >= 0) {
            SpringData &data = m_VehData.Get(pVeh);
            CaptureState(pVeh, pFrame);

            SpringEntry entry;
            entry.frame = pFrame;
            entry.wheelIndex = idx;
            entry.curScaleZ = 1.0f;

            data.springs.push_back(entry);
            data.bInitialized = true;
        }
    });

    ModelInfoMgr::RegisterRender([](CVehicle *pVeh)
    {
        if (!CBaseFeature::IsEnabled(eFeatureMatrix::SuspensionSprings)) return;
        if (!pVeh || !pVeh->GetIsOnScreen() || pVeh->m_fHealth <= 0.0f) return;
        if (pVeh->m_nVehicleSubClass != VEHICLE_AUTOMOBILE) return;

        SpringData &data = m_VehData.Get(pVeh);
        if (!data.bInitialized || data.springs.empty()) return;

        auto *autoVeh = static_cast<CAutomobile*>(pVeh);

        for (auto &spring : data.springs) {
            if (!spring.frame) continue;

            float comp = autoVeh->m_fWheelsSuspensionCompression[spring.wheelIndex];
            // Nominal compression is ~0.65f.
            // As compression increases, spring shortens (scale < 1.0f).
            // As wheel drops, spring extends (scale > 1.0f).
            float targetScaleZ = std::clamp(1.0f - (comp - 0.65f) * 0.8f, 0.4f, 1.6f);

            if (std::abs(targetScaleZ - spring.curScaleZ) > 0.001f && spring.curScaleZ > 0.001f) {
                float scaleRatio = targetScaleZ / spring.curScaleZ;
                RwV3d scale = { 1.0f, 1.0f, scaleRatio };
                RwMatrixScale(&spring.frame->modelling, &scale, rwCOMBINEPRECONCAT);
                RwFrameUpdateObjects(spring.frame);
                spring.curScaleZ = targetScaleZ;
            }
        }
    });
}

void SuspensionSpringsFeature::OnToggle(CVehicle *vehicle, bool enabled)
{
    if (!vehicle) return;
    SpringData &data = m_VehData.Get(vehicle);
    for (auto &spring : data.springs) {
        spring.curScaleZ = 1.0f;
    }
}
