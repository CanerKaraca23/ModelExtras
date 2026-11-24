#include "pch.h"
#include "rotatedoor.h"
#include "utils/datamgr.h"
#include "utils/modelinfomgr.h"

void RotateDoor::UpdateSingleFrame(CVehicle* pVeh, RotateDoorConfig& config, eDoors doorID, bool isBootBonnet)
{
    if (!config.frame) return;

    CarUtil::SyncDoorDamageVisibility(pVeh, config.frame, doorID, config.damage);

    float ratio = pVeh->GetDooorAngleOpenRatio(doorID);
    float popFactor = std::min(1.0f, ratio * 5.0f);

    if (isBootBonnet) {
        // Boot/Bonnet Logic (Z-Translation, X-Rotation)
        config.frame->modelling.pos.z = config.origPos.z + (popFactor * config.popOutAmount);
        float targetRot = ratio * config.mul * 45.0f;
        MatrixUtil::SetRotationXAbsolute(&config.frame->modelling, targetRot - config.prevRot);
        config.prevRot = targetRot;
    } else {
        // Side Door Logic (X-Translation, Z-Rotation)
        float sideMult = (doorID == DOOR_FRONT_LEFT || doorID == DOOR_REAR_LEFT) ? 1.0f : -1.0f;
        config.frame->modelling.pos.x = config.origPos.x + (popFactor * config.popOutAmount * sideMult);
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
        if (!name.starts_with("x_rd_")) return;
        if (!pVeh || !pFrame) return;

        RotateDoorData& data = m_VehData.Get(pVeh);
        std::vector<RotateDoorConfig>* group = nullptr;
        eDoors doorID = DOOR_FRONT_LEFT;

        if      (name.starts_with("x_rd_lf"))     { group = &data.leftFront;  doorID = DOOR_FRONT_LEFT; }
        else if (name.starts_with("x_rd_rf"))     { group = &data.rightFront; doorID = DOOR_FRONT_RIGHT; }
        else if (name.starts_with("x_rd_lr"))     { group = &data.leftRear;   doorID = DOOR_REAR_LEFT; }
        else if (name.starts_with("x_rd_rr"))     { group = &data.rightRear;  doorID = DOOR_REAR_RIGHT; }
        else if (name.starts_with("x_rd_boot"))   { group = &data.boot;       doorID = BOOT; }
        else if (name.starts_with("x_rd_bonnet")) { group = &data.bonnet;     doorID = BONNET; }
        if (!group) return;

        CaptureState(pVeh, pFrame);
        auto& jsonData = DataMgr::Get(pVeh->m_nModelIndex);

        float mul = 1.0f;
        float popOutAmount = 0.0f;
        std::string nameStr(name);

        if (jsonData.contains("doors") && jsonData["doors"].contains(nameStr)) {
            mul = jsonData["doors"][nameStr].value("mul", 1.0f);
            popOutAmount = jsonData["doors"][nameStr].value("popout", 0.0f);
        }

        RotateDoorConfig cfg = { pFrame, pFrame->modelling.pos, mul, popOutAmount };
        CarUtil::InitDoorDamageState(pVeh, pFrame, doorID, cfg.damage);
        group->push_back(cfg);
    });

    ModelInfoMgr::RegisterRender([](CVehicle* pVeh)
    {
        if (!CBaseFeature::IsEnabled(eFeatureMatrix::AnimatedDoors)) return;
        if (!pVeh || !pVeh->GetIsOnScreen()) return;

        RotateDoorData& data = m_VehData.Get(pVeh);

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
        for (auto &e : *group) {
            e.prevRot = 0.0f;
            e.damage.prevStatus = 0xFF;
        }
}
