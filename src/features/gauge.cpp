#include "pch.h"
#include "gauge.h"
#include "utils/adconfig.h"
#include "utils/datamgr.h"
#include <CBike.h>
#include "utils/modelinfomgr.h"
#include <algorithm>
#include <cmath>

using namespace plugin;

static inline float ClampRotation(float value, float maxRot)
{
    float limit = std::abs(maxRot);
    return Clamp(value, -limit, limit);
}

void GearIndicator::Init()
{
    ModelInfoMgr::RegisterDummy([](CVehicle *pVeh, RwFrame *pFrame, const std::string_view nodeName)
    {
        if (nodeName.starts_with("x_gearmeter") || nodeName.starts_with("fc_gm")) {
            VehGearData &data = m_VehData.Get(pVeh);

            GearIndicatorData indData;
            CaptureState(pVeh, pFrame, false, 1);
            indData.pRoot = pFrame;
            FrameUtil::StoreChilds(pFrame, indData.vecFrameList);
            data.vecIndicatorData.push_back(std::move(indData));
        }
    });

    ModelInfoMgr::RegisterRender([](CVehicle *pVeh)
    {
        if (!CBaseFeature::IsEnabled(eFeatureMatrix::AnimatedGearMeter)) return;
        if (!pVeh || !pVeh->GetIsOnScreen() || pVeh->m_fHealth <= 0.0f) return;

        VehGearData &data = m_VehData.Get(pVeh);

        for (auto&e : data.vecIndicatorData) {
            if (!e.vecFrameList.empty() &&  pVeh->m_nCurrentGear != e.iCurrent) {
                FrameUtil::HideAllChilds(e.pRoot);
                if (static_cast<size_t>(pVeh->m_nCurrentGear) < e.vecFrameList.size())
                {
                    FrameUtil::ShowAllAtomics(e.vecFrameList[pVeh->m_nCurrentGear]);
                }
                e.iCurrent = pVeh->m_nCurrentGear;
            }
        }
    });
}

