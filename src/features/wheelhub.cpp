#include "pch.h"
#include "wheelhub.h"
#include "utils/modelinfomgr.h"
#include "utils/util.h"
#include "utils/frame.h"
#include <CAutomobile.h>
#include <CBike.h>

void WheelHub::Init()
{
    ModelInfoMgr::RegisterDummy([](CVehicle *pVeh, RwFrame *pFrame, const std::string_view name)
    {
        WheelHubData& data = m_VehData.Get(pVeh);
        
        if (name == "wheel_rf_dummy")      { data.m_pWRF = pFrame; }
        else if (name == "wheel_rm_dummy") { data.m_pWRM = pFrame; }
        else if (name == "wheel_rr_dummy" || name == "wheel_rb_dummy") { data.m_pWRR = pFrame; }
        else if (name == "wheel_lf_dummy") { data.m_pWLF = pFrame; }
        else if (name == "wheel_lm_dummy") { data.m_pWLM = pFrame; }
        else if (name == "wheel_lr_dummy" || name == "wheel_lb_dummy") { data.m_pWLR = pFrame; }

        else if (name == "hub_rf")         { data.m_pHRF = pFrame; }
        else if (name == "hub_rm")         { data.m_pHRM = pFrame; }
        else if (name == "hub_rr" || name == "hub_rb") { data.m_pHRR = pFrame; }
        else if (name == "hub_lf")         { data.m_pHLF = pFrame; }
        else if (name == "hub_lm")         { data.m_pHLM = pFrame; }
        else if (name == "hub_lr" || name == "hub_lb") { data.m_pHLR = pFrame; }
    });

    const bool legacyPosition = gConfig.ReadBoolean("WHEELS", "UseLegacyHubPosition", false);
    ModelInfoMgr::RegisterRender([legacyPosition](CVehicle *pVeh)
    {
        if (!CBaseFeature::IsEnabled(eFeatureMatrix::RotatingWheelHubs)) return;
        if (!pVeh || !pVeh->m_pRwClump || !pVeh->GetIsOnScreen()) {
            return;
        }

        WheelHubData& data = m_VehData.Get(pVeh);
        bool modified = false;
        RwFrame *root = RpClumpGetFrame(pVeh->m_pRwClump);
        RwFrame *nativeWheels[6]{};
        if (pVeh->m_nVehicleSubClass == VEHICLE_AUTOMOBILE || pVeh->m_nVehicleSubClass == VEHICLE_MTRUCK ||
            pVeh->m_nVehicleSubClass == VEHICLE_QUAD || pVeh->m_nVehicleSubClass == VEHICLE_PLANE) {
            auto *nodes = static_cast<CAutomobile *>(pVeh)->m_aCarNodes;
            std::copy_n(nodes + CAR_WHEEL_RF, 6, nativeWheels);
        } else if (pVeh->m_nVehicleSubClass == VEHICLE_BIKE || pVeh->m_nVehicleSubClass == VEHICLE_BMX) {
            auto *nodes = static_cast<CBike *>(pVeh)->m_aBikeNodes;
            nativeWheels[0] = nodes[BIKE_WHEEL_FRONT];
            nativeWheels[2] = nodes[BIKE_WHEEL_REAR];
        }
        
        // Thanks to Ameer & SanVive team for their rotation fix
        auto updateRotation = [&](RwFrame* ori, RwFrame* tar, bool isLeft, RwFrame *fallback)
        {
            if (!FrameUtil::ContainsFrame(root, tar)) return;
            if (!FrameUtil::ContainsFrame(root, ori)) ori = fallback;
            if (!FrameUtil::ContainsFrame(root, ori)) return;

            RwV3d rightVec = ori->modelling.right;
            if (isLeft) RwV3dNegate(&rightVec, &rightVec);

            MatrixUtil::ForceRightVector(&tar->modelling, rightVec);
            RwV3dNegate(&tar->modelling.up, &tar->modelling.up);

            if (legacyPosition) tar->modelling.pos = ori->modelling.pos;
            else tar->modelling.pos.z = ori->modelling.pos.z;

            pVeh->UpdateRwFrame();
        };

        updateRotation(data.m_pWRF, data.m_pHRF, false, nativeWheels[0]);
        updateRotation(data.m_pWRM, data.m_pHRM, false, nativeWheels[1]);
        updateRotation(data.m_pWRR, data.m_pHRR, false, nativeWheels[2]);
        updateRotation(data.m_pWLF, data.m_pHLF, true, nativeWheels[3]);
        updateRotation(data.m_pWLM, data.m_pHLM, true, nativeWheels[4]);
        updateRotation(data.m_pWLR, data.m_pHLR, true, nativeWheels[5]);

        if (modified) {
            pVeh->UpdateRwFrame();
        }
    });
}
