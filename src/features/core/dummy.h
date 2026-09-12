#pragma once
#include "utils/modelinfomgr.h"
#include <CGeneral.h>
#include "RenderWare.h"
#include "enums/dummypos.h"
#include "enums/lightingmode.h"
#include "dummyconfig.h"

class VehicleDummy
{
private:
    DummyConfig data;
    static inline const RwV3d s_axisZ = { 0.0f, 0.0f, 1.0f };

public:
    VehicleDummy() = default;
    VehicleDummy(const DummyConfig& config);
    VehicleDummy* operator->() { return this; }
    const VehicleDummy* operator->() const { return this; }

    const DummyConfig& GetRef() {
        return data;
    }

    DummyConfig& Get() {
        return data;
    }

    // Rotators
    void ResetAngle()
    {
        if (data.rotation.currentAngle != 0.0f)
        {
            ReduceAngle(data.rotation.currentAngle);
        }
    };

    void AddAngle(float angle)
    {
        if (angle != 0.0f)
        {
            RwFrameRotate(data.frame, &s_axisZ, angle, rwCOMBINEPRECONCAT);
            data.rotation.currentAngle += angle;
        }
    };

    void ReduceAngle(float angle)
    {
        if (angle != 0.0f)
        {
            RwFrameRotate(data.frame, &s_axisZ, -angle, rwCOMBINEPRECONCAT);
            data.rotation.currentAngle -= angle;
        }
    };

    void SetAngle(float angle)
    {
        ResetAngle();
        AddAngle(angle);
    }

    void Update();
};
