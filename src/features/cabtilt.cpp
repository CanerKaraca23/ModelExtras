#include "pch.h"
#include "cabtilt.h"
#include "utils/adconfig.h"
#include "utils/inputmgr.h"
#include "utils/modelinfomgr.h"
#include <algorithm>
#include <cmath>

void CabTiltFeature::Init()
{
    LOG_VERBOSE("Init {}", __FUNCTION__);
    ModelInfoMgr::RegisterDummy([](CVehicle *pVeh, RwFrame *pFrame, const std::string_view nodeName)
    {
        if (!pVeh || !pFrame) return;

        if (nodeName == "kabina_dummy" || nodeName == "kabina" ||
            nodeName == "cab_dummy" || nodeName == "cab") {
            CabTiltData &data = m_VehData.Get(pVeh);
            CaptureState(pVeh, pFrame);
            data.cabFrame = pFrame;
            data.bInitialized = true;
        }
    });

    ModelInfoMgr::RegisterRender([](CVehicle *pVeh)
    {
        if (!CBaseFeature::IsEnabled(eFeatureMatrix::CabTilt)) return;
        if (!pVeh || !pVeh->GetIsOnScreen() || pVeh->m_fHealth <= 0.0f) return;

        CabTiltData &data = m_VehData.Get(pVeh);
        if (!data.bInitialized || !data.cabFrame) return;

        CPlayerPed *player = FindPlayerPed();
        if (!player) return;

        bool canControl = false;
        CVehicle *playerVeh = FindPlayerVehicle(-1, false);

        if (playerVeh == pVeh) {
            // Player is in truck: vehicle must be stationary
            if (pVeh->m_vecMoveSpeed.Magnitude() < 0.05f) {
                canControl = true;
            }
        } else if (!playerVeh) {
            // Player is on foot nearby
            CVector playerPos = player->GetPosition();
            CVector vehPos = pVeh->GetPosition();
            if ((playerPos - vehPos).Magnitude() < 5.0f) {
                canControl = true;
            }
        }

        if (canControl) {
            const auto &keys = ADConfig::Keys();
            float speed = 25.0f * CTimer::ms_fTimeStep * 0.02f;

            float targetAngle = data.curAngle;
            if (InputMgr::IsKeyDown(keys.keyCabTiltUp)) {
                targetAngle = std::min(data.maxAngle, targetAngle + speed);
            } else if (InputMgr::IsKeyDown(keys.keyCabTiltDown)) {
                targetAngle = std::max(0.0f, targetAngle - speed);
            }

            float delta = targetAngle - data.curAngle;
            if (std::abs(delta) > 0.001f) {
                // Tilts forward
                FrameUtil::SetRotationX(data.cabFrame, delta);
                data.curAngle = targetAngle;
            }
        }
    });
}

void CabTiltFeature::OnToggle(CVehicle *vehicle, bool enabled)
{
    if (!vehicle) return;
    CabTiltData &data = m_VehData.Get(vehicle);
    data.curAngle = 0.0f;
}
