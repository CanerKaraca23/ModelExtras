#include "pch.h"
#include "powerwindows.h"
#include "utils/adconfig.h"
#include "utils/inputmgr.h"
#include "utils/modelinfomgr.h"
#include <algorithm>

void PowerWindowsFeature::Init()
{
    LOG_VERBOSE("Init {}", __FUNCTION__);
    ModelInfoMgr::RegisterDummy([](CVehicle *pVeh, RwFrame *pFrame, const std::string_view nodeName)
    {
        if (!pVeh || !pFrame) return;

        bool isLeft = (nodeName.contains("lf_glass") || nodeName.contains("glass_lf") ||
                       nodeName.contains("steklo_lf") || nodeName.contains("window_lf"));
        bool isRight = (nodeName.contains("rf_glass") || nodeName.contains("glass_rf") ||
                        nodeName.contains("steklo_rf") || nodeName.contains("window_rf"));

        if (isLeft || isRight) {
            PowerWindowsData &data = m_VehData.Get(pVeh);

            // If dummy has a child glass frame, use that
            if (pFrame->child) {
                pFrame = pFrame->child;
            }

            CaptureState(pVeh, pFrame);

            if (isLeft) {
                data.leftWindow.frame = pFrame;
                data.leftWindow.curOffset = 0.0f;
            } else {
                data.rightWindow.frame = pFrame;
                data.rightWindow.curOffset = 0.0f;
            }
            data.bInitialized = true;
        }
    });

    ModelInfoMgr::RegisterRender([](CVehicle *pVeh)
    {
        if (!CBaseFeature::IsEnabled(eFeatureMatrix::PowerWindows)) return;
        if (!pVeh || !pVeh->GetIsOnScreen() || pVeh->m_fHealth <= 0.0f) return;

        PowerWindowsData &data = m_VehData.Get(pVeh);
        if (!data.bInitialized) return;

        if (pVeh == FindPlayerVehicle(-1, false)) {
            const auto &keys = ADConfig::Keys();
            float moveSpeed = 0.6f * CTimer::ms_fTimeStep * 0.02f;

            // Left window controls
            if (data.leftWindow.frame) {
                float targetOffset = data.leftWindow.curOffset;
                if (InputMgr::IsKeyDown(keys.keyWindowLeftDown)) {
                    targetOffset = std::min(1.0f, targetOffset + moveSpeed);
                } else if (InputMgr::IsKeyDown(keys.keyWindowLeftUp)) {
                    targetOffset = std::max(0.0f, targetOffset - moveSpeed);
                }

                float delta = targetOffset - data.leftWindow.curOffset;
                if (std::abs(delta) > 0.0001f) {
                    data.leftWindow.frame->modelling.pos.z -= delta * data.leftWindow.maxDrop;
                    RwFrameUpdateObjects(data.leftWindow.frame);
                    data.leftWindow.curOffset = targetOffset;
                }
            }

            // Right window controls
            if (data.rightWindow.frame) {
                float targetOffset = data.rightWindow.curOffset;
                if (InputMgr::IsKeyDown(keys.keyWindowRightDown)) {
                    targetOffset = std::min(1.0f, targetOffset + moveSpeed);
                } else if (InputMgr::IsKeyDown(keys.keyWindowRightUp)) {
                    targetOffset = std::max(0.0f, targetOffset - moveSpeed);
                }

                float delta = targetOffset - data.rightWindow.curOffset;
                if (std::abs(delta) > 0.0001f) {
                    data.rightWindow.frame->modelling.pos.z -= delta * data.rightWindow.maxDrop;
                    RwFrameUpdateObjects(data.rightWindow.frame);
                    data.rightWindow.curOffset = targetOffset;
                }
            }
        }
    });
}

void PowerWindowsFeature::OnToggle(CVehicle *vehicle, bool enabled)
{
    if (!vehicle) return;
    PowerWindowsData &data = m_VehData.Get(vehicle);
    data.leftWindow.curOffset = 0.0f;
    data.rightWindow.curOffset = 0.0f;
}
