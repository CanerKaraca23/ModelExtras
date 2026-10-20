#include "pch.h"
#include "tipper.h"
#include "utils/adconfig.h"
#include "utils/inputmgr.h"
#include "utils/modelinfomgr.h"
#include <algorithm>
#include <cmath>

static bool IsTipperBedNode(const std::string_view name)
{
    return (name == "kuzov" || name == "samosval" || name == "samosval_dummy" ||
            name.starts_with("tipper") || name.starts_with("dump_bed"));
}

static bool IsHydraulicNode(const std::string_view name)
{
    return (name.contains("hydrauli") || name.starts_with("gidro"));
}

void TipperHydraulicsFeature::Init()
{
    LOG_VERBOSE("Init {}", __FUNCTION__);
    ModelInfoMgr::RegisterDummy([](CVehicle *pVeh, RwFrame *pFrame, const std::string_view nodeName)
    {
        if (!pVeh || !pFrame) return;

        if (IsTipperBedNode(nodeName)) {
            TipperData &data = m_VehData.Get(pVeh);

            // If it's samosval_dummy / tipper_dummy and has a child kuzov, pick that
            if (nodeName.ends_with("_dummy") && pFrame->child) {
                pFrame = pFrame->child;
            }

            CaptureState(pVeh, pFrame);
            data.bedFrame = pFrame;

            float maxAng = 45.0f;
            const auto *ad = ADConfig::Get(pVeh->m_nModelIndex);
            if (ad && ad->tipperAngle > 0.0f) {
                // If value in vehsets is <= 5, treat as multiplier or ratio; if > 5, treat as degrees
                maxAng = (ad->tipperAngle <= 5.0f) ? (ad->tipperAngle * 45.0f) : ad->tipperAngle;
            }
            data.maxAngle = maxAng;
            data.bInitialized = true;
        } else if (IsHydraulicNode(nodeName)) {
            TipperData &data = m_VehData.Get(pVeh);
            CaptureState(pVeh, pFrame);
            data.hydraulicFrames.push_back(pFrame);
            data.bInitialized = true;
        }
    });

    ModelInfoMgr::RegisterRender([](CVehicle *pVeh)
    {
        if (!CBaseFeature::IsEnabled(eFeatureMatrix::TipperHydraulics)) return;
        if (!pVeh || !pVeh->GetIsOnScreen() || pVeh->m_fHealth <= 0.0f) return;

        TipperData &data = m_VehData.Get(pVeh);
        if (!data.bInitialized || !data.bedFrame) return;

        // Player vehicle or towed trailer of player vehicle
        bool isControlled = (pVeh == FindPlayerVehicle(-1, false));
        if (!isControlled && pVeh->m_nVehicleSubClass == VEHICLE_TRAILER) {
            CVehicle *playerVeh = FindPlayerVehicle(-1, false);
            if (playerVeh && playerVeh->m_pTrailer == pVeh) {
                isControlled = true;
            }
        }

        if (isControlled) {
            const auto &keys = ADConfig::Keys();
            float speed = 25.0f * CTimer::ms_fTimeStep * 0.02f; // deg/frame

            float targetAngle = data.curAngle;
            if (InputMgr::IsKeyDown(keys.keyTipperUp)) {
                targetAngle = std::min(data.maxAngle, targetAngle + speed);
            } else if (InputMgr::IsKeyDown(keys.keyTipperDown)) {
                targetAngle = std::max(0.0f, targetAngle - speed);
            }

            float delta = targetAngle - data.curAngle;
            if (std::abs(delta) > 0.001f) {
                // Negative X rotation tilts bed upward at rear pivot
                FrameUtil::SetRotationX(data.bedFrame, -delta);

                for (RwFrame *hyd : data.hydraulicFrames) {
                    if (hyd) {
                        // Hydraulic cylinder tilts halfway
                        FrameUtil::SetRotationX(hyd, -delta * 0.5f);
                    }
                }
                data.curAngle = targetAngle;
            }
        }
    });
}

void TipperHydraulicsFeature::OnToggle(CVehicle *vehicle, bool enabled)
{
    if (!vehicle) return;
    TipperData &data = m_VehData.Get(vehicle);
    data.curAngle = 0.0f;
}
