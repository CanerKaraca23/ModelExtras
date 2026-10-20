#include "pch.h"
#include "rotatedoor.h"
#include "utils/adconfig.h"
#include "utils/datamgr.h"
#include "utils/inputmgr.h"
#include "utils/modelinfomgr.h"

void RotateDoor::UpdateSingleFrame(CVehicle* pVeh, RotateDoorConfig& config, eDoors doorID, bool isBootBonnet)
{
    if (!config.frame) return;

    float ratio = pVeh->GetDooorAngleOpenRatio(doorID);
    float popFactor = std::min(1.0f, ratio * 5.0f);

    if (isBootBonnet) {
        // Boot/Bonnet Logic (Z-Translation, X-Rotation)
        config.frame->modelling.pos.z = popFactor * config.popOutAmount;
        float targetRot = ratio * config.mul * 45.0f;
        MatrixUtil::SetRotationXAbsolute(&config.frame->modelling, targetRot - config.prevRot);
        config.prevRot = targetRot;
    } else {
        // Side Door Logic (X-Translation, Z-Rotation)
        float sideMult = (doorID == DOOR_FRONT_LEFT || doorID == DOOR_REAR_LEFT) ? 1.0f : -1.0f;
        config.frame->modelling.pos.x = popFactor * config.popOutAmount * sideMult;
        float targetRot = ratio * config.mul * 90.0f * sideMult;
        MatrixUtil::SetRotationZAbsolute(&config.frame->modelling, targetRot - config.prevRot);
        config.prevRot = targetRot;
    }

    RwMatrixUpdate(&config.frame->modelling);
}

void RotateDoor::UpdateDoorGroup(CVehicle* pVeh, std::vector<RotateDoorConfig>& configs, eDoors doorID, bool isBootBonnet)
{
    for (auto& config : configs) {
        UpdateSingleFrame(pVeh, config, doorID, isBootBonnet);
    }
}

void RotateDoor::Init()
{
    ModelInfoMgr::RegisterDummy([](CVehicle* pVeh, RwFrame* pFrame, const std::string_view name)
    {
        if (!name.starts_with("x_rd_") && !name.starts_with("doorbuda_")) return;
        if (!pVeh || !pFrame) return;

        // If dummy has child, use child frame
        if (name.ends_with("_dummy") && pFrame->child) {
            pFrame = pFrame->child;
        }

        CaptureState(pVeh, pFrame);

        auto& jsonData = DataMgr::Get(pVeh->m_nModelIndex);
        RotateDoorData& data = m_VehData.Get(pVeh);

        float mul = 1.0f;
        float popOutAmount = 0.0f;
        std::string nameStr(name);

        if (jsonData.contains("doors") && jsonData["doors"].contains(nameStr)) {
            mul = jsonData["doors"][nameStr].value("mul", 1.0f);
            popOutAmount = jsonData["doors"][nameStr].value("popout", 0.15f);
        }

        float orgRot = static_cast<float>(MatrixUtil::GetRotationZ(&pFrame->modelling));
        RotateDoorConfig cfg = { pFrame, orgRot, mul, popOutAmount };

        if (name.starts_with("x_rd_lf"))      data.leftFront.push_back(cfg);
        else if (name.starts_with("x_rd_rf")) data.rightFront.push_back(cfg);
        else if (name.starts_with("x_rd_lr") || name.starts_with("doorbuda_lr")) data.leftRear.push_back(cfg);
        else if (name.starts_with("x_rd_rr") || name.starts_with("doorbuda_rr")) data.rightRear.push_back(cfg);
        else if (name.starts_with("x_rd_boot"))   data.boot.push_back(cfg);
        else if (name.starts_with("x_rd_bonnet")) data.bonnet.push_back(cfg);
    });

    ModelInfoMgr::RegisterRender([](CVehicle* pVeh)
    {
        if (!CBaseFeature::IsEnabled(eFeatureMatrix::AnimatedDoors)) return;
        if (!pVeh || !pVeh->GetIsOnScreen()) return;

        RotateDoorData& data = m_VehData.Get(pVeh);

        // Player toggle key for cargo box doors
        if (!data.leftRear.empty() || !data.rightRear.empty()) {
            bool canToggle = false;
            CVehicle *playerVeh = FindPlayerVehicle(-1, false);
            if (playerVeh == pVeh) {
                canToggle = true;
            } else if (!playerVeh && FindPlayerPed()) {
                if ((FindPlayerPed()->GetPosition() - pVeh->GetPosition()).Magnitude() < 5.0f) {
                    canToggle = true;
                }
            }

            if (canToggle && InputMgr::IsKeyJustDown(ADConfig::Keys().keyCargoDoorsToggle)) {
                float curRatio = pVeh->GetDooorAngleOpenRatio(DOOR_REAR_LEFT);
                float target = (curRatio > 0.5f) ? 0.0f : 1.0f;
                pVeh->OpenDoor(nullptr, 0, DOOR_REAR_LEFT, target, true);
                pVeh->OpenDoor(nullptr, 0, DOOR_REAR_RIGHT, target, true);
            }
        }

        UpdateDoorGroup(pVeh, data.leftFront,  DOOR_FRONT_LEFT,  false);
        UpdateDoorGroup(pVeh, data.rightFront, DOOR_FRONT_RIGHT, false);
        UpdateDoorGroup(pVeh, data.leftRear,   DOOR_REAR_LEFT,   false);
        UpdateDoorGroup(pVeh, data.rightRear,  DOOR_REAR_RIGHT,  false);
        UpdateDoorGroup(pVeh, data.boot,       BOOT,             true);
        UpdateDoorGroup(pVeh, data.bonnet,     BONNET,           true);
    });
}

void RotateDoor::OnToggle(CVehicle *vehicle, bool enabled) {
    if (enabled) return;
    auto &data = m_VehData.Get(vehicle);
    for (auto *group : {&data.leftFront, &data.rightFront, &data.leftRear, &data.rightRear, &data.boot, &data.bonnet})
        for (auto &e : *group) e.prevRot = 0.0f;
}
