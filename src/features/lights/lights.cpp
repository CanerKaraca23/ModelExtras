#include "enums/materialtype.h"
#include "pch.h"
#include "lights.h"
#include "manager.h"
#include "utils/meevents.h"
#include "utils/datamgr.h"
#include "ModelExtrasAPI.h"
#include "utils/samp.h"

float gfGlobalCoronaSize = 0.3f;
int gGlobalCoronaIntensity = 80;
int gGlobalShadowIntensity = 80;
bool gbLightPointLights = true;
bool gbSirenPointLights = false;

void Lights::Init() {
    ReloadConfig();
    if (!m_bEnabled) {
        return;
    }

    LightManager::Init();

    patch::Nop(0x6E2722, 19);	  // CVehicle::DoHeadLightReflection
	patch::SetUChar(0x6E1A22, 0); // CVehicle::DoTailLightEffect

	// CVehicle::DoHeadLightEffect
	patch::SetUChar(0x6E0CF8, 0);
	patch::SetUChar(0x6E0DEE, 0);

	// CVehicle::DoVehicleLights (native headlight coronas)
	patch::SetUChar(0x6E2193, 0);
	patch::SetUChar(0x6E228B, 0);
	patch::SetUChar(0x6E2532, 0);
	patch::SetUChar(0x6E2627, 0);

	SAMP::PatchVehicleLights();

	// NOP CVehicle::DoHeadLightBeam
	if (!gConfig.ReadBoolean("LIGHTS", "HeadLightBeams", gConfig.ReadBoolean("TWEAKS", "HeadLightBeams", true)))
	{
		// cmp ax, ax
		patch::SetRaw(0x6A2EA5, (void *)"\x66\x39\xC0\x90", 4);
		patch::SetRaw(0x6BDE63, (void *)"\x66\x39\xC0\x90\x90\x90\x90", 7);
	}

	Events::initGameEvent += []()
	{
		LightsConfig::Get().InitConfig();
	};

    ModelInfoMgr::RegisterMaterial([](CVehicle *pVeh, RpMaterial *pMat) {
        if (!m_bEnabled) return eMaterialType::UnknownMaterial;
        return LightManager::GetMatType(pMat); 
    });

	ModelInfoMgr::RegisterDummy([](CVehicle *pVeh, RwFrame *pFrame, const std::string_view nodeName) {
        LightManager::RegisterDummy(pVeh, pFrame, nodeName);
    });

    ModelInfoMgr::RegisterMaterialColProvider([](CVehicle *pVeh, RpMaterial *pMat, eMaterialType type) -> MatStateColor {
        if (!m_bEnabled || !pVeh || type < 0 || type >= eMaterialType::TotalMaterial) {
            return MatStateColor{DEFAULT_MAT_COL, DEFAULT_MAT_COL};
        }
        VehLightData &data = LightManager::m_VehData.Get(pVeh);
        if (LightManager::IsDummyAvailable(data, type)) {
            const DummyConfig &c = data.dummies[type][0]->GetRef();
            if (c.hasCustomColor) {
                return MatStateColor{c.corona.color, DEFAULT_MAT_COL};
            }
        }

        auto &json = DataMgr::Get(pVeh->m_nModelIndex);
        if (json.contains("lights")) {
            auto &lights = json["lights"];
            auto CheckCol = [&](const char *key) -> std::optional<CRGBA> {
                if (lights.contains(key) && lights[key].contains("corona") && lights[key]["corona"].contains("color")) {
                    auto &c = lights[key]["corona"]["color"];
                    CRGBA col;
                    col.r = c.value("red", 255);
                    col.g = c.value("green", 255);
                    col.b = c.value("blue", 255);
                    col.a = 255;
                    return col;
                }
                return std::nullopt;
            };

            auto CheckOffCol = [&](const char *key) -> std::optional<CRGBA> {
                if (!lights.contains(key)) return std::nullopt;
                const nlohmann::json *pSec = &lights[key];
                if (pSec->contains("material") && (*pSec)["material"].contains("color_off")) {
                    pSec = &(*pSec)["material"]["color_off"];
                } else if (pSec->contains("color_off")) {
                    pSec = &(*pSec)["color_off"];
                } else {
                    return std::nullopt;
                }
                CRGBA off;
                off.r = pSec->value("red", 255);
                off.g = pSec->value("green", 255);
                off.b = pSec->value("blue", 255);
                off.a = 255;
                return off;
            };

            std::optional<CRGBA> col;
            std::optional<CRGBA> offCol;
            auto ResolveLightCol = [&](const char *specific, const char *generic = nullptr, const char *alt = nullptr) {
                col = CheckCol(specific);
                offCol = CheckOffCol(specific);
                if (alt) {
                    if (!col) col = CheckCol(alt);
                    if (!offCol) offCol = CheckOffCol(alt);
                }
                if (generic) {
                    if (!col) col = CheckCol(generic);
                    if (!offCol) offCol = CheckOffCol(generic);
                }
            };

            switch (type) {
            case eMaterialType::HeadLightLeft: ResolveLightCol("headlight_l", "headlights"); break;
            case eMaterialType::HeadLightRight: ResolveLightCol("headlight_r", "headlights"); break;
            case eMaterialType::TailLightLeft: ResolveLightCol("taillight_l", "taillights"); break;
            case eMaterialType::TailLightRight: ResolveLightCol("taillight_r", "taillights"); break;
            case eMaterialType::BrakeLightLeft: case eMaterialType::NABrakeLightLeft: ResolveLightCol("brakelight_l", "brakelights"); break;
            case eMaterialType::BrakeLightRight: case eMaterialType::NABrakeLightRight: ResolveLightCol("brakelight_r", "brakelights"); break;
            case eMaterialType::ReverseLightLeft: ResolveLightCol("reverselight_l", "reverselights"); break;
            case eMaterialType::ReverseLightRight: ResolveLightCol("reverselight_r", "reverselights"); break;
            case eMaterialType::IndicatorLightLeftFront: ResolveLightCol("indicator_lf", "indicators"); break;
            case eMaterialType::IndicatorLightRightFront: ResolveLightCol("indicator_rf", "indicators"); break;
            case eMaterialType::IndicatorLightLeftRear: ResolveLightCol("indicator_lr", "indicators"); break;
            case eMaterialType::IndicatorLightRightRear: ResolveLightCol("indicator_rr", "indicators"); break;
            case eMaterialType::IndicatorLightLeftMiddle: ResolveLightCol("indicator_lm", "indicators"); break;
            case eMaterialType::IndicatorLightRightMiddle: ResolveLightCol("indicator_rm", "indicators"); break;
            case eMaterialType::FogLightLeft: ResolveLightCol("foglight_l", "foglights", "fogl_l"); break;
            case eMaterialType::FogLightRight: ResolveLightCol("foglight_r", "foglights", "fogl_r"); break;
            default: break;
            }
            if (col || offCol) {
                return MatStateColor{col.value_or(DEFAULT_MAT_COL), offCol.value_or(DEFAULT_MAT_COL)};
            }
        }

        return MatStateColor{DEFAULT_MAT_COL, DEFAULT_MAT_COL};
    });

	MEEvents::vehPreRenderEvent.before += [](CVehicle *pVeh)
	{
		if (!m_bEnabled) return;
		LightManager::ProcessPointLights(pVeh);
	};



	ModelInfoMgr::RegisterRender([](CVehicle *pControlVeh) {
		if (!m_bEnabled) return;
		int model = pControlVeh->m_nModelIndex;

		if (CModelInfo::IsTrailerModel(model)) {
			return;
		}

		CVehicle *pTowedVeh = pControlVeh;
		if (pControlVeh->m_pTrailer) {
			pTowedVeh = pControlVeh->m_pTrailer;
		}

		LightManager::Render(pControlVeh, pTowedVeh);
	});
}