void MileageIndicator::Init()
{
    ModelInfoMgr::RegisterDummy([](CVehicle *pVeh, RwFrame *pFrame, const std::string_view nodeName) {
        if (nodeName.starts_with("x_odometer") || nodeName.starts_with("fc_om")) {
            VehMileageData &data = m_VehData.Get(pVeh);
            std::string name(nodeName);
            auto& indicator = data.vecIndicatorData[name];

            FrameUtil::StoreChilds(pFrame, indicator.vecFrameList);
            for (size_t i = 0; i < std::min<size_t>(10, indicator.vecFrameList.size()); ++i)
                CaptureState(pVeh, indicator.vecFrameList[i]);

            indicator.dCurrentDistance = static_cast<double>(rand() % 999999);
            indicator.pFrame = pFrame;

            auto &jsonData = DataMgr::Get(pVeh->m_nModelIndex);
            if (jsonData.contains("gauges") && jsonData["gauges"].contains(name)) {
                indicator.fMul = jsonData["gauges"][name].value("kph", true) ? 160.9f : 1.0f;
            }
            data.bInitialized = true;
        } else if (nodeName.starts_with("odometr")) {
            // Active Dashboard individual rolling drum nodes (odometr1 .. odometr10)
            VehMileageData &data = m_VehData.Get(pVeh);
            auto& indicator = data.vecIndicatorData["ad_odometer"];
            CaptureState(pVeh, pFrame);
            int idx = 0;
            if (nodeName.size() > 7 && std::isdigit(nodeName[7])) {
                idx = std::stoi(std::string(nodeName.substr(7))) - 1;
            }
            if (idx >= 0 && idx < 10) {
                if (indicator.vecFrameList.size() <= static_cast<size_t>(idx)) {
                    indicator.vecFrameList.resize(idx + 1, nullptr);
                }
                indicator.vecFrameList[idx] = pFrame;
            }
            indicator.pFrame = pFrame;
            if (!data.bInitialized) {
                indicator.dCurrentDistance = static_cast<double>(rand() % 999999);
                data.bInitialized = true;
            }
        }
    });

    ModelInfoMgr::RegisterRender([](CVehicle *pVeh) {
    if (!CBaseFeature::IsEnabled(eFeatureMatrix::AnimatedOdoMeter)) return;
    if (!pVeh || !pVeh->GetIsOnScreen() || pVeh->m_fHealth <= 0.0f) return;

    VehMileageData &data = m_VehData.Get(pVeh);
    if (!data.bInitialized) return;

    for (auto& [name, indicator] : data.vecIndicatorData) {
        if (indicator.vecFrameList.empty()) continue;

        float curWheelRot = (pVeh->m_nVehicleSubClass == VEHICLE_BIKE)
            ? static_cast<CBike *>(pVeh)->m_aWheelPitchAngles[1]
            : static_cast<CAutomobile *>(pVeh)->m_fWheelRotation[3];

        float diff = curWheelRot - indicator.fLastWheelRot;
        if (abs(diff) > 5.0f) diff = 0.0f;

        CVehicleModelInfo *pModelInfo = static_cast<CVehicleModelInfo *>(CModelInfo::GetModelInfo(pVeh->m_nModelIndex));
        float wheelRadius = (pModelInfo && pModelInfo->m_fWheelSizeRear > 0.0f) ? pModelInfo->m_fWheelSizeRear : 0.35f;
        float wheelDivisor = (wheelRadius * 8.17f) * indicator.fMul;
        indicator.dCurrentDistance += (abs(diff) / (wheelDivisor > 0.0f ? wheelDivisor : 2.86f));
        indicator.fLastWheelRot = curWheelRot;

        int numDigits = std::min<int>(10, static_cast<int>(indicator.vecFrameList.size()));
        int displayVal = static_cast<int>(indicator.dCurrentDistance);

        int divisor = 1;
        for (int k = 1; k < numDigits; ++k) divisor *= 10;

        for (int i = 0; i < numDigits; i++) {
            int currentDigit = (displayVal / divisor) % 10;
            divisor /= 10;

            if (indicator.vecFrameList[i] && indicator.lastDigits[i] != currentDigit) {
                if (indicator.lastDigits[i] != -1) {
                    int steps = currentDigit - indicator.lastDigits[i];
                    if (steps < 0) steps += 10;

                    float angleToRotate = static_cast<float>(steps) * 36.0f;
                    FrameUtil::SetRotationX(indicator.vecFrameList[i], angleToRotate);
                } else {
                    FrameUtil::SetRotationX(indicator.vecFrameList[i], currentDigit * 36.0f);
                }

                indicator.lastDigits[i] = currentDigit;
            }
        }
    }
});
}

