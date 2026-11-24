#pragma once
#include <CVehicle.h>
#include <CAutomobile.h>
#include <CBike.h>
#include <RenderWare.h>
#include <cstdint>
#include <optional>
#include "enums/dummypos.h"

struct DummyConfig;

struct DoorDamageState {
    bool hasOk = false;
    bool hasDam = false;
    uint8_t prevStatus = 0xFF;
};

class CarUtil
{
public:
    static bool IsLightsForcedOff(CVehicle *pVeh);
    static bool IsLightsForcedOn(CVehicle *pVeh);
    static bool AreLightsOn(CVehicle *pVeh);

    static bool IsEngineOff(CVehicle *pVeh);
    static bool IsDoorDamaged(CVehicle *pVeh, eDoors door);
    static void InitDoorDamageState(CVehicle *pVeh, RwFrame *pFrame, eDoors doorID, DoorDamageState &state);
    static void SyncDoorDamageVisibility(CVehicle *pVeh, RwFrame *pFrame, eDoors doorID, DoorDamageState &state);
    static bool IsLightDamaged(CVehicle *pVeh, eLights light);
    static bool IsPanelDamaged(CVehicle *pVeh, ePanels panel);
    static bool IsFrameDamaged(CVehicle *pVeh, RwFrame *frame);
    static bool IsDummyDamaged(CVehicle *pVeh, const DummyConfig &c);
    static CVector UpdateRelativeToBoundingBox(CVehicle *pVeh, eDummyPos dummyPos, CVector center, CVector up, CVector right);

    static float GetVehiclePitch(CVehicle *pVeh);
    static bool IsVehicleDoingWheelie(CVehicle *pVeh);
    static float GetVehicleSpeed(CVehicle *pVeh);
    static float GetVehicleSpeedRealistic(CVehicle *vehicle);
};
