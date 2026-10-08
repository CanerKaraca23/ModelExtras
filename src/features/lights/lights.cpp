#include "enums/materialtype.h"
#include "pch.h"
#include "lights.h"
#include "manager.h"
#include "utils/meevents.h"
#include "utils/datamgr.h"
#include "utils/car.h"
#include "utils/render.h"
#include "utils/texmgr.h"
#include "ModelExtrasAPI.h"
#include "utils/samp.h"
#include "components/fog_light.h"
#include "components/headlight.h"
#include <CWeather.h>
#include <CCoronas.h>
#include <CPointLights.h>


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

static void __cdecl RegisterNativeHeadPointLight(CVehicle *veh, unsigned char type, CVector point, CVector direction,
    float radius, float red, float green, float blue, unsigned char fog, bool shadows, CEntity *affected)
{
    const auto &cfg = LightsConfig::Get();
    if (!cfg.bLegacyHeadPointLights || !Lights::m_bEnabled || !gbLightPointLights || !veh
        || !CPools::ms_pVehiclePool || !CPools::ms_pVehiclePool->IsObjectValid(veh) || !veh->m_pRwClump
        || !(veh->m_fHealth > 0.0f) || (cfg.bLightsRequireEngine && Util::IsEngineOff(veh))) return;
    if (veh->m_nModelIndex < 0) return;
    auto *model = static_cast<CVehicleModelInfo *>(CModelInfo::GetModelInfo(veh->m_nModelIndex));
    if (!model || !model->m_pVehicleStruct) return;
    const auto family = veh->m_nVehicleSubClass;
    if (family == VEHICLE_BMX || family == VEHICLE_BOAT || family == VEHICLE_TRAILER || family == VEHICLE_HELI || family == VEHICLE_PLANE
        || CModelInfo::IsBmxModel(veh->m_nModelIndex) || CModelInfo::IsBoatModel(veh->m_nModelIndex)
        || CModelInfo::IsTrailerModel(veh->m_nModelIndex) || CModelInfo::IsHeliModel(veh->m_nModelIndex)
        || CModelInfo::IsPlaneModel(veh->m_nModelIndex)) return;
    auto &data = LightManager::m_VehData.Get(veh);
    if (!HeadlightComponent::AreHeadlightsOpen(veh, data)) return;
    const bool left = data.bLightStates[eMaterialType::HeadLightLeft]
        && !LightManager::IsDummyAvailable(data, {eMaterialType::HeadLightLeft, eMaterialType::HighBeamLeft})
        && !Util::IsLightDamaged(veh, eLights::LIGHT_FRONT_LEFT) && !Util::IsPanelDamaged(veh, ePanels::WING_FRONT_LEFT);
    const bool right = data.bLightStates[eMaterialType::HeadLightRight]
        && !LightManager::IsDummyAvailable(data, {eMaterialType::HeadLightRight, eMaterialType::HighBeamRight})
        && !Util::IsLightDamaged(veh, eLights::LIGHT_FRONT_RIGHT) && !Util::IsPanelDamaged(veh, ePanels::WING_FRONT_RIGHT);
    if (!left && !right) return;
    if (data.bLongLightsOn) radius = cfg.bLegacyHighBeamRange ? 35.0f : radius * cfg.fHighBeamPointLightMul;
    auto color = [&](eMaterialType type) {
        return DataMgr::Find(veh->m_nModelIndex) ? LightManager::GetMaterialColor(veh, type).on : DEFAULT_MAT_COL;
    };
    const CRGBA a = left ? color(eMaterialType::HeadLightLeft) : DEFAULT_MAT_COL;
    const CRGBA b = right ? color(eMaterialType::HeadLightRight) : DEFAULT_MAT_COL;
    auto emit = [&](CVector position, CRGBA tint) {
        if (!std::isfinite(radius) || radius <= 0.0f || !std::isfinite(position.x) || !std::isfinite(position.y)
            || !std::isfinite(position.z) || !std::isfinite(direction.x) || !std::isfinite(direction.y) || !std::isfinite(direction.z)) return;
        const float intensity = cfg.fPointLightIntensity * (tint.a / 255.0f);
        CPointLights::AddLight(type, position, direction, radius, red * (tint.r / 255.0f) * intensity,
            green * (tint.g / 255.0f) * intensity, blue * (tint.b / 255.0f) * intensity, fog, shadows, affected);
    };
    if (left && right && a == b) {
        emit(point, a);
    } else {
        CVector dummy = model->m_pVehicleStruct->m_avDummyPos[0];
        if (!std::isfinite(dummy.x) || !std::isfinite(dummy.y) || !std::isfinite(dummy.z)) return;
        const float x = std::abs(dummy.x);
        if (left) { dummy.x = -x; emit(veh->TransformFromObjectSpace(dummy), a); }
        if (right) { dummy.x = x; emit(veh->TransformFromObjectSpace(dummy), b); }
    }
}

