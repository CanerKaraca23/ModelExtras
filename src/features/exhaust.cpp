#include "pch.h"
#include "exhausts.h"
#include <CWorld.h>
#include <CAutomobile.h>
#include <CCamera.h>
#include <CGeneral.h>
#include <CWaterLevel.h>
#include <CParticles.h>
#include "utils/car.h"

#include "utils/modelinfomgr.h"
#include "utils/datamgr.h"
#include "ModelExtrasAPI.h"
#include "backfire.h"
#include "enums/vehdummy.h"
#include "utils/meevents.h"
#include <CPointLights.h>
#include <rwcore.h>
#include <rwplcore.h>

#define NODE_NAME "x_exhaust"

// Global trampolines
ExhaustFn_t ogFunc1 = nullptr, ogFunc2 = nullptr;

void __fastcall ExhaustFx::hkAddExhaustParticles1(CVehicle * pVeh)
{
    if (!pVeh) return;
    if (!CBaseFeature::IsEnabled(eFeatureMatrix::ExhaustFx)) {
        if (ogFunc1) ogFunc1(pVeh);
        return;
    }
    auto &data = m_VehData.Get(pVeh);
    if (!data.isUsed && ogFunc1) {
        ogFunc1(pVeh);
    }
}

void __fastcall ExhaustFx::hkAddExhaustParticles2(CVehicle *pVeh)
{
    if (!pVeh) return;
    if (!CBaseFeature::IsEnabled(eFeatureMatrix::ExhaustFx)) {
        if (ogFunc2) ogFunc2(pVeh);
        return;
    }
    auto &data = m_VehData.Get(pVeh);
    if (!data.isUsed && ogFunc2) {
        ogFunc2(pVeh);
    }
}

void ExhaustFx::FindNodes(CVehicle *pVeh, RwFrame *pFrame)
{
    if (pFrame)
    {
        std::string_view name = GetSafeFrameNodeName(pFrame);
        if (name.starts_with(NODE_NAME))
        {
            auto &data = m_VehData.Get(pVeh);
            data.isUsed = true;
            data.m_pDummies.emplace_back(std::string(name), LoadData(pVeh, pFrame));
        }

        if (RwFrame *newFrame = pFrame->child)
        {
            FindNodes(pVeh, newFrame);
        }
        if (RwFrame *newFrame = pFrame->next)
        {
            FindNodes(pVeh, newFrame);
        }
    }
    return;
}

void ExhaustFx::Init()
{
    bEnabled = true;

    ModelInfoMgr::RegisterRender([](CVehicle *pVeh)
    {
        if (!pVeh || !pVeh->GetIsOnScreen()) {
            return;
        }

        ExhaustVehData &data = m_VehData.Get(pVeh);

        // Must be here to work with VehFuncs recursive extras
        if (!data.bNodesSearched) {
            data.bNodesSearched = true;
            if (pVeh->m_pRwClump && pVeh->m_pRwClump->object.parent) {
                RwFrame *pFrame = (RwFrame*)pVeh->m_pRwClump->object.parent;
                FindNodes(pVeh, pFrame);
            }
        }

        for (auto& e : data.m_pDummies) {
            RenderSmokeFx(pVeh, e.second);
        }
    });

    MEEvents::vehPreRenderEvent.before += [](CVehicle *pVeh)
    {
        ExhaustFx::ProcessPointLights(pVeh);
    };

    // Hook VC 1.0 CAutomobile::AddExhaustParticles (0x589570) and CBike::AddExhaustParticles (0x60E890)
    ogFunc1 = injector::GetBranchDestination(0x589570, true).get();
    injector::MakeCALL(0x589570, hkAddExhaustParticles1, true);

    ogFunc2 = injector::GetBranchDestination(0x60E890, true).get();
    injector::MakeCALL(0x60E890, hkAddExhaustParticles2, true);
}

