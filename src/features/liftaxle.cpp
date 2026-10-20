#include "pch.h"
#include "liftaxle.h"
#include "utils/adconfig.h"
#include "utils/inputmgr.h"
#include "utils/modelinfomgr.h"
#include <CAutomobile.h>
#include <algorithm>
#include <cmath>

void LiftAxleFeature::Init()
{
    LOG_VERBOSE("Init {}", __FUNCTION__);
    ModelInfoMgr::RegisterDummy([](CVehicle *pVeh, RwFrame *pFrame, const std::string_view nodeName)
    {
        if (!pVeh || !pFrame) return;

        if (nodeName == "leniv1" || nodeName == "leniv2" || nodeName.starts_with("liftaxle")) {
            LiftAxleData &data = m_VehData.Get(pVeh);
            CaptureState(pVeh, pFrame);

            LiftAxleEntry entry;
            entry.axleFrame = pFrame;

            // Collect child wheels or sibling wheels
            RwFrame *child = pFrame->child;
            while (child) {
                CaptureState(pVeh, child);
                entry.wheels.push_back(child);
                child = child->next;
            }

            data.axles.push_back(entry);
            data.bInitialized = true;
        } else if (nodeName == "rwheelleft" || nodeName == "rwheelright" ||
                   nodeName == "rwheelleft2" || nodeName == "rwheelright2") {
            // Sibling wheel attached to lazy axle
            LiftAxleData &data = m_VehData.Get(pVeh);
            CaptureState(pVeh, pFrame);
            if (!data.axles.empty()) {
                data.axles.back().wheels.push_back(pFrame);
            }
        }
    });

    ModelInfoMgr::RegisterRender([](CVehicle *pVeh)
    {
        if (!CBaseFeature::IsEnabled(eFeatureMatrix::LiftAxle)) return;
        if (!pVeh || !pVeh->GetIsOnScreen() || pVeh->m_fHealth <= 0.0f) return;

        LiftAxleData &data = m_VehData.Get(pVeh);
        if (!data.bInitialized || data.axles.empty()) return;

        // Player toggle
        if (pVeh == FindPlayerVehicle(-1, false)) {
            if (InputMgr::IsKeyJustDown(ADConfig::Keys().keyLiftAxle)) {
                data.bRaised = !data.bRaised;
            }
        }

        float dt = CTimer::ms_fTimeStep * 0.02f;
        float speed = 1.5f * dt;

        float curWheelRot = 0.0f;
        if (pVeh->m_nVehicleSubClass == VEHICLE_AUTOMOBILE) {
            curWheelRot = static_cast<CAutomobile*>(pVeh)->m_fWheelRotation[2];
        }

        for (auto &axle : data.axles) {
            float targetLift = data.bRaised ? 1.0f : 0.0f;
            float oldLift = axle.curLift;

            if (axle.curLift < targetLift) axle.curLift = std::min(targetLift, axle.curLift + speed);
            else if (axle.curLift > targetLift) axle.curLift = std::max(targetLift, axle.curLift - speed);

            float liftDelta = axle.curLift - oldLift;
            if (std::abs(liftDelta) > 0.0001f && axle.axleFrame) {
                axle.axleFrame->modelling.pos.z += liftDelta * axle.maxTravel;
                RwFrameUpdateObjects(axle.axleFrame);
            }

            // If lowered, wheels spin with road
            if (axle.curLift < 0.2f) {
                float diff = curWheelRot - axle.lastWheelRot;
                if (std::abs(diff) < 3.0f) {
                    for (RwFrame *w : axle.wheels) {
                        if (w) FrameUtil::SetRotationX(w, diff * 57.2957795f);
                    }
                }
            }
            axle.lastWheelRot = curWheelRot;
        }
    });
}

void LiftAxleFeature::OnToggle(CVehicle *vehicle, bool enabled)
{
    if (!vehicle) return;
    LiftAxleData &data = m_VehData.Get(vehicle);
    for (auto &axle : data.axles) {
        axle.curLift = 0.0f;
    }
    data.bRaised = false;
}
