#include "enums/materialtype.h"
#include "pch.h"
#include "lights.h"
#include "manager.h"
#include "utils/meevents.h"
#include "utils/datamgr.h"
#include "utils/car.h"
#include "ModelExtrasAPI.h"
#include "utils/samp.h"
#include "components/fog_light.h"
#include <CWeather.h>
#include <CCoronas.h>


float gfGlobalCoronaSize = 0.3f;
int gGlobalCoronaIntensity = 80;
int gGlobalShadowIntensity = 80;
bool gbLightPointLights = true;
bool gbSirenPointLights = false;

// Keep GTA's beam geometry, vertex initialization and rendering intact.
static CRGBA g_HeadLightBeamColor{255, 255, 255, 255};

static void *TransformHeadLightBeam(RwIm3DVertex *vertices, RwUInt32 count, RwMatrix *matrix, RwUInt32 flags)
{
	if (vertices && count == 5) {
		for (RwUInt32 i = 0; i < count; ++i) {
			const RwUInt32 alpha = (vertices[i].color >> 24) * g_HeadLightBeamColor.a / 255;
			RwIm3DVertexSetRGBA(&vertices[i], g_HeadLightBeamColor.r, g_HeadLightBeamColor.g, g_HeadLightBeamColor.b, alpha);
		}
	}
	return RwIm3DTransform(vertices, count, matrix, flags);
}

static void __fastcall Hooked_DoHeadLightBeam(CVehicle *pVeh, void *, int dummyId, CMatrix &matrix, unsigned char isRight)
{
	if (!pVeh || !Lights::m_bEnabled) return;
	if (LightsConfig::Get().bLightsRequireEngine && Util::IsEngineOff(pVeh)) return;
	if (!LightsConfig::Get().bHeadLightBeams) return;
	auto *mi = CModelInfo::GetModelInfo(pVeh->m_nModelIndex);
	if (!mi || !reinterpret_cast<CVehicleModelInfo *>(mi)->m_pVehicleStruct || dummyId < 0 || dummyId >= 8) return;

	const CRGBA previousColor = g_HeadLightBeamColor;
	g_HeadLightBeamColor = LightManager::GetMaterialColor(pVeh, isRight ? eMaterialType::HeadLightRight : eMaterialType::HeadLightLeft).on;
	pVeh->DoHeadLightBeam(dummyId, matrix, isRight);
	g_HeadLightBeamColor = previousColor;
}

// The native caller keeps its camera-facing gate, local position and corona ID.
static void __cdecl RegisterTailCorona(unsigned int id, CEntity *attach,
    unsigned char red, unsigned char green, unsigned char blue, unsigned char alpha,
    const CVector &pos, float radius, float farClip, eCoronaType type, eCoronaFlareType flare,
    bool reflection, bool obstacles, int unused, float angle, bool longDistance,
    float nearClip, unsigned char fadeState, float fadeSpeed, bool onlyFromBelow, bool reflectionDelay)
{
    auto &cfg = LightsConfig::Get();
    if (cfg.bLegacyTailCoronas && Lights::m_bEnabled && cfg.gbLightCoronasFeature) {
        auto *veh = static_cast<CVehicle *>(attach); // Only CVehicle::DoTailLightEffect calls this hook.
        if (veh && CPools::ms_pVehiclePool && CPools::ms_pVehiclePool->IsObjectValid(veh) && veh->m_pRwClump
            && veh->m_fHealth > 0.0f && (!cfg.bLightsRequireEngine || !Util::IsEngineOff(veh))) {
            auto &data = LightManager::m_VehData.Get(veh);
            const bool left = pos.x <= 0.0f;
            const bool right = pos.x >= 0.0f;
            const bool owned = (left && LightManager::IsDummyAvailable(data, {eMaterialType::TailLightLeft,
                eMaterialType::BrakeLightLeft, eMaterialType::STTLightLeft, eMaterialType::NABrakeLightLeft}))
                || (right && LightManager::IsDummyAvailable(data, {eMaterialType::TailLightRight,
                    eMaterialType::BrakeLightRight, eMaterialType::STTLightRight, eMaterialType::NABrakeLightRight}));
            if (!owned && (!left || data.bLightStates[eMaterialType::TailLightLeft])
                && (!right || data.bLightStates[eMaterialType::TailLightRight])) {
                const auto brakeTypes = {eMaterialType::BrakeLightLeft, eMaterialType::BrakeLightRight,
                    eMaterialType::STTLightLeft, eMaterialType::STTLightRight,
                    eMaterialType::NABrakeLightLeft, eMaterialType::NABrakeLightRight};
                const bool dedicatedBrake = LightManager::IsDummyAvailable(data, brakeTypes)
                    || LightManager::IsMaterialAvailable(veh, brakeTypes);
                red = red && (!dedicatedBrake || CarUtil::AreLightsOn(veh)) ? 100 : 0;
                alpha = !dedicatedBrake && veh->m_fBreakPedal > 0.0f && veh->m_pDriver && !veh->bIsHandbrakeOn ? 200 : 120;
                radius = static_cast<float>(static_cast<double>(radius) * 1.5
                    + static_cast<double>(CWeather::Foggyness) * 1.7999999523162842);
            }
        }
    }
    CCoronas::RegisterCorona(id, attach, red, green, blue, alpha, pos, radius, farClip, type, flare,
        reflection, obstacles, unused, angle, longDistance, nearClip, fadeState, fadeSpeed, onlyFromBelow, reflectionDelay);
}

void Lights::Init() {
    ReloadConfig();
    if (!m_bEnabled) {
        return;
    }

    LightManager::Init();

    patch::Nop(0x6E2722, 19);	  // CVehicle::DoHeadLightReflection
	patch::SetUChar(0x6E1A22, 0); // CVehicle::DoTailLightEffect
    patch::ReplaceFunctionCall(0x6E1A2D, reinterpret_cast<void *>(RegisterTailCorona));

	// CVehicle::DoHeadLightEffect
	patch::SetUChar(0x6E0CF8, 0);
	patch::SetUChar(0x6E0DEE, 0);

	// CVehicle::DoVehicleLights (native headlight coronas)
	patch::SetUChar(0x6E2193, 0);
	patch::SetUChar(0x6E228B, 0);
	patch::SetUChar(0x6E2532, 0);
	patch::SetUChar(0x6E2627, 0);

	// Disable native hardcoded white pointlights in CVehicle::DoVehicleLights
	patch::Nop(0x6E27E6, 5);
	patch::Nop(0x6E28E7, 5);

	SAMP::PatchVehicleLights();

	// Redirect native automobile/bike beam calls; keep DoHeadLightBeam intact (SA 1.0 US).
	patch::ReplaceFunctionCall(0x6A2EDA, reinterpret_cast<void *>(Hooked_DoHeadLightBeam));
	patch::ReplaceFunctionCall(0x6A2EF2, reinterpret_cast<void *>(Hooked_DoHeadLightBeam));
	patch::ReplaceFunctionCall(0x6BDE80, reinterpret_cast<void *>(Hooked_DoHeadLightBeam));
	patch::ReplaceFunctionCall(0x6E13AE, reinterpret_cast<void *>(TransformHeadLightBeam));

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
        if (!m_bEnabled) return MatStateColor{DEFAULT_MAT_COL, DEFAULT_MAT_COL};
        return LightManager::GetMaterialColor(pVeh, type);
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
    FogLightComponent::UpdateLegacyWeather();
    BlinkerState::Get().Update();
}

void Lights::ProcessVehicle(CVehicle* pVeh) {
    if (!m_bEnabled) return;
    LightManager::Process(pVeh);
}
