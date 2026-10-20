#include "pch.h"
#include "cardan.h"
#include "utils/adconfig.h"
#include "utils/modelinfomgr.h"
#include <CAutomobile.h>
#include <cmath>

static bool IsCardanNode(const std::string_view name)
{
    return (name.contains("kardan") || name.contains("karda") || name.contains("cardan"));
}

void CardanShaftFeature::Init()
{
    LOG_VERBOSE("Init {}", __FUNCTION__);
    ModelInfoMgr::RegisterDummy([](CVehicle *pVeh, RwFrame *pFrame, const std::string_view nodeName)
    {
        if (!pVeh || !pFrame) return;

        if (IsCardanNode(nodeName)) {
            CardanData &data = m_VehData.Get(pVeh);
            CaptureState(pVeh, pFrame);

            CardanJoint joint;
            joint.frame = pFrame;
            joint.isFrontShaft = (nodeName.starts_with("f") || nodeName.starts_with("1f") || nodeName.starts_with("2f") || nodeName.contains("front"));

            const auto *ad = ADConfig::Get(pVeh->m_nModelIndex);
            if (ad) {
                if (nodeName.starts_with("1") && ad->cardan1Angle > 0.0f) {
                    joint.maxTiltAngle = ad->cardan1Angle * 57.2957795f;
                } else if (nodeName.starts_with("2") && ad->cardan2Angle > 0.0f) {
                    joint.maxTiltAngle = ad->cardan2Angle * 57.2957795f;
                }
            }

            data.shafts.push_back(joint);
            data.bInitialized = true;
        }
    });

    ModelInfoMgr::RegisterRender([](CVehicle *pVeh)
    {
        if (!CBaseFeature::IsEnabled(eFeatureMatrix::CardanShaft)) return;
        if (!pVeh || !pVeh->GetIsOnScreen() || pVeh->m_fHealth <= 0.0f) return;
        if (pVeh->m_nVehicleSubClass != VEHICLE_AUTOMOBILE) return;

        CardanData &data = m_VehData.Get(pVeh);
        if (!data.bInitialized || data.shafts.empty()) return;

        auto *autoVeh = static_cast<CAutomobile*>(pVeh);

        float frontWheelRot = autoVeh->m_fWheelRotation[0];
        float rearWheelRot = autoVeh->m_fWheelRotation[2];
        float frontSusp = 0.5f * (autoVeh->m_fWheelsSuspensionCompression[0] + autoVeh->m_fWheelsSuspensionCompression[1]);
        float rearSusp = 0.5f * (autoVeh->m_fWheelsSuspensionCompression[2] + autoVeh->m_fWheelsSuspensionCompression[3]);

        for (auto &shaft : data.shafts) {
            if (!shaft.frame) continue;

            float curWheelRot = shaft.isFrontShaft ? frontWheelRot : rearWheelRot;
            float diff = curWheelRot - shaft.lastWheelRot;
            if (std::abs(diff) > 3.0f) diff = 0.0f; // Handle wrap around
            shaft.lastWheelRot = curWheelRot;

            // Spin proportional to final drive ratio (~3.5x wheel speed) around local Y
            float spinDelta = diff * 57.2957795f * 3.5f;
            FrameUtil::SetRotationY(shaft.frame, spinDelta);
            shaft.curSpinAngle += spinDelta;

            // Suspension pitch tilt relative to chassis (POSTCONCAT along parent X axis)
            if (shaft.maxTiltAngle > 0.001f) {
                float susp = shaft.isFrontShaft ? frontSusp : rearSusp;
                // Nominal rest compression in SA is around 0.65f
                float targetTilt = (susp - 0.65f) * shaft.maxTiltAngle;
                float tiltDelta = targetTilt - shaft.curTiltAngle;
                if (std::abs(tiltDelta) > 0.001f) {
                    RwFrameRotate(shaft.frame, (RwV3d *)0x008D2E00, (RwReal)tiltDelta, rwCOMBINEPOSTCONCAT);
                    RwFrameUpdateObjects(shaft.frame);
                    shaft.curTiltAngle = targetTilt;
                }
            }
        }
    });
}

void CardanShaftFeature::OnToggle(CVehicle *vehicle, bool enabled)
{
    if (!vehicle) return;
    CardanData &data = m_VehData.Get(vehicle);
    for (auto &shaft : data.shafts) {
        shaft.curTiltAngle = 0.0f;
        shaft.curSpinAngle = 0.0f;
    }
}
