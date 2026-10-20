#pragma once
#include <plugin.h>
#include "core/base.h"

enum class eDriveMode {
    AWD,
    RWD,
    FWD,
    LowRange
};

struct DrivetrainData {
    unsigned char originalDriveType = '4';
    float originalAcceleration = 0.0f;
    float originalMaxVelocity = 0.0f;
    eDriveMode mode = eDriveMode::AWD;
    bool bInitialized = false;

    DrivetrainData(CVehicle *pVeh) {}
    ~DrivetrainData() {}
};

class DrivetrainTransferCaseFeature : public CVehFeature<DrivetrainData>
{
private:
    static inline tHandlingData *m_pModifiedHandling = nullptr;
    static inline CVehicle *m_pModifiedVeh = nullptr;
    static inline unsigned char m_SavedDriveType = '4';
    static inline float m_SavedAcceleration = 0.0f;
    static inline float m_SavedMaxVelocity = 0.0f;

public:
    static void RestoreHandling();

protected:
    void Init() override;
    void OnToggle(CVehicle *vehicle, bool enabled) override;

public:
    DrivetrainTransferCaseFeature() : CVehFeature<DrivetrainData>("DrivetrainTransferCase", "FEATURES", eFeatureMatrix::DrivetrainTransferCase) {}
};
