#include "pch.h"
#include "car.h"
#include "mathutil.h"
#include "frame.h"
#include "enums/lightoverride.h"
#include "features/core/dummyconfig.h"
#include "features/lights/data.h"
#include "util.h"
#include <CModelInfo.h>
#include <CVehicleModelInfo.h>
#include <CColModel.h>
#include <CBike.h>
#include <CAutomobile.h>
#include <CVisibilityPlugins.h>
#include <cmath>
#include <algorithm>

static constexpr bool HasAutomobileDamage(const CVehicle *pVeh) noexcept {
    if (!pVeh) return false;
    switch (pVeh->m_nVehicleSubClass) {
        case VEHICLE_AUTOMOBILE:
        case VEHICLE_MTRUCK:
        case VEHICLE_QUAD:
        case VEHICLE_HELI:
        case VEHICLE_PLANE:
        case VEHICLE_TRAILER:
            return true;
        default:
            return false;
    }
}

static constexpr int GetCarNodeIndexForDoor(eDoors doorID) noexcept {
    switch (doorID) {
        case eDoors::BONNET:           return eCarNodes::CAR_BONNET;
        case eDoors::BOOT:             return eCarNodes::CAR_BOOT;
        case eDoors::DOOR_FRONT_LEFT:  return eCarNodes::CAR_DOOR_LF;
        case eDoors::DOOR_FRONT_RIGHT: return eCarNodes::CAR_DOOR_RF;
        case eDoors::DOOR_REAR_LEFT:   return eCarNodes::CAR_DOOR_LR;
        case eDoors::DOOR_REAR_RIGHT:  return eCarNodes::CAR_DOOR_RR;
        default:                       return -1;
    }
}

static void ScanDoorDamageFlags(RwFrame *pFrame, DoorDamageState &state) {
    if (!pFrame) return;
    RwFrameForAllObjects(pFrame, [](RwObject *obj, void *data) -> RwObject * {
        if (RwObjectGetType(obj) == rpATOMIC) {
            auto flags = static_cast<uint16_t>(CVisibilityPlugins::GetUserValue(reinterpret_cast<RpAtomic *>(obj)));
            auto *s = static_cast<DoorDamageState *>(data);
            if (flags & 0x1) s->hasOk = true;  // ATOMIC_OK
            if (flags & 0x2) s->hasDam = true; // ATOMIC_DAMAGED
        }
        return obj;
    }, &state);
    for (RwFrame *child = pFrame->child; child; child = child->next) {
        ScanDoorDamageFlags(child, state);
    }
}

static void SetDoorVisibilityRecursive(CVehicle *pVeh, RwFrame *pFrame, uint32_t state) {
    if (!pFrame) return;
    pVeh->SetComponentVisibility(pFrame, state);
    for (RwFrame *child = pFrame->child; child; child = child->next) {
        SetDoorVisibilityRecursive(pVeh, child, state);
    }
}

bool CarUtil::IsLightsForcedOn(CVehicle *pVeh)
{
    return pVeh->m_nOverrideLights == eLightOverride::ForceLightsOn;
}

bool CarUtil::IsLightsForcedOff(CVehicle *pVeh)
{
    return CVehicle::ms_forceVehicleLightsOff || pVeh->m_nOverrideLights == eLightOverride::ForceLightsOff;
}

bool CarUtil::AreLightsOn(CVehicle *pVeh)
{
    if (!pVeh) return false;
    if (IsLightsForcedOff(pVeh)) return false;
    if (LightsConfig::Get().bLightsRequireEngine && IsEngineOff(pVeh)) {
        return false;
    }
    return pVeh->bLightsOn || IsLightsForcedOn(pVeh) || (Util::IsNightTime() && !IsEngineOff(pVeh));
}

bool CarUtil::IsEngineOff(CVehicle *pVeh) {
    return !pVeh->bEngineOn || pVeh->bEngineBroken;
}

float CarUtil::GetVehicleSpeed(CVehicle *pVeh)
{
    return pVeh->m_vecMoveSpeed.Magnitude2D() * 50.0f;
}

float CarUtil::GetVehicleSpeedRealistic(CVehicle *vehicle)
{
    float wheelSpeed = 0.0;
    CVehicleModelInfo *vehicleModelInfo = (CVehicleModelInfo *)GetSafeModelInfo(vehicle->m_nModelIndex);
    if (!vehicleModelInfo) return vehicle->m_vecMoveSpeed.Magnitude() * 50.0f;
    if (vehicle->m_nVehicleSubClass == VEHICLE_BIKE || vehicle->m_nVehicleSubClass == VEHICLE_BMX)
    {
        CBike *bike = (CBike *)vehicle;
        wheelSpeed = ((bike->m_aWheelAngularVelocity[0] * vehicleModelInfo->m_fWheelSizeFront) +
                      (bike->m_aWheelAngularVelocity[1] * vehicleModelInfo->m_fWheelSizeRear)) /
                     2.0f;
    }
    else if (vehicle->m_nVehicleSubClass == VEHICLE_AUTOMOBILE || vehicle->m_nVehicleSubClass == VEHICLE_MTRUCK || vehicle->m_nVehicleSubClass == VEHICLE_QUAD)
    {
        CAutomobile *automobile = (CAutomobile *)vehicle;
        wheelSpeed = ((automobile->m_fWheelSpeed[0] + automobile->m_fWheelSpeed[1] * vehicleModelInfo->m_fWheelSizeFront) +
                      (automobile->m_fWheelSpeed[2] + automobile->m_fWheelSpeed[3] * vehicleModelInfo->m_fWheelSizeRear)) /
                     4.0f;
    }
    else
    {
        return (CarUtil::GetVehicleSpeed(vehicle)) * 3.6f;
    }
    wheelSpeed /= 2.45f;
    wheelSpeed *= -186.0f;

    return wheelSpeed;
}

