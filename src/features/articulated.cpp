#include "pch.h"
#include "articulated.h"
#include "utils/modelinfomgr.h"
#include <cmath>

static float NormalizeAngle(float angle)
{
    while (angle > 3.14159265f) angle -= 6.2831853f;
    while (angle < -3.14159265f) angle += 6.2831853f;
    return angle;
}

void ArticulatedVehicleFeature::Init()
{
    LOG_VERBOSE("Init {}", __FUNCTION__);
    ModelInfoMgr::RegisterDummy([](CVehicle *pVeh, RwFrame *pFrame, const std::string_view nodeName)
    {
        if (!pVeh || !pFrame) return;

        bool matched = false;
        ArticulatedData &data = m_VehData.Get(pVeh);

        if (nodeName == "povrama" || nodeName == "povrama_dummy" || nodeName.starts_with("turntable_disc")) {
            if (nodeName.ends_with("_dummy") && pFrame->child) pFrame = pFrame->child;
            CaptureState(pVeh, pFrame);
            data.turntableDisc = pFrame;
            matched = true;
        } else if (nodeName == "dozer_dummy" || nodeName.starts_with("pivot_joint")) {
            CaptureState(pVeh, pFrame);
            data.tractorPivot = pFrame;
            matched = true;
        } else if (nodeName.starts_with("twheel")) {
            CaptureState(pVeh, pFrame);
            data.trailerSteerWheels.push_back(pFrame);
            matched = true;
        }

        if (matched) {
            data.bInitialized = true;
        }
    });

    ModelInfoMgr::RegisterRender([](CVehicle *pVeh)
    {
        if (!CBaseFeature::IsEnabled(eFeatureMatrix::ArticulatedVehicle)) return;
        if (!pVeh || !pVeh->GetIsOnScreen() || pVeh->m_fHealth <= 0.0f) return;

        ArticulatedData &data = m_VehData.Get(pVeh);
        if (!data.bInitialized) return;

        // 1. Tractor pivot joint (K-700 / wheel loaders)
        if (data.tractorPivot) {
            float targetAngle = (-pVeh->m_fSteerAngle) * 57.2957795f * 0.8f;
            float delta = targetAngle - data.curPivotAngle;
            FrameUtil::SetRotationZ(data.tractorPivot, delta);
            data.curPivotAngle = targetAngle;
        }

        // 2. Articulated bus disc and trailer steer wheels
        if (pVeh->m_pTrailer) {
            float busHeading = pVeh->GetHeading();
            float trailerHeading = pVeh->m_pTrailer->GetHeading();
            float diff = NormalizeAngle(busHeading - trailerHeading) * 57.2957795f;

            if (data.turntableDisc) {
                float targetDiscAngle = diff * 0.5f;
                float deltaDisc = targetDiscAngle - data.curDiscAngle;
                FrameUtil::SetRotationZ(data.turntableDisc, deltaDisc);
                data.curDiscAngle = targetDiscAngle;
            }

            if (!data.trailerSteerWheels.empty()) {
                float targetTrailerAngle = diff * 0.4f;
                float deltaTrailer = targetTrailerAngle - data.curTrailerSteerAngle;
                for (RwFrame *twheel : data.trailerSteerWheels) {
                    if (twheel) FrameUtil::SetRotationZ(twheel, deltaTrailer);
                }
                data.curTrailerSteerAngle = targetTrailerAngle;
            }
        } else {
            // Smoothly straighten if trailer detached
            if (std::abs(data.curDiscAngle) > 0.001f && data.turntableDisc) {
                FrameUtil::SetRotationZ(data.turntableDisc, -data.curDiscAngle);
                data.curDiscAngle = 0.0f;
            }
            if (std::abs(data.curTrailerSteerAngle) > 0.001f && !data.trailerSteerWheels.empty()) {
                for (RwFrame *twheel : data.trailerSteerWheels) {
                    if (twheel) FrameUtil::SetRotationZ(twheel, -data.curTrailerSteerAngle);
                }
                data.curTrailerSteerAngle = 0.0f;
            }
        }
    });
}

void ArticulatedVehicleFeature::OnToggle(CVehicle *vehicle, bool enabled)
{
    if (!vehicle) return;
    ArticulatedData &data = m_VehData.Get(vehicle);
    data.curPivotAngle = 0.0f;
    data.curDiscAngle = 0.0f;
    data.curTrailerSteerAngle = 0.0f;
}
