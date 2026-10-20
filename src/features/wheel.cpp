#include "pch.h"
#include "wheel.h"
#include "utils/adconfig.h"
#include "utils/modelinfomgr.h"
#include <string_view>
#include <CAutomobile.h>

void UpdateWheelRotation(CVehicle *pVeh, RwFrame *ori, RwFrame *tar)
{
    if (ori && tar)
    {
        MatrixUtil::SetRotationXAbsolute(&tar->modelling, MatrixUtil::GetRotationX(&ori->modelling) - MatrixUtil::GetRotationX(&tar->modelling));
        pVeh->UpdateRwFrame();
    }
}

static const std::unordered_map<std::string_view, eWheelPos> originalMap = {
    { "wheel_rf_dummy", eWheelPos::RightFront },
    { "wheel_rm_dummy", eWheelPos::RightMiddle },
    { "wheel_rr_dummy", eWheelPos::RightRear },
    { "wheel_rb_dummy", eWheelPos::RightRear },
    { "wheel_lf_dummy", eWheelPos::LeftFront },
    { "wheel_lm_dummy", eWheelPos::LeftMiddle },
    { "wheel_lr_dummy", eWheelPos::LeftRear },
    { "wheel_lb_dummy", eWheelPos::LeftRear },
};

static const std::unordered_map<std::string_view, eWheelPos> extraMap = {
    { "x_wheel_lf", eWheelPos::LeftFront },
    { "x_wheel_lm", eWheelPos::LeftMiddle },
    { "x_wheel_lr", eWheelPos::LeftRear },
    { "x_wheel_rf", eWheelPos::RightFront },
    { "x_wheel_rm", eWheelPos::RightMiddle },
    { "x_wheel_rr", eWheelPos::RightRear },
};

void ExtraWheel::Init()
{
    ModelInfoMgr::RegisterDummy([](CVehicle* pVeh, RwFrame* pFrame, const std::string_view name) {
        ExtraWheelData& data = m_VehData.Get(pVeh);

        auto itOrig = originalMap.find(name);
        if (itOrig != originalMap.end()) {
            data.pOriginals[(int)itOrig->second].push_back(pFrame);
            return;
        }

        // Active Dashboard front steered extra axle: wheellef, wheelrig, dwheelleft, dwheelright, rotwheel, wheel_rf_dummy2, wheel_lf_dummy2
        if (name == "wheellef" || name == "wheelrig" || name == "dwheelleft" || name == "dwheelright" || name == "rotwheel" ||
            name == "wheel_rf_dummy2" || name == "wheel_lf_dummy2" || name == "wheel_rm_dummy2" || name == "wheel_lm_dummy2") {
            CaptureState(pVeh, pFrame);
            data.pFrontSteerWheels.push_back(pFrame);

            const auto *ad = ADConfig::Get(pVeh->m_nModelIndex);
            if (ad && ad->frontSteerRatio != 0.0f) {
                data.frontSteerRatio = ad->frontSteerRatio;
            }
            return;
        }

        // Active Dashboard rear steered extra axle: rwheellef, rwheelrig, wheel_rr_dummy2, wheel_lr_dummy2
        if (name == "rwheellef" || name == "rwheelrig" || name == "wheel_rr_dummy2" || name == "wheel_lr_dummy2") {
            CaptureState(pVeh, pFrame);
            data.pRearSteerWheels.push_back(pFrame);

            const auto *ad = ADConfig::Get(pVeh->m_nModelIndex);
            if (ad && ad->rearSteerRatio != 0.0f) {
                data.rearSteerRatio = ad->rearSteerRatio;
            }
            return;
        }

        if (!name.starts_with("x_wheel")) {
            return;
        }

        std::string_view prefix = name.substr(0, 10);
        auto itExtra = extraMap.find(prefix);
        if (itExtra != extraMap.end()) {
            CaptureState(pVeh, pFrame);
            data.pExtras[(int)itExtra->second].push_back(pFrame);
        }
    });

    ModelInfoMgr::RegisterRender([](CVehicle *pVeh)
    {
        if (!CBaseFeature::IsEnabled(eFeatureMatrix::ExtraWheels)) return;
        if (!pVeh || !pVeh->GetIsOnScreen()) return;

        ExtraWheelData &data = m_VehData.Get(pVeh);

        // Standard middle/extra wheels
        for (int i = 0; i < static_cast<int>(eWheelPos::COUNT); i++)
        {
            if (data.pOriginals[i].empty()) continue;
            for (size_t j = 0; j < data.pExtras[i].size(); j++) {
                RwFrame *pOrig = (j < data.pOriginals[i].size()) ? data.pOriginals[i][j] : data.pOriginals[i][0];
                UpdateWheelRotation(pVeh, pOrig, data.pExtras[i][j]);
            }
        }

        // Multi-axle steering
        float steerAngleDeg = (-pVeh->m_fSteerAngle) * 57.2957795f;

        // 1. Front extra steering axle
        if (!data.pFrontSteerWheels.empty()) {
            float targetFront = steerAngleDeg * data.frontSteerRatio;
            float deltaFront = targetFront - data.curFrontSteer;
            for (RwFrame *w : data.pFrontSteerWheels) {
                if (w) FrameUtil::SetRotationZ(w, deltaFront);
            }
            data.curFrontSteer = targetFront;
        }

        // 2. Rear steering axle
        if (!data.pRearSteerWheels.empty()) {
            float targetRear = steerAngleDeg * data.rearSteerRatio;
            float deltaRear = targetRear - data.curRearSteer;
            for (RwFrame *w : data.pRearSteerWheels) {
                if (w) FrameUtil::SetRotationZ(w, deltaRear);
            }
            data.curRearSteer = targetRear;
        }
    });
}

void ExtraWheel::OnToggle(CVehicle *vehicle, bool enabled)
{
    if (!vehicle) return;
    ExtraWheelData &data = m_VehData.Get(vehicle);
    data.curFrontSteer = 0.0f;
    data.curRearSteer = 0.0f;
}
