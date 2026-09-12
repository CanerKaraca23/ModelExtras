#include "pch.h"
#include "defines.h"
#include "backfire.h"
#include "utils/datamgr.h"
#include "utils/audiomgr.h"
#include "enums/vehdummy.h"
#include <CCamera.h>
#include <CParticle.h>
#include "ModelExtrasAPI.h"

using namespace plugin;

void BackFireEffect::BackFireFX(CVehicle *pVeh, float x, float y, float z, float dirX, float dirY, float dirZ)
{
    CVector exhaustLocalPos(x, y, z);
    CVector exhaustWorldPos = pVeh->TransformFromObjectSpace(exhaustLocalPos);
    CVector dir = pVeh->TransformFromObjectSpace(CVector(dirX, dirY, dirZ)) - pVeh->GetPosition();
    dir.Normalize();

    CParticle::AddParticle(PARTICLE_GUNFLASH, exhaustWorldPos, dir * 0.2f, pVeh, 0.4f, 0, 0, 0, 0);
    CParticle::AddParticle(PARTICLE_CARFLAME, exhaustWorldPos, dir * 0.1f, pVeh, 0.3f, 0, 0, 0, 0);

    static std::string audioPath = MOD_DATA_PATH("audio/backfire.wav");
    AudioMgr::Play3DSound(audioPath, exhaustWorldPos, pVeh, 1.5f, 80.0f);
}

void BackFireEffect::BackFireSingle(CVehicle *pVeh)
{

    size_t count = ME_GetExhaustCount(pVeh);
    if (count <= 0)
    {
        CVehicleModelInfo *pInfo = static_cast<CVehicleModelInfo *>(CModelInfo::GetModelInfo(pVeh->m_nModelIndex));
        if (!pInfo) return;
        CVector pos = pInfo->m_avDummyPos[2];
        if (!pos.IsZero())
        {
            if (CarUtil::HasDoubleExhaust(pVeh))
            {
                float vx = pos.x * -1.0f;
                BackFireFX(pVeh, vx, pos.y, pos.z);
            }
            BackFireFX(pVeh, pos.x, pos.y, pos.z);
        }
    }
    else
    {
        for (size_t i = 0; i < count; i++)
        {
            const ME_ExhaustInfo &info = ME_GetExhaustData(pVeh, static_cast<int>(i));
            if (info.pFrame)
            {
                CVector f = info.pFrame->modelling.up; // Up is Forward
                BackFireFX(pVeh, info.pFrame->modelling.pos.x, info.pFrame->modelling.pos.y, info.pFrame->modelling.pos.z, f.x * 1.5f, f.y * 1.5f, f.z * 1.5f);
            }
        }
    }
}

void BackFireEffect::BackFireMulti(CVehicle *pVeh)
{
    int num = RandomNumberInRange(0, 3) - 1;

    BackFireSingle(pVeh);
    BackfireData &data = m_VehData.Get(pVeh);
    if (num > 0)
    {
        data.m_nleftFires = num;
    }
    else
    {
        data.m_nleftFires = 0;
    }
}

std::vector<int> ValidModels = {};
bool onlySelected = false;

void BackFireEffect::ReloadConfig()
{
    CBaseFeature::ReloadConfig();
    std::string line = gConfig.ReadString("TABLE", "BackFireEffect_VehicleModels", "");
    onlySelected = gConfig.ReadBoolean("FEATURES", "BackfireEffect_OnlySelectedModels", true);
    ValidModels.clear();
    Util::GetModelsFromIni(line, ValidModels);
}

void BackFireEffect::Init()
{
    ReloadConfig();

    Events::initGameEvent += [this]()
    {
        ReloadConfig();
    };

    Events::vehicleRenderEvent.before += [](CVehicle *vehicle)
    {
        BackFireEffect::Process(vehicle);
    };
}

// Inspired by Junior's https://www.mixmods.com.br/2016/06/backfire-als-v2-5-mod-estalar-escapamento/
void BackFireEffect::Process(CVehicle *pVeh)
{
    if (!CBaseFeature::IsEnabled(eFeatureMatrix::BackfireEffect))
    {
        return;
    }

    if (!pVeh->GetIsOnScreen() || CarUtil::IsEngineBroken(pVeh) || !pVeh->bEngineOn || pVeh->bIsBig || pVeh->bIsVan || pVeh->bIsBus || CarUtil::IsRCVehicle(pVeh))
    {
        return;
    }

    bool isValidVeh = std::find(ValidModels.begin(), ValidModels.end(), pVeh->m_nModelIndex) != ValidModels.end();

    if (!isValidVeh && onlySelected)
    {
        return;
    }

    if (pVeh->m_nCurrentGear == 0)
    {
        return;
    }

    BackfireData &data = m_VehData.Get(pVeh);

    if (CarUtil::IsBike(pVeh) || CarUtil::IsAutomobile(pVeh))
    {
        float speed = Util::GetVehicleSpeed(pVeh);
        float throttle = std::abs(pVeh->m_fGasPedal);

        // handle multi
        size_t timer = CTimer::m_snTimeInMilliseconds;

        if (data.wasFullThrottled)
        {
            if (throttle < 0.78f)
            {
                BackFireMulti(pVeh);
                data.wasFullThrottled = false;
            }
        }
        else
        {
            if (throttle >= 0.99f)
            {
                data.wasFullThrottled = true;
            }
        }

        if (timer - data.prevTimer > 200)
        {
            if (data.m_nleftFires > 0)
            {
                BackFireSingle(pVeh);
                data.m_nleftFires--;
            }
            data.prevTimer = timer;
        }
    }
}