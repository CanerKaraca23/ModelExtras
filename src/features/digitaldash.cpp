#include "pch.h"
#include "digitaldash.h"
#include "utils/modelinfomgr.h"
#include "utils/util.h"
#include <CClock.h>
#include <CAutomobile.h>
#include <CBike.h>
#include <algorithm>

static int ExtractDigitIndex(const std::string_view name)
{
    // Extract trailing number: e.g. "dsp_speedo1" -> 0, "dsp_odometr10" -> 9
    size_t lastNonDigit = name.find_last_not_of("0123456789");
    if (lastNonDigit != std::string_view::npos && lastNonDigit + 1 < name.size()) {
        int val = 0;
        for (size_t i = lastNonDigit + 1; i < name.size(); ++i) {
            val = val * 10 + (name[i] - '0');
        }
        return val - 1; // 1-based to 0-based
    }
    return 0;
}

void DigitalDashboardFeature::Init()
{
    LOG_VERBOSE("Init {}", __FUNCTION__);
    ModelInfoMgr::RegisterDummy([](CVehicle *pVeh, RwFrame *pFrame, const std::string_view nodeName)
    {
        if (!pVeh || !pFrame) return;

        bool matched = false;
        DigitalDashData &data = m_VehData.Get(pVeh);

        if (nodeName.starts_with("dsp_speedo")) {
            CaptureState(pVeh, pFrame);
            int idx = ExtractDigitIndex(nodeName);
            if (idx >= 0 && idx < 10) {
                if (data.speedDigits.size() <= static_cast<size_t>(idx)) {
                    data.speedDigits.resize(idx + 1, nullptr);
                }
                data.speedDigits[idx] = pFrame;
                matched = true;
            }
        } else if (nodeName.starts_with("dsp_odometr")) {
            CaptureState(pVeh, pFrame);
            int idx = ExtractDigitIndex(nodeName);
            if (idx >= 0 && idx < 10) {
                if (data.odoDigits.size() <= static_cast<size_t>(idx)) {
                    data.odoDigits.resize(idx + 1, nullptr);
                }
                data.odoDigits[idx] = pFrame;
                matched = true;
            }
        } else if (nodeName.starts_with("dsp_hour")) {
            CaptureState(pVeh, pFrame);
            int idx = ExtractDigitIndex(nodeName);
            if (idx >= 0 && idx < 5) {
                if (data.clockHourDigits.size() <= static_cast<size_t>(idx)) {
                    data.clockHourDigits.resize(idx + 1, nullptr);
                }
                data.clockHourDigits[idx] = pFrame;
                matched = true;
            }
        } else if (nodeName.starts_with("dsp_min")) {
            CaptureState(pVeh, pFrame);
            int idx = ExtractDigitIndex(nodeName);
            if (idx >= 0 && idx < 5) {
                if (data.clockMinDigits.size() <= static_cast<size_t>(idx)) {
                    data.clockMinDigits.resize(idx + 1, nullptr);
                }
                data.clockMinDigits[idx] = pFrame;
                matched = true;
            }
        }

        if (matched) {
            data.bInitialized = true;
            data.totalOdo = static_cast<double>(rand() % 99999);
        }
    });

    ModelInfoMgr::RegisterRender([](CVehicle *pVeh)
    {
        if (!CBaseFeature::IsEnabled(eFeatureMatrix::DigitalDashboard)) return;
        if (!pVeh || !pVeh->GetIsOnScreen() || pVeh->m_fHealth <= 0.0f) return;

        DigitalDashData &data = m_VehData.Get(pVeh);
        if (!data.bInitialized) return;

        // 1. Digital Speedometer
        if (!data.speedDigits.empty()) {
            int speedKmh = static_cast<int>(Util::GetVehicleSpeedRealistic(pVeh));
            speedKmh = std::clamp(speedKmh, 0, 999);

            int d1 = (speedKmh / 100) % 10;
            int d2 = (speedKmh / 10) % 10;
            int d3 = speedKmh % 10;

            if (data.speedDigits.size() >= 1 && data.speedDigits[0]) FrameUtil::HideAllAtomicsExcept(data.speedDigits[0], d1);
            if (data.speedDigits.size() >= 2 && data.speedDigits[1]) FrameUtil::HideAllAtomicsExcept(data.speedDigits[1], d2);
            if (data.speedDigits.size() >= 3 && data.speedDigits[2]) FrameUtil::HideAllAtomicsExcept(data.speedDigits[2], d3);
        }

        // 2. Digital Odometer
        if (!data.odoDigits.empty()) {
            float curRot = (pVeh->m_nVehicleSubClass == VEHICLE_BIKE)
                ? static_cast<CBike*>(pVeh)->m_aWheelPitchAngles[1]
                : static_cast<CAutomobile*>(pVeh)->m_fWheelRotation[3];

            float diff = std::abs(curRot - data.lastWheelRot);
            if (diff > 5.0f) diff = 0.0f;
            data.lastWheelRot = curRot;
            data.totalOdo += (diff / 2.86f);

            uint64_t odoVal = static_cast<uint64_t>(data.totalOdo);
            for (size_t i = 0; i < data.odoDigits.size(); ++i) {
                // Digits from right to left
                size_t revIdx = data.odoDigits.size() - 1 - i;
                int digit = static_cast<int>(odoVal % 10);
                odoVal /= 10;
                if (data.odoDigits[revIdx]) {
                    FrameUtil::HideAllAtomicsExcept(data.odoDigits[revIdx], digit);
                }
            }
        }

        // 3. Digital Clock
        if (!data.clockHourDigits.empty() || !data.clockMinDigits.empty()) {
            int hours = CClock::ms_nGameClockHours;
            int mins = CClock::ms_nGameClockMinutes;

            if (data.clockHourDigits.size() >= 2) {
                if (data.clockHourDigits[0]) FrameUtil::HideAllAtomicsExcept(data.clockHourDigits[0], (hours / 10) % 10);
                if (data.clockHourDigits[1]) FrameUtil::HideAllAtomicsExcept(data.clockHourDigits[1], hours % 10);
            }
            if (data.clockMinDigits.size() >= 2) {
                if (data.clockMinDigits[0]) FrameUtil::HideAllAtomicsExcept(data.clockMinDigits[0], (mins / 10) % 10);
                if (data.clockMinDigits[1]) FrameUtil::HideAllAtomicsExcept(data.clockMinDigits[1], mins % 10);
            }
        }
    });
}

void DigitalDashboardFeature::OnToggle(CVehicle *vehicle, bool enabled)
{
    // FrameState restores original matrices and atomic visibilities
}