void ExhaustFx::ProcessPointLights(CVehicle *pVeh)
{
    extern bool gbLightPointLights;
    if (!gbLightPointLights || !pVeh || !pVeh->GetIsOnScreen() || !pVeh->bEngineOn || CarUtil::IsEngineBroken(pVeh))
    {
        return;
    }

    ExhaustVehData &data = m_VehData.Get(pVeh);
    bool bNitroActive = (data.lastNitroFrame == CTimer::m_FrameCounter) ||
                        (CTimer::m_FrameCounter > 0 && data.lastNitroFrame == CTimer::m_FrameCounter - 1);

    if (!bNitroActive || pVeh->m_fGasPedal <= 0.05f)
    {
        return;
    }

    pVeh->UpdateRwFrame();

    float nitroScale = std::clamp(pVeh->m_fGasPedal, 0.5f, 1.0f);
    const float radius = 0.70f * nitroScale;
    const float r = 0.0f;
    const float g = 0.45f * nitroScale;
    const float b = 1.0f;
    const float rearOffset = 0.25f;

    if (data.isUsed && !data.m_pDummies.empty())
    {
        for (const auto &e : data.m_pDummies)
        {
            if (e.second.bNitroEffect && e.second.pFrame)
            {
                CVector pos = e.second.pFrame->ltm.pos;
                if (!pos.IsZero())
                {
                    CVector backwardDir = -(CVector &)e.second.pFrame->ltm.up;
                    backwardDir.Normalize();
                    CVector plightPos = pos + backwardDir * rearOffset;
                    CPointLights::AddLight(PLTYPE_POINTLIGHT, plightPos, CVector(0.0f, 0.0f, 0.0f), radius, r, g, b, 0, false);
                }
            }
        }
    }
    else
    {
        CVehicleModelInfo *pInfo = static_cast<CVehicleModelInfo *>(CModelInfo::GetModelInfo(pVeh->m_nModelIndex));
        if (pInfo)
        {
            CVector pos = pInfo->m_avDummyPos[2];
            if (!pos.IsZero())
            {
                CVector worldPos = pVeh->TransformFromObjectSpace(CVector(pos.x, pos.y - rearOffset, pos.z));
                CPointLights::AddLight(PLTYPE_POINTLIGHT, worldPos, CVector(0.0f, 0.0f, 0.0f), radius, r, g, b, 0, false);

                if (CarUtil::HasDoubleExhaust(pVeh))
                {
                    CVector doubleWorldPos = pVeh->TransformFromObjectSpace(CVector(-pos.x, pos.y - rearOffset, pos.z));
                    CPointLights::AddLight(PLTYPE_POINTLIGHT, doubleWorldPos, CVector(0.0f, 0.0f, 0.0f), radius, r, g, b, 0, false);
                }
            }
        }
    }
}
ExhaustData ExhaustFx::LoadData(CVehicle *pVeh, RwFrame *pFrame)
{
    ExhaustData f;
    f.sName = GetSafeFrameNodeName(pFrame);
    f.pFrame = pFrame;
    f.bNitroEffect = true;

    auto &jsonData = DataMgr::Get(pVeh->m_nModelIndex);
    if (jsonData.contains("exhausts") && jsonData["exhausts"].contains(f.sName))
    {
        auto &data = jsonData["exhausts"][f.sName];
        f.fLifeTime = data.value("lifetime", f.fLifeTime);
        f.fSpeedMul *= data.value("speed", 1.0f);
        f.fSizeMul = data.value("size", f.fSizeMul);
        f.bNitroEffect = data.value("nitro_effect", f.bNitroEffect);

        if (data.contains("color"))
        {
            f.Color.r = data["color"].value("red", f.Color.r);
            f.Color.g = data["color"].value("green", f.Color.g);
            f.Color.b = data["color"].value("blue", f.Color.b);
            f.Color.a = data["color"].value("alpha", f.Color.a);
        }
    }

    return f;
}