// The original caller retains its 14-word AddLight stack; ESI is its vehicle.
static void __declspec(naked) NativeHeadPointLightBridge()
{
    __asm {
        push ebp
        mov ebp, esp
        lea edx, [ebp + 60]
        mov ecx, 14
    copyArgument:
        push dword ptr [edx]
        sub edx, 4
        loop copyArgument
        push esi
        call RegisterNativeHeadPointLight
        add esp, 60
        pop ebp
        ret
    }
}

static RwTexture *g_LegacyHeadShadowTextures[2]{};
static bool g_LegacyHeadShadowReady = false;

// Borrow textures outside rendering; TextureMgr retains their ownership.
static void PrepareLegacyHeadShadows()
{
    if (!g_LegacyHeadShadowReady) return;
    if (LightsConfig::Get().bLegacyHeadShadows) {
        g_LegacyHeadShadowTextures[0] = TextureMgr::Get("headlight_short");
        g_LegacyHeadShadowTextures[1] = TextureMgr::Get("headlight_long");
        RenderUtil::ReloadConfig();
    } else {
        g_LegacyHeadShadowTextures[0] = g_LegacyHeadShadowTextures[1] = nullptr;
    }
}

static void __fastcall Hooked_DoHeadLightReflection(CVehicle *veh, void *, CMatrix &matrix,
    unsigned int flags, unsigned char first, unsigned char second)
{
    const auto &cfg = LightsConfig::Get();
    if (!cfg.bLegacyHeadShadows || !Lights::m_bEnabled || !g_LegacyHeadShadowReady
        || !veh || !CPools::ms_pVehiclePool || !CPools::ms_pVehiclePool->IsObjectValid(veh)
        || !veh->m_pRwClump || !(veh->m_fHealth > 0.0f) || (cfg.bLightsRequireEngine && Util::IsEngineOff(veh))) return;
    const int modelId = veh->m_nModelIndex;
    const auto family = veh->m_nVehicleSubClass;
    if (family == VEHICLE_BMX || family == VEHICLE_BOAT || family == VEHICLE_TRAILER
        || family == VEHICLE_HELI || family == VEHICLE_PLANE) return;
    if (CModelInfo::IsBmxModel(modelId) || CModelInfo::IsBoatModel(modelId) || CModelInfo::IsTrailerModel(modelId)
        || CModelInfo::IsHeliModel(modelId) || CModelInfo::IsPlaneModel(modelId)) return;
    auto *model = static_cast<CVehicleModelInfo *>(CModelInfo::GetModelInfo(modelId));
    if (!model || !model->m_pVehicleStruct) return;
    auto &data = LightManager::m_VehData.Get(veh);
    if (!HeadlightComponent::AreHeadlightsOpen(veh, data)) return;
    auto *texture = g_LegacyHeadShadowTextures[data.bLongLightsOn ? 1 : 0];
    if (!texture) return;
    const CVector dummy = model->m_pVehicleStruct->m_avDummyPos[0];
    auto available = [&](bool right) {
        const float x = right ? dummy.x : -dummy.x;
        return (x > 0.0f || (data.bLightStates[eMaterialType::HeadLightLeft]
            && !LightManager::IsDummyAvailable(data, {eMaterialType::HeadLightLeft, eMaterialType::HighBeamLeft})))
            && (x < 0.0f || (data.bLightStates[eMaterialType::HeadLightRight]
                && !LightManager::IsDummyAvailable(data, {eMaterialType::HeadLightRight, eMaterialType::HighBeamRight})));
    };
    const bool a = available(false) && ((flags & 1) ? first != 0 : modelId == 532);
    const bool b = available(true) && ((flags & 1) ? second != 0 : true);
    if (!a && !b) return;
    auto color = [&](bool right) {
        const auto type = (right ? dummy.x : -dummy.x) > 0.0f ? eMaterialType::HeadLightRight : eMaterialType::HeadLightLeft;
        return DataMgr::Find(modelId) ? LightManager::GetMaterialColor(veh, type).on : DEFAULT_MAT_COL;
    };
    const CRGBA colorA = a ? color(false) : DEFAULT_MAT_COL;
    const CRGBA colorB = b ? color(true) : DEFAULT_MAT_COL;
    if (a && b && colorA == colorB) {
        RenderUtil::RegisterLegacyHeadlightShadow(veh, matrix, dummy, true, true, texture, colorA);
    } else {
        if (a) RenderUtil::RegisterLegacyHeadlightShadow(veh, matrix, dummy, false, false, texture, colorA);
        if (b) RenderUtil::RegisterLegacyHeadlightShadow(veh, matrix, dummy, false, true, texture, colorB);
    }
}