void RPMGauge::Init()
{
    ModelInfoMgr::RegisterDummy([](CVehicle *pVeh, RwFrame *pFrame, const std::string_view nodeName)
    {
        if (nodeName.starts_with("x_rpm") || nodeName.starts_with("fc_rpm") || nodeName.contains("tahook")) {
            VehRPMData &data = m_VehData.Get(pVeh);
            std::string name(nodeName);
            data.vecGaugeData[name].bReverse = (nodeName.starts_with("rtahook") || nodeName.contains("rtaho"));
            auto &jsonData = DataMgr::Get(pVeh->m_nModelIndex);
            if (jsonData.contains("gauges") && jsonData["gauges"].contains(name))
            {
                if (jsonData["gauges"][name].contains("maxrpm"))
                {
                    data.vecGaugeData[name].iMaxRPM = jsonData["gauges"][name].value("maxrpm", data.vecGaugeData[name].iMaxRPM);
                }
                if (jsonData["gauges"][name].contains("maxrotation"))
                {
                    data.vecGaugeData[name].fMaxRotation = jsonData["gauges"][name].value("maxrotation", data.vecGaugeData[name].fMaxRotation);
                }
            } else {
                const auto *ad = ADConfig::Get(pVeh->m_nModelIndex);
                if (ad && ad->driveRpmMul > 0.0f) {
                    data.vecGaugeData[name].fMaxRotation *= ad->driveRpmMul;
                }
            }
            CaptureState(pVeh, pFrame);
            data.vecGaugeData[name].pFrame = pFrame;
            data.bInitialized = true;
        }
    });

    ModelInfoMgr::RegisterRender([](CVehicle *pVeh)
    {
        if (!CBaseFeature::IsEnabled(eFeatureMatrix::AnimatedRpmMeter)) return;
        if (!pVeh || !pVeh->GetIsOnScreen() || pVeh->m_fHealth <= 0.0f) return;

        VehRPMData &data = m_VehData.Get(pVeh);
        if (data.bInitialized) {
            float delta = CTimer::ms_fTimeStep;
            float speed = Util::GetVehicleSpeedRealistic(pVeh);

            for (auto& e : data.vecGaugeData) {
                float rpm = 0.0f;

                if (pVeh->m_nCurrentGear != 0) {
                    if (pVeh->m_pHandlingData && pVeh->m_nCurrentGear > 0 && pVeh->m_nCurrentGear <= pVeh->m_pHandlingData->m_transmissionData.m_nNumberOfGears) {
                        float maxGearVel = pVeh->m_pHandlingData->m_transmissionData.m_aGears[pVeh->m_nCurrentGear].m_fMaxVelocity;
                        if (maxGearVel > 0.0f) {
                            rpm = std::clamp((speed / (maxGearVel * 160.9f)), 0.1f, 1.0f) * e.second.iMaxRPM;
                        } else {
                            rpm = (speed / abs((float)pVeh->m_nCurrentGear)) * 100.0f;
                        }
                    } else {
                        rpm = (speed / abs((float)pVeh->m_nCurrentGear)) * 100.0f;
                    }
                }

                if (pVeh->bEngineOn) {
                  rpm = std::max(rpm, 0.12f * e.second.iMaxRPM);
                }

                float targetRotation = (rpm / (float)e.second.iMaxRPM) * e.second.fMaxRotation;
                if (e.second.bReverse) targetRotation = -targetRotation;
                targetRotation = ClampRotation(targetRotation, e.second.fMaxRotation);

                float change = (targetRotation - e.second.fCurRotation) * 0.25f * delta;
                FrameUtil::SetRotationY(e.second.pFrame, change);
                e.second.fCurRotation += change;
            }
        }
    });
}

void SpeedGauge::Init()
{
    ModelInfoMgr::RegisterDummy([](CVehicle *pVeh, RwFrame *pFrame, const std::string_view nodeName)
    {
        if (nodeName.starts_with("x_sm") || nodeName.starts_with("fc_sm") || nodeName.contains("speedook")) {
            VehSpeedData &data = m_VehData.Get(pVeh);
            std::string name(nodeName);
            data.vecGaugeData[name].bReverse = (nodeName.starts_with("rspeedook") || nodeName.contains("rspeedo"));
            auto &jsonData = DataMgr::Get(pVeh->m_nModelIndex);
            if (jsonData.contains("gauges") && jsonData["gauges"].contains(name))
            {
                if (jsonData["gauges"][name].contains("kph"))
                {
                    data.vecGaugeData[name].fMul = jsonData["gauges"][name]["kph"].get<bool>() ? 1.0f : (1.0f / 1.609f);
                }
                if (jsonData["gauges"][name].contains("maxspeed"))
                {
                    data.vecGaugeData[name].iMaxSpeed = jsonData["gauges"][name].value("maxspeed", data.vecGaugeData[name].iMaxSpeed);
                }
                if (jsonData["gauges"][name].contains("maxrotation"))
                {
                    data.vecGaugeData[name].fMaxRotation = jsonData["gauges"][name].value("maxrotation", data.vecGaugeData[name].fMaxRotation);
                }
            } else {
                const auto *ad = ADConfig::Get(pVeh->m_nModelIndex);
                if (ad && ad->speedoMul > 0.0f) {
                    data.vecGaugeData[name].fMaxRotation *= ad->speedoMul;
                }
            }
            CaptureState(pVeh, pFrame);
            data.vecGaugeData[name].pFrame = pFrame;
            data.bInitialized = true;
        }
    });

    ModelInfoMgr::RegisterRender([](CVehicle *pVeh)
    {
        if (!CBaseFeature::IsEnabled(eFeatureMatrix::AnimatedSpeedMeter)) return;
        if (!pVeh || !pVeh->GetIsOnScreen() || pVeh->m_fHealth <= 0.0f) return;

        VehSpeedData &data = m_VehData.Get(pVeh);
        if (data.bInitialized) {
            float speed = Util::GetVehicleSpeedRealistic(pVeh);
            float delta = CTimer::ms_fTimeStep;

            for (auto& e : data.vecGaugeData) {
                float targetRotation = (speed * e.second.fMul / (float)e.second.iMaxSpeed) * e.second.fMaxRotation;
                if (pVeh->m_nCurrentGear == 0) {
                    targetRotation = -targetRotation;
                }
                if (e.second.bReverse) targetRotation = -targetRotation;
                targetRotation = ClampRotation(targetRotation, e.second.fMaxRotation);
                float change = (targetRotation - e.second.fCurRotation) * 0.5f * delta;
                FrameUtil::SetRotationY(e.second.pFrame, change);
                e.second.fCurRotation += change;
            }
        }
    });
}

