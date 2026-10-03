#include "pch.h"
#include "spoiler.h"
#include "utils/datamgr.h"
#include "utils/modelinfomgr.h"
#include "utils/car.h"
#include "utils/frame.h"
#include <cmath>

void Spoiler::Init()
{
    const bool legacyDefaults = gConfig.ReadBoolean("SPOILERS", "UseLegacyDefaults", false);
    ModelInfoMgr::RegisterDummy([legacyDefaults](CVehicle *pVeh, RwFrame *pFrame, const std::string_view nodeName)
    {
        if (!pVeh || !pFrame || !nodeName.starts_with("movspoiler")) {
            return;
        }
        
        SpoilerVehData &data = m_VehData.Get(pVeh);
        SpoilerData spoilerData;
        if (legacyDefaults) {
            spoilerData.m_fRotation = 25.0f;
            spoilerData.m_nTime = 2500.0f;
            auto first = nodeName.find('_');
            if (first != std::string_view::npos) {
                const std::string suffix(nodeName.substr(first + 1));
                try {
                    size_t end = 0;
                    float rotation = std::stof(suffix, &end);
                    if (std::isfinite(rotation)) spoilerData.m_fRotation = rotation;
                    if (end < suffix.size() && suffix[end] == '_') {
                        int time = std::stoi(suffix.substr(end + 1));
                        if (time > 0) spoilerData.m_nTime = static_cast<float>(time);
                    }
                } catch (...) {}
            }
        } else {
        auto first = nodeName.find('_');
        auto second = (first != std::string_view::npos) ? nodeName.find('_', first + 1) : std::string_view::npos;
        if (first != std::string_view::npos && second != std::string_view::npos && second > first + 1) {
            try {
                spoilerData.m_fRotation = std::stof(std::string(nodeName.substr(first + 1, second - first - 1)));
            } catch (...) {
                spoilerData.m_fRotation = 3.0f;
            }
        }
        else {
            spoilerData.m_fRotation = 3.0f;
        }

        auto last = nodeName.rfind('_');
        if (last != std::string_view::npos && last + 1 < nodeName.size()) {
            try {
                spoilerData.m_nTime = std::stof(std::string(nodeName.substr(last + 1)));
            } catch (...) {
                spoilerData.m_nTime = 3000.0f;
            }
        }
        else {
            spoilerData.m_nTime = 3000.0f;
        }
        }

        spoilerData.m_pFrame = pFrame;

        auto &jsonData = DataMgr::Get(pVeh->m_nModelIndex);
        std::string name(nodeName);
        if (jsonData.contains("spoilers") && jsonData["spoilers"].contains(name))
        {
            spoilerData.m_fRotation = jsonData["spoilers"][name].value("rotation", 30.0f);
            spoilerData.m_nTime = jsonData["spoilers"][name].value("time", 3000.0f);
            spoilerData.m_nTriggerSpeed = jsonData["spoilers"][name].value("triggerspeed", 20.0f);
            spoilerData.m_bLegacySpeed = !jsonData["spoilers"][name].contains("triggerspeed");
        }
        else
        {
            spoilerData.m_nTriggerSpeed = 20.0f;
        }
        data.m_Spoilers.push_back(spoilerData);
    });

    const bool legacyMotion = gConfig.ReadBoolean("SPOILERS", "UseLegacyMotion", false);
    ModelInfoMgr::RegisterRender([legacyMotion](CVehicle *pVeh)
                                {
        if (!CBaseFeature::IsEnabled(eFeatureMatrix::AnimatedSpoiler))
        {
            return;
        }
        if (!pVeh || !pVeh->m_pRwClump || !pVeh->GetIsOnScreen())
        {
            return;
        }

        SpoilerVehData &data = m_VehData.Get(pVeh); 
        if (data.m_Spoilers.size() == 0) {
            return;
        }

        for (auto& e: data.m_Spoilers) {
            if (!FrameUtil::ContainsFrame(RpClumpGetFrame(pVeh->m_pRwClump), e.m_pFrame) ||
                CarUtil::IsLegacyParentDamaged(pVeh, e.m_pFrame)) continue;
            if (legacyMotion) {
                if (!std::isfinite(e.m_fRotation)) continue;
                const uint32_t now = CTimer::m_snTimeInMilliseconds;
                if (e.m_nLegacyState < 2) {
                    const float speed = e.m_bLegacySpeed ? pVeh->m_vecMoveSpeed.Magnitude() : Util::GetVehicleSpeed(pVeh);
                    if (!std::isfinite(speed)) continue;
                    const bool open = e.m_bLegacySpeed ? static_cast<double>(speed) * 178.0 >= 125.0 : speed > e.m_nTriggerSpeed;
                    if (open != (e.m_nLegacyState == 1)) {
                        e.m_nLegacyState = open ? 3 : 2;
                        e.m_nTransitionStart = now;
                    }
                    continue;
                }
                const double time = std::isfinite(e.m_nTime) && e.m_nTime > 0 ? e.m_nTime : 2500.0;
                const uint32_t duration = static_cast<uint32_t>(std::clamp(time, 1.0, 4294967295.0));
                const uint32_t elapsed = now - e.m_nTransitionStart;
                const bool opening = e.m_nLegacyState == 3;
                const double progress = elapsed > duration ? 1.0 : static_cast<double>(elapsed) / duration;
                e.m_fCurrentRotation = -static_cast<float>((opening ? progress : 1.0 - progress) * e.m_fRotation);
                if (elapsed > duration) e.m_nLegacyState = opening ? 1 : 0;
                const float radians = static_cast<float>(e.m_fCurrentRotation * 0.017453292);
                const float sine = std::sin(radians), cosine = std::cos(radians);
                auto &matrix = e.m_pFrame->modelling;
                matrix.right = {1.0f, 0.0f, 0.0f};
                matrix.up = {0.0f, cosine, sine};
                matrix.at = {0.0f, -sine, cosine};
                RwMatrixUpdate(&matrix);
                continue;
            }
            bool isEnabled = Util::GetVehicleSpeed(pVeh) > e.m_nTriggerSpeed;

           float targetAngle = isEnabled ? -e.m_fRotation : 0.0f;
           float totalTime = std::max(1.0f, static_cast<float>(e.m_nTime));

           float transitionSpeed = (isEnabled ? 10.0f : 15.0f) / totalTime;

           // Smoothing
           float t = 1.0f - std::exp(-transitionSpeed * CTimer::ms_fTimeStep);

           e.m_fCurrentRotation =
           e.m_fCurrentRotation * (1.0f - t) + targetAngle * t;

           MatrixUtil::ResetRotation(&e.m_pFrame->modelling);
           MatrixUtil::SetRotationXAbsolute(&e.m_pFrame->modelling, e.m_fCurrentRotation);
           RwMatrixUpdate(&e.m_pFrame->modelling);
        } });
}
