#include "pch.h"
#include "caliper.h"
#include "utils/modelinfomgr.h"
#include <CAutomobile.h>

static bool IsCaliperNode(const std::string_view name)
{
    if (name.contains("light")) return false;
    return (name.contains("tormoza") || name.contains("caliper") ||
            name.starts_with("brake_fl") || name.starts_with("brake_fr") ||
            name.starts_with("brake_rl") || name.starts_with("brake_rr") ||
            name.starts_with("f_caliper"));
}

void BrakeCaliperFeature::Init()
{
    LOG_VERBOSE("Init {}", __FUNCTION__);
    ModelInfoMgr::RegisterDummy([](CVehicle *pVeh, RwFrame *pFrame, const std::string_view nodeName)
    {
        if (!pVeh || !pFrame) return;

        if (IsCaliperNode(nodeName)) {
            CaliperData &data = m_VehData.Get(pVeh);
            CaptureState(pVeh, pFrame);

            CaliperEntry entry;
            entry.frame = pFrame;
            // 1tormoza (FL) and 2tormoza (FR) are front; 3tormoza (RL) and 4tormoza (RR) are rear
            entry.isRear = (nodeName.starts_with("3") || nodeName.starts_with("4") ||
                            nodeName.contains("_rl") || nodeName.contains("_rr") ||
                            nodeName.contains("rear") || nodeName.contains("_b"));

            data.calipers.push_back(entry);
            data.bInitialized = true;
        }
    });

    ModelInfoMgr::RegisterRender([](CVehicle *pVeh)
    {
        if (!CBaseFeature::IsEnabled(eFeatureMatrix::BrakeCaliper)) return;
        if (!pVeh || !pVeh->GetIsOnScreen() || pVeh->m_fHealth <= 0.0f) return;

        CaliperData &data = m_VehData.Get(pVeh);
        if (!data.bInitialized || data.calipers.empty()) return;

        float steerAngleDeg = (-pVeh->m_fSteerAngle) * 57.2957795f;

        for (auto &caliper : data.calipers) {
            if (!caliper.frame) continue;

            // Only front calipers steer with the wheel
            if (!caliper.isRear) {
                float delta = steerAngleDeg - caliper.curSteerAngle;
                FrameUtil::SetRotationZ(caliper.frame, delta);
                caliper.curSteerAngle = steerAngleDeg;
            }
        }
    });
}

void BrakeCaliperFeature::OnToggle(CVehicle *vehicle, bool enabled)
{
    if (!vehicle) return;
    CaliperData &data = m_VehData.Get(vehicle);
    for (auto &caliper : data.calipers) {
        caliper.curSteerAngle = 0.0f;
    }
}
