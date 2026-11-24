#include "pch.h"
#include "slidedoor.h"
#include "utils/datamgr.h"
#include "utils/modelinfomgr.h"

void SlideDoor::UpdateDoorGroup(CVehicle *pVeh, std::vector<SlideDoorConfig> &configs, eDoors doorID)
{
    if (configs.empty())
        return;

    float ratio = pVeh->GetDooorAngleOpenRatio(doorID);
    float sideMult = (doorID == DOOR_FRONT_LEFT || doorID == DOOR_REAR_LEFT) ? -1.0f : 1.0f;
    float popFactor = std::min(1.0f, ratio * 5.0f);

    for (auto &config : configs)
    {
        if (!config.frame)
            continue;

        CarUtil::SyncDoorDamageVisibility(pVeh, config.frame, doorID, config.damage);

        // Sliding movement (Y axis)
        config.frame->modelling.pos.y = config.origPos.y + (config.mul * ratio * -1.0f);

        // Pop-out movement (X axis)
        config.frame->modelling.pos.x = config.origPos.x + (popFactor * config.popOutAmount * sideMult);

        RwMatrixUpdate(&config.frame->modelling);
    }
}

void SlideDoor::Init()
{
    ModelInfoMgr::RegisterDummy([](CVehicle *pVeh, RwFrame *pFrame, const std::string_view nodeName)
    {
        bool isLF = nodeName.starts_with("dvan_l") || nodeName.starts_with("dmbus_l") || nodeName.starts_with("x_sd_lf");
        bool isRF = nodeName.starts_with("dvan_r") || nodeName.starts_with("dmbus_r") || nodeName.starts_with("x_sd_rf");
        bool isLR = nodeName.starts_with("x_sd_lr");
        bool isRR = nodeName.starts_with("x_sd_rr");

        if (!isLF && !isRF && !isLR && !isRR) return;
        if (!pVeh || !pFrame) return;
        CaptureState(pVeh, pFrame);

        auto& jsonData = DataMgr::Get(pVeh->m_nModelIndex);
        SlideDoorData& data = m_VehData.Get(pVeh);

        float mul = 1.0f;
        float popOutAmount = 0.15f;
        std::string name(nodeName);

        if (jsonData.contains("doors") && jsonData["doors"].contains(name)) {
            mul = jsonData["doors"][name].value("movmul", 1.0f);
            popOutAmount = jsonData["doors"][name].value("popout", 0.15f);
        }

        std::vector<SlideDoorConfig>* group = nullptr;
        eDoors doorID = eDoors::DOOR_FRONT_LEFT;
        if      (isLF) { group = &data.leftFront;  doorID = eDoors::DOOR_FRONT_LEFT; }
        else if (isRF) { group = &data.rightFront; doorID = eDoors::DOOR_FRONT_RIGHT; }
        else if (isLR) { group = &data.leftRear;   doorID = eDoors::DOOR_REAR_LEFT; }
        else if (isRR) { group = &data.rightRear;  doorID = eDoors::DOOR_REAR_RIGHT; }

        SlideDoorConfig cfg = { pFrame, pFrame->modelling.pos, mul, popOutAmount };
        CarUtil::InitDoorDamageState(pVeh, pFrame, doorID, cfg.damage);
        group->push_back(cfg);
    });

    ModelInfoMgr::RegisterRender([](CVehicle *pVeh)
                                 {
        if (!CBaseFeature::IsEnabled(eFeatureMatrix::AnimatedDoors)) return;
        if (!pVeh || !pVeh->GetIsOnScreen()) return;

        SlideDoorData& data = m_VehData.Get(pVeh);
        UpdateDoorGroup(pVeh, data.leftFront,  eDoors::DOOR_FRONT_LEFT);
        UpdateDoorGroup(pVeh, data.rightFront, eDoors::DOOR_FRONT_RIGHT);
        UpdateDoorGroup(pVeh, data.leftRear,   eDoors::DOOR_REAR_LEFT);
        UpdateDoorGroup(pVeh, data.rightRear,  eDoors::DOOR_REAR_RIGHT); });
}

void SlideDoor::OnToggle(CVehicle *vehicle, bool enabled) {
    if (enabled) return;
    auto &data = m_VehData.Get(vehicle);
    for (auto *group : {&data.leftFront, &data.rightFront, &data.leftRear, &data.rightRear})
        for (auto &e : *group) e.damage.prevStatus = 0xFF;
}