void Lights::ReloadConfig() {
	CBaseFeature::ReloadConfig();
	if (!m_bActive) {
		m_bActive = gConfig.ReadBoolean("LIGHTS", "StandardLightsv2", gConfig.ReadBoolean("FEATURES", "StandardLightsv2", false));
	}
	m_bEnabled = m_bActive;
	LightsConfig::Get().InitConfig();
}

void Lights::Reload(CVehicle* pVeh) {
	ReloadConfig();
	LightManager::Reload(pVeh);
}

VehLightData& Lights::GetVehicleData(CVehicle* pVeh) {
    return LightManager::m_VehData.Get(pVeh);
}

bool Lights::IsIndicatorOn(CVehicle* pVeh) {
    return LightManager::IsIndicatorOn(pVeh);
}

bool Lights::GetLightState(CVehicle* pVeh, eMaterialType lightId) {
    return LightManager::GetLightState(pVeh, lightId);
}

void Lights::SetLightState(CVehicle* pVeh, eMaterialType lightId, bool state) {
    LightManager::SetLightState(pVeh, lightId, state);
}

extern "C"
{
	ME_WRAPPER bool ME_GetVehicleLightState(CVehicle *pVeh, ME_LightID lightId)
	{
		return LightManager::GetLightState(pVeh, static_cast<eMaterialType>(lightId));
	}

	ME_WRAPPER void ME_SetVehicleLightState(CVehicle *pVeh, ME_LightID lightId, bool state)
	{
		LightManager::SetLightState(pVeh, static_cast<eMaterialType>(lightId), state);
	}

	// Dummy function to show on crash logs
	int __declspec(dllexport) ignore4(int i)
	{
		return 1;
	}
}

void Lights::ProcessTick() {
    if (!m_bEnabled) return;
    BlinkerState::Get().Update();
}

void Lights::ProcessVehicle(CVehicle* pVeh) {
    if (!m_bEnabled) return;
    LightManager::Process(pVeh);
}

void Lights::ProcessBikePointLights(CVehicle* pVeh) {
    if (!m_bEnabled) return;
    LightManager::ProcessPointLights(pVeh);
}