static void __cdecl RegisterHeadCorona(unsigned int id, CEntity *attach,
    unsigned char red, unsigned char green, unsigned char blue, unsigned char alpha,
    const CVector &pos, float radius, float farClip, eCoronaType type, eCoronaFlareType flare,
    bool reflection, bool obstacles, int unused, float angle, bool longDistance,
    float nearClip, unsigned char fadeState, float fadeSpeed, bool onlyFromBelow, bool reflectionDelay)
{
    auto &cfg = LightsConfig::Get();
    if (cfg.bLegacyHeadCoronas && Lights::m_bEnabled && cfg.gbLightCoronasFeature) {
        auto *veh = static_cast<CVehicle *>(attach); // Only CVehicle::DoHeadLightEffect calls this hook.
        if (veh && CPools::ms_pVehiclePool && CPools::ms_pVehiclePool->IsObjectValid(veh) && veh->m_pRwClump
            && veh->m_fHealth > 0.0f && (!cfg.bLightsRequireEngine || !Util::IsEngineOff(veh))) {
            // Native IDs are vehicle + 2 * dummyId + side; the corona position has a heading-dependent offset.
            const auto lightId = id - static_cast<unsigned int>(reinterpret_cast<uintptr_t>(veh));
            auto *model = static_cast<CVehicleModelInfo *>(CModelInfo::GetModelInfo(veh->m_nModelIndex));
            if (lightId >= 4 || !model || !model->m_pVehicleStruct) {
                CCoronas::RegisterCorona(id, attach, red, green, blue, alpha, pos, radius, farClip, type, flare,
                    reflection, obstacles, unused, angle, longDistance, nearClip, fadeState, fadeSpeed, onlyFromBelow, reflectionDelay);
                return;
            }
            const float x = model->m_pVehicleStruct->m_avDummyPos[lightId / 2].x * (lightId & 1 ? 1.0f : -1.0f);
            auto &data = LightManager::m_VehData.Get(veh);
            const bool left = x <= 0.0f;
            const bool right = x >= 0.0f;
            const bool owned = (left && LightManager::IsDummyAvailable(data, {eMaterialType::HeadLightLeft, eMaterialType::HighBeamLeft}))
                || (right && LightManager::IsDummyAvailable(data, {eMaterialType::HeadLightRight, eMaterialType::HighBeamRight}));
            if (!owned && (!left || data.bLightStates[eMaterialType::HeadLightLeft])
                && (!right || data.bLightStates[eMaterialType::HeadLightRight]) && HeadlightComponent::AreHeadlightsOpen(veh, data)) {
                const auto lightType = right && !left ? eMaterialType::HeadLightRight : eMaterialType::HeadLightLeft;
                // Missing model data needs no color lookup (or lazy empty-JSON allocation).
                const auto color = DataMgr::Find(veh->m_nModelIndex) ? LightManager::GetMaterialColor(veh, lightType).on : DEFAULT_MAT_COL;
                red = red * color.r / 255;
                green = green * color.g / 255;
                blue = blue * color.b / 255;
                alpha = 128 * color.a / 255;
                radius = static_cast<float>(static_cast<double>(CWeather::Foggyness) + static_cast<double>(radius)
                    + (data.bLongLightsOn ? 1.0 : 0.0));
            }
        }
    }
    CCoronas::RegisterCorona(id, attach, red, green, blue, alpha, pos, radius, farClip, type, flare,
        reflection, obstacles, unused, angle, longDistance, nearClip, fadeState, fadeSpeed, onlyFromBelow, reflectionDelay);
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
            && !veh->m_pTrailer && veh->m_fHealth > 0.0f && (!cfg.bLightsRequireEngine || !Util::IsEngineOff(veh))) {
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

    // Keep the native argument setup at 0x6E2722; the wrapper replaces its former no-op.
    patch::ReplaceFunctionCall(0x6E2730, reinterpret_cast<void *>(Hooked_DoHeadLightReflection));
	patch::SetUChar(0x6E1A22, 0); // CVehicle::DoTailLightEffect
    patch::ReplaceFunctionCall(0x6E1A2D, reinterpret_cast<void *>(RegisterTailCorona));

	// CVehicle::DoHeadLightEffect
	patch::SetUChar(0x6E0CF8, 0);
	patch::SetUChar(0x6E0DEE, 0);
    patch::ReplaceFunctionCall(0x6E0DF7, reinterpret_cast<void *>(RegisterHeadCorona));

	// CVehicle::DoVehicleLights (native headlight coronas)
	patch::SetUChar(0x6E2193, 0);
	patch::SetUChar(0x6E228B, 0);
	patch::SetUChar(0x6E2532, 0);
	patch::SetUChar(0x6E2627, 0);

    // Front headlight spotlight and the separate rear brake spotlight.
    patch::ReplaceFunctionCall(0x6E27E6, reinterpret_cast<void *>(NativeHeadPointLightBridge));
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
    Events::initGameEvent.after += [] {
        g_LegacyHeadShadowReady = true;
        PrepareLegacyHeadShadows();
    };
    Events::shutdownRwEvent += [] {
        g_LegacyHeadShadowReady = false;
        g_LegacyHeadShadowTextures[0] = g_LegacyHeadShadowTextures[1] = nullptr;
    };

    ModelInfoMgr::RegisterMaterial([](CVehicle *pVeh, RpMaterial *pMat) {
        if (!m_bEnabled) return eMaterialType::UnknownMaterial;
        bool legacyFilter = false;
        const auto *config = pVeh ? DataMgr::Find(pVeh->m_nModelIndex) : nullptr;
        if (config && config->contains("lights") && (*config)["lights"].is_object()) {
            const auto &lights = (*config)["lights"];
            auto flag = lights.find("legacy_filter");
            if (flag != lights.end() && flag->is_boolean()) legacyFilter = flag->get<bool>();
        }
        return LightManager::GetMatType(pMat, legacyFilter);
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



	ModelInfoMgr::RegisterRender([](CVehicle *pVeh) {
		if (!m_bEnabled || !pVeh || !CPools::ms_pVehiclePool ||
			!CPools::ms_pVehiclePool->IsObjectValid(pVeh) || !pVeh->m_pRwClump) return;
		CVehicle *pControlVeh = pVeh;
		CVehicle *pTowedVeh = pVeh;
		if (CModelInfo::IsTrailerModel(pVeh->m_nModelIndex)) {
			CVehicle *tractor = pVeh->m_pTractor;
			if (!tractor || !CPools::ms_pVehiclePool->IsObjectValid(tractor) ||
				!tractor->m_pRwClump || tractor->m_pTrailer != pVeh) return;
			pControlVeh = tractor;
		} else if (pVeh->m_pTrailer && CPools::ms_pVehiclePool->IsObjectValid(pVeh->m_pTrailer) &&
			pVeh->m_pTrailer->m_pRwClump && pVeh->m_pTrailer->m_pTractor == pVeh) {
			pTowedVeh = pVeh->m_pTrailer;
		}
		LightManager::Render(pControlVeh, pTowedVeh,
			CModelInfo::IsTrailerModel(pTowedVeh->m_nModelIndex) ? pVeh : nullptr);
	});
}

void Lights::ReloadConfig() {
	CBaseFeature::ReloadConfig();
	m_bEnabled = m_bActive;
	LightsConfig::Get().InitConfig();
    PrepareLegacyHeadShadows();
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