void ExhaustFx::RenderSmokeFx(CVehicle *pVeh, const ExhaustData &info)
{
    if (!CBaseFeature::IsEnabled(eFeatureMatrix::ExhaustFx))
    {
        return;
    }
    if (!pVeh || !pVeh->GetIsOnScreen() || !pVeh->bEngineOn || CarUtil::IsEngineBroken(pVeh))
    {
        return;
    }

    float dist = CVector::Distance(pVeh->GetPosition(), TheCamera.GetPosition());
    dist *= dist;

    if (dist > 256.0f || (dist > 64.0f && !((CTimer::m_FrameCounter + pVeh->m_nModelIndex) & 1)))
    {
        return;
    }

    CVector exhaustPos = info.pFrame->ltm.pos;
    if (exhaustPos.IsZero())
    {
        return;
    }

    auto &data = m_VehData.Get(pVeh);
    if (data.reloadCount < nReloadCount)
    {
        for (auto &e : data.m_pDummies)
        {
            e.second = LoadData(pVeh, e.second.pFrame);
        }
        data.reloadCount++;
    }

    CVector particleDir = info.pFrame->ltm.up;
    particleDir *= -1;

    CVector parVelocity;
    if (CVector::Dot(particleDir, pVeh->m_vecMoveSpeed) >= 0.05f)
    {
        parVelocity = pVeh->m_vecMoveSpeed * 30.0f;
    }
    else
    {
        static float randomFactor = CGeneral::GetRandomNumberInRange(-1.8f, -0.9f);
        parVelocity = randomFactor * particleDir;
    }

    float randomFactor = CGeneral::GetRandomNumberInRange(1.0f, 3.0f);
    if (randomFactor * (pVeh->m_fGasPedal + 1.1f) <= 2.5f)
    {
        return;
    }

    for (int i = 0; i < 2; i++)
    {
        CParticles::AddParticle(
            PARTICLE_EXHAUST_STEAM,
            exhaustPos,
            parVelocity,
            nullptr,
            0.2f * info.fSizeMul,
            nullptr,
            0,
            0,
            0,
            0);
    }
}

void ExhaustFx::RenderNitroFx(CVehicle *pVeh, float power)
{
}

void ExhaustFx::Reload(CVehicle* pVeh)
{
    if (pVeh) {
        auto &data = m_VehData.Get(pVeh);
        data.bNodesSearched = false;
        data.isUsed = false;
        data.m_pDummies.clear();
    }
    nReloadCount++;
}

#ifdef __cplusplus
extern "C"
{
#endif
    unsigned int ME_GetExhaustCount(CVehicle *pVeh)
    {
        if (!pVeh)
            return 0;

        ExhaustVehData &data = ExhaustFx::m_VehData.Get(pVeh);
        if (!data.isUsed)
            return 0;

        return static_cast<unsigned int>(data.m_pDummies.size());
    }

    ME_ExhaustInfo ME_GetExhaustData(CVehicle *pVeh, int index)
    {
        ME_ExhaustInfo info{};
        if (!pVeh)
            return info;

        ExhaustVehData &data = ExhaustFx::m_VehData.Get(pVeh);
        if (!data.isUsed || index < 0 || index >= static_cast<int>(data.m_pDummies.size()))
            return info;

        const ExhaustData &e = data.m_pDummies[index].second;
        info.pFrame = e.pFrame;
        info.Color = { e.Color.r, e.Color.g, e.Color.b, e.Color.a };
        info.fSpeedMul = e.fSpeedMul;
        info.fLifeTime = e.fLifeTime;
        info.fSizeMul = e.fSizeMul;
        info.bNitroEffect = e.bNitroEffect;

        return info;
    }

    void ME_SetExhaustData(CVehicle *pVeh, int index, ME_ExhaustInfo &data)
    {
        if (!pVeh)
            return;

        ExhaustVehData &vData = ExhaustFx::m_VehData.Get(pVeh);
        if (!vData.isUsed || index < 0 || index >= static_cast<int>(vData.m_pDummies.size()))
            return;

        ExhaustData &e = vData.m_pDummies[index].second;
        e.pFrame = data.pFrame;
        e.Color = CRGBA(data.Color.r, data.Color.g, data.Color.b, data.Color.a);
        e.fSpeedMul = data.fSpeedMul;
        e.fLifeTime = data.fLifeTime;
        e.fSizeMul = data.fSizeMul;
        e.bNitroEffect = data.bNitroEffect;
    }

    // Dummy function to show on crash logs
    int __declspec(dllexport) ignore3(int i)
    {
        return 1;
    }

#ifdef __cplusplus
}
#endif