void TurboGauge::Init()
{
    ModelInfoMgr::RegisterDummy([](CVehicle *pVeh, RwFrame *pFrame, const std::string_view nodeName)
    {
        if (nodeName.starts_with("x_tm")) {
            VehTurboData &data = m_VehData.Get(pVeh);
            std::string name(nodeName);
            auto &jsonData = DataMgr::Get(pVeh->m_nModelIndex);
            if (jsonData.contains("gauges") && jsonData["gauges"].contains(name))
            {
                if (jsonData["gauges"][name].contains("maxturbo"))
                {
                    data.vecGaugeData[name].iMaxTurbo = jsonData["gauges"][name].value("maxturbo", data.vecGaugeData[name].iMaxTurbo);
                }
                if (jsonData["gauges"][name].contains("maxrotation"))
                {
                    data.vecGaugeData[name].fMaxRotation = jsonData["gauges"][name].value("maxrotation", data.vecGaugeData[name].fMaxRotation);
                }
            }
            CaptureState(pVeh, pFrame);
            data.vecGaugeData[name].pFrame = pFrame;
            data.bInitialized = true;
        }
    });

    ModelInfoMgr::RegisterRender([](CVehicle *pVeh)
    {
        if (!CBaseFeature::IsEnabled(eFeatureMatrix::AnimatedTurboMeter)) return;
        if (!pVeh || !pVeh->GetIsOnScreen() || pVeh->m_fHealth <= 0.0f) return;

        VehTurboData &data = m_VehData.Get(pVeh);
        if (data.bInitialized) {
            float speed = Util::GetVehicleSpeedRealistic(pVeh);
            float delta = CTimer::ms_fTimeStep;

            for (auto& e : data.vecGaugeData) {
                float turbo = speed - e.second.fPrevTurbo;

                if (pVeh->m_nCurrentGear != 0)
                {
                    turbo += (turbo >= 0) ? 10.0f : -10.0f;
                }

                float targetRotation = (e.second.fMaxRotation / (float)e.second.iMaxTurbo) * turbo * delta;
                if (pVeh->m_nCurrentGear == 0) {
                    targetRotation = -targetRotation;
                }
                targetRotation = ClampRotation(targetRotation, e.second.fMaxRotation);
                float change = (targetRotation - e.second.fCurRotation) * 0.25f * delta;
                FrameUtil::SetRotationY(e.second.pFrame, change);
                e.second.fCurRotation += change;
                e.second.fPrevTurbo = speed;
            }
        }
    });
}