bool CarUtil::IsLightDamaged(CVehicle *pVeh, eLights light) {
    if (!pVeh || pVeh->m_nVehicleSubClass != VEHICLE_AUTOMOBILE) {
        return false;
    }
    CAutomobile *pAutoMobile = static_cast<CAutomobile*>(pVeh);
    if (!pAutoMobile) {
        return false;
    }

    return pAutoMobile->m_damageManager.GetLightStatus(light);
}

bool CarUtil::IsDoorDamaged(CVehicle *pVeh, eDoors door) {
    if (!pVeh || pVeh->m_nVehicleSubClass != VEHICLE_AUTOMOBILE) {
        return false;
    }
    CAutomobile *pAutoMobile = static_cast<CAutomobile*>(pVeh);
    if (!pAutoMobile) {
        return false;
    }

    return pAutoMobile->m_damageManager.GetDoorStatus(door);
}

void CarUtil::InitDoorDamageState(CVehicle *pVeh, RwFrame *pFrame, eDoors doorID, DoorDamageState &state) {
    state = {};
    if (!pVeh || !pFrame) return;

    ScanDoorDamageFlags(pFrame, state);
    if (!state.hasOk && !state.hasDam) return;

    if (pVeh->m_nVehicleSubClass != VEHICLE_AUTOMOBILE &&
        pVeh->m_nVehicleSubClass != VEHICLE_MTRUCK &&
        pVeh->m_nVehicleSubClass != VEHICLE_QUAD &&
        pVeh->m_nVehicleSubClass != VEHICLE_TRAILER) {
        return;
    }

    int nodeIdx = GetCarNodeIndexForDoor(doorID);
    if (nodeIdx < 0) return;

    // Moving _ok/_dam out of native door dummies prevents PreprocessHierarchy from marking the door damageable.
    if (auto *pInfo = static_cast<CVehicleModelInfo *>(GetSafeModelInfo(pVeh->m_nModelIndex))) {
        if (pInfo->m_pVehicleStruct) {
            pInfo->m_pVehicleStruct->m_nMaskComponentsDamagable |= (1u << nodeIdx);
        }
    }
}

void CarUtil::SyncDoorDamageVisibility(CVehicle *pVeh, RwFrame *pFrame, eDoors doorID, DoorDamageState &state) {
    if (!pVeh || !pFrame || (!state.hasOk && !state.hasDam) || !HasAutomobileDamage(pVeh)) return;

    uint8_t status = static_cast<uint8_t>(static_cast<CAutomobile *>(pVeh)->m_damageManager.GetDoorStatus(doorID));
    if (status == state.prevStatus) return;
    state.prevStatus = status;

    uint32_t targetVisibility = 0; // ATOMIC_NONE
    if (status == eDamageState::DAMSTATE_OK || status == eDamageState::DAMSTATE_OPENED) {
        targetVisibility = state.hasOk ? 1u : 0u; // ATOMIC_OK
    } else if (status == eDamageState::DAMSTATE_DAMAGED || status == eDamageState::DAMSTATE_OPENED_DAMAGED) {
        // Keep intact mesh visible on vehicles that move _ok without a _dam counterpart.
        targetVisibility = state.hasDam ? 2u : (state.hasOk ? 1u : 0u);
    }

    SetDoorVisibilityRecursive(pVeh, pFrame, targetVisibility);
}

bool CarUtil::IsPanelDamaged(CVehicle *pVeh, ePanels panel) {
    if (!pVeh || pVeh->m_nVehicleSubClass != VEHICLE_AUTOMOBILE) {
        return false;
    }
    CAutomobile *pAutoMobile = static_cast<CAutomobile*>(pVeh);
    if (!pAutoMobile) {
        return false;
    }

    return pAutoMobile->m_damageManager.GetPanelStatus(panel);
}

