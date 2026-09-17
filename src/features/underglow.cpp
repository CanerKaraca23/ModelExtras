#include "pch.h"
#include "underglow.h"
#include <CCamera.h>
#include <CPointLights.h>
#include "utils/modelinfomgr.h"
#include "utils/meevents.h"
#include "utils/datamgr.h"
#include "utils/util.h"
#include "utils/car.h"
#include <cmath>

void Underglow::Init()
{
    ReloadConfig();

    ModelInfoMgr::RegisterDummy([](CVehicle* pVeh, RwFrame* pFrame, const std::string_view nodeName)
    {
        if (nodeName.find("underglow") != std::string_view::npos || nodeName.find("neon") != std::string_view::npos)
        {
            UnderglowData& data = m_VehData.Get(pVeh);
            data.pDummies.push_back(pFrame);
            data.bConfigured = true;
        }
    });

    MEEvents::vehPreRenderEvent.before += [](CVehicle* pVeh)
    {
        Process(pVeh);
    };
}

void Underglow::ReloadConfig()
{
    CBaseFeature::ReloadConfig();
}

void Underglow::Reload(CVehicle* pVeh)
{
    ReloadConfig();
    if (pVeh)
    {
        UnderglowData& data = m_VehData.Get(pVeh);
        for (auto& h : data.hLights)
        {
            ProperShadersMgr::DestroyLight(h);
        }
        data.hLights.clear();
        data.bConfigured = false;
        data.bHasJsonc = false;
    }
}

void Underglow::Process(CVehicle* pVeh)
{
    if (!CBaseFeature::IsEnabled(eFeatureMatrix::Underglow))
    {
        return;
    }

    if (!pVeh || pVeh->m_fHealth <= 0.0f || !pVeh->GetIsOnScreen())
    {
        return;
    }

    if (pVeh->m_nVehicleSubClass != VEHICLE_AUTOMOBILE && pVeh->m_nVehicleSubClass != VEHICLE_BIKE)
    {
        return;
    }

    UnderglowData& data = m_VehData.Get(pVeh);

    // One-time JSONC config check
    if (!data.bConfigured)
    {
        data.bConfigured = true;
        if (DataMgr::Has(pVeh->m_nModelIndex))
        {
            const auto& j = DataMgr::Get(pVeh->m_nModelIndex);
            if (j.contains("underglow") && j["underglow"].is_object())
            {
                const auto& ug = j["underglow"];
                data.bHasJsonc = true;
                if (ug.contains("color") && ug["color"].is_array() && ug["color"].size() >= 3)
                {
                    data.color.r = ug["color"][0].get<unsigned char>();
                    data.color.g = ug["color"][1].get<unsigned char>();
                    data.color.b = ug["color"][2].get<unsigned char>();
                }
                if (ug.contains("intensity") && ug["intensity"].is_number())
                {
                    data.fIntensity = ug["intensity"].get<float>();
                }
                if (ug.contains("mode") && ug["mode"].is_string())
                {
                    data.mode = ug["mode"].get<std::string>();
                }
            }
        }
    }

    if (!data.bHasJsonc && data.pDummies.empty())
    {
        return;
    }

    bool isLightActive = (pVeh->bLightsOn || CarUtil::IsLightsForcedOn(pVeh) || (Util::IsNightTime() && !Util::IsEngineOff(pVeh))) && !CarUtil::IsLightsForcedOff(pVeh);
    if (!isLightActive)
    {
        for (auto& h : data.hLights)
        {
            ProperShadersMgr::SetEnabled(h, false);
        }
        return;
    }

    if (CVector::Distance(pVeh->GetPosition(), TheCamera.GetPosition()) > 100.0f)
    {
        for (auto& h : data.hLights)
        {
            ProperShadersMgr::SetEnabled(h, false);
        }
        return;
    }

    // Dynamic mode multipliers (e.g. breathe, strobe)
    float curIntensity = data.fIntensity;
    if (data.mode == "breathe")
    {
        float t = static_cast<float>(CTimer::m_snTimeInMilliseconds) * 0.003f;
        curIntensity *= (0.5f + 0.5f * std::sin(t));
    }
    else if (data.mode == "strobe")
    {
        bool flash = ((CTimer::m_snTimeInMilliseconds / 120) % 2) == 0;
        if (!flash) curIntensity = 0.0f;
    }

    if (curIntensity <= 0.01f)
    {
        for (auto& h : data.hLights)
        {
            ProperShadersMgr::SetEnabled(h, false);
        }
        return;
    }

    // Determine emitter positions
    std::vector<CVector> worldPositions;
    if (!data.pDummies.empty())
    {
        for (auto* pFrame : data.pDummies)
        {
            if (pFrame)
            {
                RwFrameUpdateObjects(pFrame);
                worldPositions.push_back((*(CMatrix*)&pFrame->ltm).pos);
            }
        }
    }
    else
    {
        // Default left and right undercarriage tubes
        CVector leftSide(-0.85f, 0.0f, -0.35f);
        CVector rightSide(0.85f, 0.0f, -0.35f);
        worldPositions.push_back(pVeh->TransformFromObjectSpace(leftSide));
        worldPositions.push_back(pVeh->TransformFromObjectSpace(rightSide));
    }

    if (ProperShadersMgr::IsAvailable())
    {
        if (data.hLights.size() != worldPositions.size())
        {
            for (auto& h : data.hLights)
            {
                ProperShadersMgr::DestroyLight(h);
            }
            data.hLights.clear();

            for (const auto& pos : worldPositions)
            {
                PS_LightHandle h = ProperShadersMgr::CreatePointLight(
                    pos,
                    2.8f,
                    data.color,
                    curIntensity,
                    /*bFog=*/ false
                );
                data.hLights.push_back(h);
            }
        }
        else
        {
            for (size_t i = 0; i < worldPositions.size(); i++)
            {
                ProperShadersMgr::SetEnabled(data.hLights[i], true);
                ProperShadersMgr::SetPosition(data.hLights[i], worldPositions[i]);
                ProperShadersMgr::SetColor(data.hLights[i], data.color, curIntensity);
            }
        }
    }
    else
    {
        for (const auto& pos : worldPositions)
        {
            CPointLights::AddLight(
                PLTYPE_POINTLIGHT,
                pos,
                CVector(0.0f, 0.0f, 0.0f),
                2.5f,
                (static_cast<float>(data.color.r) / 255.0f) * curIntensity,
                (static_cast<float>(data.color.g) / 255.0f) * curIntensity,
                (static_cast<float>(data.color.b) / 255.0f) * curIntensity,
                0,
                false,
                nullptr
            );
        }
    }
}