void FixedGauge::Init()
{
    ModelInfoMgr::RegisterDummy([](CVehicle *pVeh, RwFrame *pFrame, const std::string_view nodeName)
    {
        if (!pVeh || !pFrame) return;
        if (nodeName.starts_with("x_gauge_fixed") || nodeName == "x_gasmeter" || nodeName == "x_gm" || nodeName == "petrolok") {
            auto &jsonData = DataMgr::Get(pVeh->m_nModelIndex);

            float minAngle = 30.0f;
            float maxAngle = 120.0f;
            std::string name(nodeName);
            if (jsonData.contains("gauges") && jsonData["gauges"].contains(name)) {
                minAngle = jsonData["gauges"][name].value("minangle", minAngle);
                maxAngle = jsonData["gauges"][name].value("maxangle", maxAngle);
            }
            auto &gauges = m_VehData.Get(pVeh).gauges;
            for (auto &entry : gauges) if (entry.frame == pFrame) {
                float angle = CBaseFeature::IsEnabled(eFeatureMatrix::AnimatedGasMeter) ? minAngle + entry.fraction * (maxAngle - minAngle) : 0.0f;
                FrameUtil::SetRotationY(pFrame, angle - entry.angle);
                entry.angle = angle;
                return;
            }
            CaptureState(pVeh, pFrame);
            float angle = RandomNumberInRange(minAngle, maxAngle);
            float fraction = maxAngle != minAngle ? (angle - minAngle) / (maxAngle - minAngle) : 0.0f;
            if (!CBaseFeature::IsEnabled(eFeatureMatrix::AnimatedGasMeter)) angle = 0.0f;
            FrameUtil::SetRotationY(pFrame, angle);
            gauges.push_back({pFrame, fraction, angle});
        }
    });
}

void AuxGauges::Init()
{
    LOG_VERBOSE("Init {}", __FUNCTION__);
    ModelInfoMgr::RegisterDummy([](CVehicle *pVeh, RwFrame *pFrame, const std::string_view nodeName)
    {
        if (!pVeh || !pFrame) return;

        eAuxGaugeType type = eAuxGaugeType::Oil;
        bool isReverse = false;
        bool matched = false;

        if (nodeName == "oil" || nodeName == "roil") {
            type = eAuxGaugeType::Oil;
            isReverse = (nodeName == "roil");
            matched = true;
        } else if (nodeName == "temperature" || nodeName == "rtemperature") {
            type = eAuxGaugeType::Temperature;
            isReverse = (nodeName == "rtemperature");
            matched = true;
        } else if (nodeName == "volts") {
            type = eAuxGaugeType::Volts;
            matched = true;
        } else if (nodeName == "ampers") {
            type = eAuxGaugeType::Ampers;
            matched = true;
        }

        if (matched) {
            VehAuxGaugeData &data = m_VehData.Get(pVeh);
            CaptureState(pVeh, pFrame);

            AuxGaugeEntry entry;
            entry.frame = pFrame;
            entry.type = type;
            entry.isReverse = isReverse;
            entry.maxRotation = 85.0f;

            const auto *ad = ADConfig::Get(pVeh->m_nModelIndex);
            if (ad) {
                if (type == eAuxGaugeType::Oil && ad->oilMul > 0.0f) entry.mul = ad->oilMul;
                else if (type == eAuxGaugeType::Temperature && ad->tempMul > 0.0f) entry.mul = ad->tempMul;
                else if (type == eAuxGaugeType::Volts && ad->voltAmpMul > 0.0f) entry.mul = ad->voltAmpMul;
                else if (type == eAuxGaugeType::Ampers && ad->voltAmpMul > 0.0f) entry.mul = ad->voltAmpMul;
            }

            data.gauges.push_back(entry);
            data.bInitialized = true;
        }
    });

    ModelInfoMgr::RegisterRender([](CVehicle *pVeh)
    {
        if (!CBaseFeature::IsEnabled(eFeatureMatrix::AnimatedGasMeter)) return;
        if (!pVeh || !pVeh->GetIsOnScreen() || pVeh->m_fHealth <= 0.0f) return;

        VehAuxGaugeData &data = m_VehData.Get(pVeh);
        if (!data.bInitialized || data.gauges.empty()) return;

        float delta = CTimer::ms_fTimeStep;
        bool engineOn = pVeh->bEngineOn;

        // Smoothly warm up engine temp if running, cool down if off
        if (engineOn) {
            data.engineTemp = std::min(90.0f, data.engineTemp + 0.02f * delta);
        } else {
            data.engineTemp = std::max(40.0f, data.engineTemp - 0.03f * delta);
        }

        float speed = Util::GetVehicleSpeedRealistic(pVeh);

        for (auto &g : data.gauges) {
            if (!g.frame) continue;

            float targetFrac = 0.0f;
            switch (g.type) {
                case eAuxGaugeType::Oil:
                    if (engineOn) {
                        float rpmFrac = std::clamp(speed / 120.0f, 0.2f, 1.0f);
                        targetFrac = 0.3f + rpmFrac * 0.55f;
                    } else {
                        targetFrac = 0.0f;
                    }
                    break;
                case eAuxGaugeType::Temperature:
                    targetFrac = std::clamp((data.engineTemp - 40.0f) / 60.0f, 0.0f, 1.0f);
                    break;
                case eAuxGaugeType::Volts:
                    if (engineOn) {
                        targetFrac = 0.72f; // ~14V
                    } else {
                        targetFrac = 0.45f; // ~12V
                    }
                    break;
                case eAuxGaugeType::Ampers:
                    if (engineOn) {
                        targetFrac = 0.65f; // Charging
                    } else {
                        targetFrac = 0.50f; // Center / 0
                    }
                    break;
            }

            float targetRot = targetFrac * g.maxRotation * g.mul;
            if (g.isReverse) targetRot = -targetRot;

            float change = (targetRot - g.curRotation) * 0.2f * delta;
            FrameUtil::SetRotationY(g.frame, change);
            g.curRotation += change;
        }
    });
}