bool CarUtil::IsFrameDamaged(CVehicle *pVeh, RwFrame *frame) {
    if (!pVeh || !frame || pVeh->m_nVehicleSubClass != VEHICLE_AUTOMOBILE) {
        return false;
    }

    RwFrame *current = frame;
    while (current) {
        const char *name = GetFrameNodeName(current);
        if (name) {
            std::string_view sName(name);
            if (sName.starts_with("bump_front") || sName.starts_with("bump_f")) {
                return CarUtil::IsPanelDamaged(pVeh, ePanels::BUMP_FRONT);
            }
            if (sName.starts_with("bump_rear") || sName.starts_with("bump_r")) {
                return CarUtil::IsPanelDamaged(pVeh, ePanels::BUMP_REAR);
            }
            if (sName.starts_with("wing_lf")) {
                return CarUtil::IsPanelDamaged(pVeh, ePanels::WING_FRONT_LEFT);
            }
            if (sName.starts_with("wing_rf")) {
                return CarUtil::IsPanelDamaged(pVeh, ePanels::WING_FRONT_RIGHT);
            }
            if (sName.starts_with("wing_lr")) {
                return CarUtil::IsPanelDamaged(pVeh, ePanels::WING_REAR_LEFT);
            }
            if (sName.starts_with("wing_rr")) {
                return CarUtil::IsPanelDamaged(pVeh, ePanels::WING_REAR_RIGHT);
            }
            if (sName.starts_with("bonnet")) {
                return CarUtil::IsDoorDamaged(pVeh, eDoors::BONNET);
            }
            if (sName.starts_with("boot")) {
                return CarUtil::IsDoorDamaged(pVeh, eDoors::BOOT);
            }
            if (sName.starts_with("windscreen")) {
                return CarUtil::IsPanelDamaged(pVeh, ePanels::WINDSCREEN);
            }
            if (sName.starts_with("door_lf")) {
                return CarUtil::IsDoorDamaged(pVeh, eDoors::DOOR_FRONT_LEFT);
            }
            if (sName.starts_with("door_rf")) {
                return CarUtil::IsDoorDamaged(pVeh, eDoors::DOOR_FRONT_RIGHT);
            }
            if (sName.starts_with("door_lr")) {
                return CarUtil::IsDoorDamaged(pVeh, eDoors::DOOR_REAR_LEFT);
            }
            if (sName.starts_with("door_rr")) {
                return CarUtil::IsDoorDamaged(pVeh, eDoors::DOOR_REAR_RIGHT);
            }
            if (sName.starts_with("chassis") || sName == "Root") {
                break;
            }
        }
        current = RwFrameGetParent(current);
    }
    return false;
}

bool CarUtil::IsDummyDamaged(CVehicle *pVeh, const DummyConfig &c) {
    if (!pVeh || pVeh->m_nVehicleSubClass != VEHICLE_AUTOMOBILE) {
        return false;
    }
    if (c.damagePanel != -1) {
        return IsPanelDamaged(pVeh, static_cast<ePanels>(c.damagePanel));
    }
    if (c.damageDoor != -1) {
        return IsDoorDamaged(pVeh, static_cast<eDoors>(c.damageDoor));
    }
    return false;
}

float CarUtil::GetVehiclePitch(CVehicle *pVeh) {
    if (!pVeh || !pVeh->m_matrix) {
        return 0.0f;
    }

    CVector forward = pVeh->m_matrix->at;
    forward.Normalize();

    float pitchRad = asinf(forward.y);
    return pitchRad * 57.2957795f;
}

bool CarUtil::IsVehicleDoingWheelie(CVehicle *pVeh) {
    return pVeh && pVeh->m_nVehicleSubClass == VEHICLE_BIKE && CarUtil::GetVehiclePitch(pVeh) > 45.0f;
}

static float mx1, my1, mz1;
static float mx2, my2, mz2;

CVector CarUtil::UpdateRelativeToBoundingBox(CVehicle *pVeh, eDummyPos dummyPos, CVector shdwPos, CVector up, CVector right) {
    CVehicleModelInfo* pInfo = (CVehicleModelInfo*)GetSafeModelInfo(pVeh->m_nModelIndex);
    if (!pInfo || !pInfo->m_pColModel) return shdwPos;
    CVector min = pInfo->m_pColModel->m_boundBox.m_vecMin + CVector(mx1, my1, mz1);
    CVector max = pInfo->m_pColModel->m_boundBox.m_vecMax + CVector(mx2, my2, mz2);

    shdwPos += (min + max) * 0.5f;
    CVector corner1 = shdwPos + up + right;
    CVector corner2 = shdwPos + up - right;
    CVector corner3 = shdwPos - up + right;
    CVector corner4 = shdwPos - up - right;

    CVector minVec;
    minVec.x = std::min({ corner1.x, corner2.x, corner3.x, corner4.x });
    minVec.y = std::min({ corner1.y, corner2.y, corner3.y, corner4.y });
    minVec.z = std::min({ corner1.z, corner2.z, corner3.z, corner4.z });

    CVector maxVec;
    maxVec.x = std::max({ corner1.x, corner2.x, corner3.x, corner4.x });
    maxVec.y = std::max({ corner1.y, corner2.y, corner3.y, corner4.y });
    maxVec.z = std::max({ corner1.z, corner2.z, corner3.z, corner4.z });

    if (dummyPos == eDummyPos::Front) {
        minVec.y = std::max(minVec.y, max.y);
    }
    else if (dummyPos == eDummyPos::Rear) {
        maxVec.y = std::min(maxVec.y, min.y);
    } 

    shdwPos = (maxVec + minVec) * 0.5f;
    shdwPos -= (min + max) * 0.5f;
    return shdwPos;
}
