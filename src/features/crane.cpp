#include "pch.h"
#include "crane.h"
#include "utils/adconfig.h"
#include "utils/inputmgr.h"
#include "utils/modelinfomgr.h"
#include <algorithm>
#include <cmath>

void MobileCraneFeature::Init()
{
    LOG_VERBOSE("Init {}", __FUNCTION__);
    ModelInfoMgr::RegisterDummy([](CVehicle *pVeh, RwFrame *pFrame, const std::string_view nodeName)
    {
        if (!pVeh || !pFrame) return;

        bool isCranePart = (nodeName == "osnova" || nodeName.starts_with("crane_base") || nodeName == "turntable" ||
                            nodeName.starts_with("strela") || nodeName.starts_with("crane_boom") || nodeName.starts_with("crane_arm") ||
                            nodeName.starts_with("opora") || nodeName.starts_with("outrigger") ||
                            nodeName == "hook_kr_dummy" || nodeName.starts_with("crane_hook") ||
                            nodeName == "kran_dummy" || nodeName == "crane_dummy");

        if (isCranePart) {
            CraneData &data = m_VehData.Get(pVeh);
            CaptureState(pVeh, pFrame);

            if (nodeName == "osnova" || nodeName.starts_with("crane_base") || nodeName == "turntable") {
                data.baseTurntable = pFrame;
            } else if (nodeName == "strela1" || nodeName == "strela" || nodeName.starts_with("crane_boom") || nodeName == "crane_arm1") {
                data.mainBoom = pFrame;
                const auto *ad = ADConfig::Get(pVeh->m_nModelIndex);
                if (ad && ad->craneBoomMul > 0.0f) {
                    data.maxBoomPitch = (ad->craneBoomMul <= 10.0f) ? (ad->craneBoomMul * 60.0f) : ad->craneBoomMul;
                }
            } else if (nodeName == "strela2" || nodeName == "strel" || nodeName.starts_with("crane_ext") || nodeName == "crane_arm2") {
                data.telescopicBoom = pFrame;
            } else if (nodeName == "opora_l" || nodeName.starts_with("outrigger_l")) {
                data.outriggerLeft = pFrame;
            } else if (nodeName == "opora_r" || nodeName.starts_with("outrigger_r")) {
                data.outriggerRight = pFrame;
            } else if (nodeName == "hook_kr_dummy" || nodeName.starts_with("crane_hook")) {
                data.hookFrame = pFrame;
            }
            data.bInitialized = true;
        }
    });

    ModelInfoMgr::RegisterRender([](CVehicle *pVeh)
    {
        if (!CBaseFeature::IsEnabled(eFeatureMatrix::MobileCrane)) return;
        if (!pVeh || !pVeh->GetIsOnScreen() || pVeh->m_fHealth <= 0.0f) return;
        if (pVeh != FindPlayerVehicle(-1, false)) return;

        CraneData &data = m_VehData.Get(pVeh);
        if (!data.bInitialized) return;

        const auto &keys = ADConfig::Keys();
        float dt = CTimer::ms_fTimeStep * 0.02f;

        // 1. Base turntable rotation (Num4 / Num6)
        if (data.baseTurntable) {
            float rotSpeed = 35.0f * dt;
            float rotDelta = 0.0f;
            if (InputMgr::IsKeyDown(keys.keyCraneRotateLeft)) rotDelta += rotSpeed;
            if (InputMgr::IsKeyDown(keys.keyCraneRotateRight)) rotDelta -= rotSpeed;
            if (std::abs(rotDelta) > 0.001f) {
                FrameUtil::SetRotationZ(data.baseTurntable, rotDelta);
                data.curBaseRot += rotDelta;
            }
        }

        // 2. Main boom pitch (Num8 / Num2)
        if (data.mainBoom) {
            float pitchSpeed = 20.0f * dt;
            float targetPitch = data.curBoomPitch;
            if (InputMgr::IsKeyDown(keys.keyCraneBoomUp)) targetPitch = std::min(data.maxBoomPitch, targetPitch + pitchSpeed);
            if (InputMgr::IsKeyDown(keys.keyCraneBoomDown)) targetPitch = std::max(0.0f, targetPitch - pitchSpeed);

            float delta = targetPitch - data.curBoomPitch;
            if (std::abs(delta) > 0.001f) {
                FrameUtil::SetRotationX(data.mainBoom, -delta);
                data.curBoomPitch = targetPitch;
            }
        }

        // 3. Telescopic boom extension (Num7 / Num9)
        if (data.telescopicBoom) {
            float extSpeed = 1.5f * dt;
            float targetPos = data.curTelescopicPos;
            if (InputMgr::IsKeyDown(keys.keyCraneBoomExtend)) targetPos = std::min(data.maxTelescopicDist, targetPos + extSpeed);
            if (InputMgr::IsKeyDown(keys.keyCraneBoomRetract)) targetPos = std::max(0.0f, targetPos - extSpeed);

            float delta = targetPos - data.curTelescopicPos;
            if (std::abs(delta) > 0.0001f) {
                data.telescopicBoom->modelling.pos.y += delta;
                RwFrameUpdateObjects(data.telescopicBoom);
                data.curTelescopicPos = targetPos;
            }
        }

        // 4. Outriggers (Num1 / Num3)
        float outSpeed = 0.6f * dt;
        float targetOutrigger = data.curOutriggerPos;
        if (InputMgr::IsKeyDown(keys.keyCraneOutriggersDeploy)) targetOutrigger = std::min(data.maxOutriggerDist, targetOutrigger + outSpeed);
        if (InputMgr::IsKeyDown(keys.keyCraneOutriggersRetract)) targetOutrigger = std::max(0.0f, targetOutrigger - outSpeed);

        float outDelta = targetOutrigger - data.curOutriggerPos;
        if (std::abs(outDelta) > 0.0001f) {
            if (data.outriggerLeft) {
                data.outriggerLeft->modelling.pos.x -= outDelta;
                data.outriggerLeft->modelling.pos.z -= outDelta * 0.4f;
                RwFrameUpdateObjects(data.outriggerLeft);
            }
            if (data.outriggerRight) {
                data.outriggerRight->modelling.pos.x += outDelta;
                data.outriggerRight->modelling.pos.z -= outDelta * 0.4f;
                RwFrameUpdateObjects(data.outriggerRight);
            }
            data.curOutriggerPos = targetOutrigger;
        }

        // 5. Hook hoist (Num+ / Num-)
        if (data.hookFrame) {
            float hookSpeed = 2.0f * dt;
            float targetHook = data.curHookPos;
            if (InputMgr::IsKeyDown(keys.keyCraneHookDown)) targetHook = std::min(data.maxHookDist, targetHook + hookSpeed);
            if (InputMgr::IsKeyDown(keys.keyCraneHookUp)) targetHook = std::max(0.0f, targetHook - hookSpeed);

            float delta = targetHook - data.curHookPos;
            if (std::abs(delta) > 0.0001f) {
                data.hookFrame->modelling.pos.z -= delta;
                RwFrameUpdateObjects(data.hookFrame);
                data.curHookPos = targetHook;
            }
        }
    });
}

void MobileCraneFeature::OnToggle(CVehicle *vehicle, bool enabled)
{
    if (!vehicle) return;
    CraneData &data = m_VehData.Get(vehicle);
    data.curBaseRot = 0.0f;
    data.curBoomPitch = 0.0f;
    data.curTelescopicPos = 0.0f;
    data.curOutriggerPos = 0.0f;
    data.curHookPos = 0.0f;
}