void AuxGauges::OnToggle(CVehicle *vehicle, bool enabled)
{
    if (!vehicle) return;
    VehAuxGaugeData &data = m_VehData.Get(vehicle);
    for (auto &g : data.gauges) {
        g.curRotation = 0.0f;
    }
}

void GearIndicator::OnToggle(CVehicle *vehicle, bool enabled) {
    if (enabled) return;
    for (auto &e : m_VehData.Get(vehicle).vecIndicatorData) e.iCurrent = UINT_MAX;
}

void MileageIndicator::OnToggle(CVehicle *vehicle, bool enabled) {
    for (auto &[name, e] : m_VehData.Get(vehicle).vecIndicatorData) {
        std::fill(std::begin(e.lastDigits), std::end(e.lastDigits), -1);
        if (vehicle->m_nVehicleSubClass == VEHICLE_BIKE) e.fLastWheelRot = static_cast<CBike *>(vehicle)->m_aWheelPitchAngles[1];
        else if (vehicle->m_nVehicleSubClass == VEHICLE_AUTOMOBILE) e.fLastWheelRot = static_cast<CAutomobile *>(vehicle)->m_fWheelRotation[3];
    }
}

void RPMGauge::OnToggle(CVehicle *vehicle, bool enabled) {
    if (enabled) return;
    for (auto &[name, e] : m_VehData.Get(vehicle).vecGaugeData) e.fCurRotation = 0.0f;
}

void SpeedGauge::OnToggle(CVehicle *vehicle, bool enabled) {
    if (enabled) return;
    for (auto &[name, e] : m_VehData.Get(vehicle).vecGaugeData) e.fCurRotation = 0.0f;
}

void TurboGauge::OnToggle(CVehicle *vehicle, bool enabled) {
    if (enabled) return;
    for (auto &[name, e] : m_VehData.Get(vehicle).vecGaugeData) { e.fCurRotation = 0.0f; e.fPrevTurbo = Util::GetVehicleSpeedRealistic(vehicle); }
}

void FixedGauge::OnToggle(CVehicle *vehicle, bool enabled) {
    for (auto &e : m_VehData.Get(vehicle).gauges) {
        e.angle = 0.0f;
        if (!enabled || !m_VehData.Get(vehicle).frameState.IsCurrent(vehicle, e.frame)) continue;
        const auto &data = DataMgr::Get(vehicle->m_nModelIndex);
        float min = 30.0f, max = 120.0f;
        if (data.contains("gauges") && data["gauges"].contains(GetSafeFrameNodeName(e.frame))) {
            const auto &json = data["gauges"][GetSafeFrameNodeName(e.frame)];
            min = json.value("minangle", min); max = json.value("maxangle", max);
        }
        e.angle = min + e.fraction * (max - min);
        FrameUtil::SetRotationY(e.frame, e.angle);
    }
}
