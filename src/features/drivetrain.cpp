#include "pch.h"
#include "drivetrain.h"
#include "utils/adconfig.h"
#include "utils/inputmgr.h"
#include "utils/util.h"
#include <CMessages.h>

void DrivetrainTransferCaseFeature::RestoreHandling()
{
    if (m_pModifiedHandling) {
        m_pModifiedHandling->m_transmissionData.m_nDriveType = m_SavedDriveType;
        m_pModifiedHandling->m_transmissionData.m_fEngineAcceleration = m_SavedAcceleration;
        m_pModifiedHandling->m_transmissionData.m_fMaxGearVelocity = m_SavedMaxVelocity;
        m_pModifiedHandling = nullptr;
        m_pModifiedVeh = nullptr;
    }
}

void DrivetrainTransferCaseFeature::Init()
{
    LOG_VERBOSE("Init {}", __FUNCTION__);
    Events::processScriptsEvent += []()
    {
        if (!CBaseFeature::IsEnabled(eFeatureMatrix::DrivetrainTransferCase)) {
            RestoreHandling();
            return;
        }

        CVehicle *pVeh = FindPlayerVehicle(-1, false);
        if (!pVeh || !pVeh->m_pHandlingData || pVeh->m_nVehicleSubClass != VEHICLE_AUTOMOBILE) {
            RestoreHandling();
            return;
        }

        // If player switched to a different car, restore the old car's handling
        if (m_pModifiedVeh && m_pModifiedVeh != pVeh) {
            RestoreHandling();
        }

        DrivetrainData &data = m_VehData.Get(pVeh);
        if (!data.bInitialized) {
            data.originalDriveType = pVeh->m_pHandlingData->m_transmissionData.m_nDriveType;
            data.originalAcceleration = pVeh->m_pHandlingData->m_transmissionData.m_fEngineAcceleration;
            data.originalMaxVelocity = pVeh->m_pHandlingData->m_transmissionData.m_fMaxGearVelocity;
            data.bInitialized = true;
        }

        const auto &keys = ADConfig::Keys();
        float speedKmh = Util::GetVehicleSpeedRealistic(pVeh);

        eDriveMode newMode = data.mode;
        bool changed = false;

        if (InputMgr::IsKeyJustDown(keys.keyDriveFWD)) {
            newMode = eDriveMode::FWD;
            changed = true;
        } else if (InputMgr::IsKeyJustDown(keys.keyDriveAWD)) {
            newMode = eDriveMode::AWD;
            changed = true;
        } else if (InputMgr::IsKeyJustDown(keys.keyDriveRWD)) {
            newMode = eDriveMode::RWD;
            changed = true;
        } else if (InputMgr::IsKeyJustDown(keys.keyLowRange)) {
            if (speedKmh <= 10.0f) {
                newMode = (data.mode == eDriveMode::LowRange) ? eDriveMode::AWD : eDriveMode::LowRange;
                changed = true;
            } else {
                static char msg[] = "~r~Reduce speed below 10 km/h to shift 4x4 Low";
                CMessages::AddMessageWithString(msg, 2000, false, nullptr, true);
            }
        }

        if (changed && newMode != data.mode) {
            // Save original handling state before any modification
            if (!m_pModifiedHandling) {
                m_pModifiedHandling = pVeh->m_pHandlingData;
                m_pModifiedVeh = pVeh;
                m_SavedDriveType = data.originalDriveType;
                m_SavedAcceleration = data.originalAcceleration;
                m_SavedMaxVelocity = data.originalMaxVelocity;
            }

            data.mode = newMode;
            switch (data.mode) {
                case eDriveMode::FWD:
                    pVeh->m_pHandlingData->m_transmissionData.m_nDriveType = 'F';
                    pVeh->m_pHandlingData->m_transmissionData.m_fEngineAcceleration = data.originalAcceleration;
                    pVeh->m_pHandlingData->m_transmissionData.m_fMaxGearVelocity = data.originalMaxVelocity;
                    {
                        static char msg[] = "~y~Drivetrain: ~w~FWD (Front-Wheel Drive)";
                        CMessages::AddMessageWithString(msg, 1500, false, nullptr, true);
                    }
                    break;
                case eDriveMode::RWD:
                    pVeh->m_pHandlingData->m_transmissionData.m_nDriveType = 'R';
                    pVeh->m_pHandlingData->m_transmissionData.m_fEngineAcceleration = data.originalAcceleration;
                    pVeh->m_pHandlingData->m_transmissionData.m_fMaxGearVelocity = data.originalMaxVelocity;
                    {
                        static char msg[] = "~y~Drivetrain: ~w~RWD (Rear-Wheel Drive)";
                        CMessages::AddMessageWithString(msg, 1500, false, nullptr, true);
                    }
                    break;
                case eDriveMode::AWD:
                    pVeh->m_pHandlingData->m_transmissionData.m_nDriveType = '4';
                    pVeh->m_pHandlingData->m_transmissionData.m_fEngineAcceleration = data.originalAcceleration;
                    pVeh->m_pHandlingData->m_transmissionData.m_fMaxGearVelocity = data.originalMaxVelocity;
                    {
                        static char msg[] = "~g~Drivetrain: ~w~4WD High (All-Wheel Drive)";
                        CMessages::AddMessageWithString(msg, 1500, false, nullptr, true);
                    }
                    break;
                case eDriveMode::LowRange:
                    pVeh->m_pHandlingData->m_transmissionData.m_nDriveType = '4';
                    pVeh->m_pHandlingData->m_transmissionData.m_fEngineAcceleration = data.originalAcceleration * 2.2f;
                    pVeh->m_pHandlingData->m_transmissionData.m_fMaxGearVelocity = data.originalMaxVelocity * 0.45f;
                    {
                        static char msg[] = "~g~Drivetrain: ~w~4WD Low (Crawler Gear)";
                        CMessages::AddMessageWithString(msg, 1500, false, nullptr, true);
                    }
                    break;
            }
        }
    };
}

void DrivetrainTransferCaseFeature::OnToggle(CVehicle *vehicle, bool enabled)
{
    RestoreHandling();
    if (!vehicle || !vehicle->m_pHandlingData) return;
    DrivetrainData &data = m_VehData.Get(vehicle);
    if (data.bInitialized) {
        vehicle->m_pHandlingData->m_transmissionData.m_nDriveType = data.originalDriveType;
        vehicle->m_pHandlingData->m_transmissionData.m_fEngineAcceleration = data.originalAcceleration;
        vehicle->m_pHandlingData->m_transmissionData.m_fMaxGearVelocity = data.originalMaxVelocity;
    }
}
