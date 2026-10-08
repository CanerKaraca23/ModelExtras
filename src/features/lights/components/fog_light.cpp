#include "pch.h"
#include "fog_light.h"
#include "utils/audiomgr.h"
#include "utils/render.h"
#include "defines.h"
#include <CWeather.h>

static short gLegacyFogWeather = -1;
static int gLegacyFogOverride = -1;
static bool gLegacyFogKeyHeld = false;

void FogLightComponent::UpdateLegacyWeather() {
    gLegacyFogOverride = -1;
    if (!LightsConfig::Get().bLegacyFogState) {
        gLegacyFogWeather = -1;
        gLegacyFogKeyHeld = false;
        return;
    }
    const short weather = CWeather::OldWeatherType;
    const auto needsFog = [](short type) {
        return type == WEATHER_RAINY_SF || type == WEATHER_FOGGY_SF ||
               type == WEATHER_RAINY_COUNTRYSIDE || type == WEATHER_SANDSTORM_DESERT;
    };
    if (gLegacyFogWeather != -1 && needsFog(weather) != needsFog(gLegacyFogWeather)) {
        gLegacyFogOverride = needsFog(weather) ? 1 : 0;
    }
    gLegacyFogWeather = weather;
}

void FogLightComponent::RegisterMaterials(std::unordered_map<uint32_t, eMaterialType>& matMap) {
    matMap[VEHCOL_FOGLIGHT_LEFT.ToInt()] = eMaterialType::FogLightLeft;
    matMap[VEHCOL_FOGLIGHT_RIGHT.ToInt()] = eMaterialType::FogLightRight;
}

eMaterialType FogLightComponent::GetMatType(CRGBA matCol) {
    if (matCol == VEHCOL_FOGLIGHT_LEFT) return eMaterialType::FogLightLeft;
    if (matCol == VEHCOL_FOGLIGHT_RIGHT) return eMaterialType::FogLightRight;
    return eMaterialType::UnknownMaterial;
}

bool FogLightComponent::TryRegisterDummy(CVehicle* pVeh, RwFrame* pFrame, const std::string_view name, VehLightData& data) {
    if (name.starts_with("fogl") || (name.starts_with("fog_") && (STR_FOUND(name, "_l") || STR_FOUND(name, "_r")))) {
        DummyConfig c = LightManager::CreateBaseConfig(pVeh, pFrame);
        c.dummyPos = eDummyPos::Front;
        bool isLeft = STR_FOUND(name, "_l");
        c.lightType = isLeft ? eMaterialType::FogLightLeft : eMaterialType::FogLightRight;
        c.shadow.render = false;
        c.corona.color = c.shadow.color = {255, 255, 255, static_cast<unsigned char>(LightsConfig::Get().gGlobalCoronaIntensity)};
        c.corona.lightingType = eLightingMode::NonDirectional;
        data.dummies[c.lightType].push_back(VehicleDummy(c));
        return true;
    }
    return false;
}

void FogLightComponent::Process(CVehicle* pVeh, VehLightData& data) {
    if (!pVeh) return;
    CPed* pPlayer = FindPlayerPed();
    if (LightsConfig::Get().bLegacyFogState) {
        if (!pVeh->m_pDriver || !pVeh->bEngineOn) return;
        if (gLegacyFogOverride >= 0) data.bFogLightsOn = gLegacyFogOverride != 0;
        if (pPlayer && pVeh->IsDriver(pPlayer)) {
            const bool held = InputMgr::IsKeyDown(LightsConfig::Get().nFogLightKey);
            if (held && !gLegacyFogKeyHeld) {
                data.bFogLightsOn = !data.bFogLightsOn;
                AudioMgr::PlaySwitchSound(pVeh);
            }
            gLegacyFogKeyHeld = held;
        }
        return;
    }
    if (pPlayer && pVeh->IsDriver(pPlayer)) {
        static size_t prev = 0;
        bool isHeadlightsActive = CarUtil::AreLightsOn(pVeh);
        bool canToggleFogLight = !LightsConfig::Get().bFoglightTiedToHeadlight || isHeadlightsActive;

        if (InputMgr::IsKeyJustDown(LightsConfig::Get().nFogLightKey) && (LightManager::IsMaterialAvailable(pVeh, {eMaterialType::FogLightLeft, eMaterialType::FogLightRight}) || LightManager::IsDummyAvailable(data, {eMaterialType::FogLightLeft, eMaterialType::FogLightRight})) && canToggleFogLight) {
            data.bFogLightsOn = !data.bFogLightsOn;
            AudioMgr::PlaySwitchSound(pVeh);
        }
    }
}

void FogLightComponent::Render(CVehicle* pControlVeh, CVehicle* pTowedVeh, VehLightData& data) {
    bool isHeadlightsActive = CarUtil::AreLightsOn(pControlVeh);
    bool legacy = LightsConfig::Get().bLegacyFogState;
    bool isFoggy = !legacy && Util::IsFoggy();
    bool shouldRenderFog = legacy || isFoggy || !LightsConfig::Get().bFoglightTiedToHeadlight || isHeadlightsActive;
    bool isFogLightOn = (data.bFogLightsOn || isFoggy) && (legacy || !LightsConfig::Get().bFoglightTiedToHeadlight || !CarUtil::IsLightsForcedOff(pControlVeh));

    if (!isFogLightOn || !shouldRenderFog) return;
    bool isFogOk = !Util::IsPanelDamaged(LightManager::GetRenderVehicle(pControlVeh), ePanels::BUMP_FRONT);
    LightManager::RenderLights(pControlVeh, pTowedVeh, data, eMaterialType::FogLightLeft, true, "foglight", 3.0f, false, isFogOk);
    LightManager::RenderLights(pControlVeh, pTowedVeh, data, eMaterialType::FogLightRight, true, "foglight", 3.0f, false, isFogOk);
}

void FogLightComponent::ProcessPointLights(CVehicle* pVeh, VehLightData& data) {
    bool isHeadlightsOn = CarUtil::AreLightsOn(pVeh);
    bool legacy = LightsConfig::Get().bLegacyFogState;
    bool isFoggy = !legacy && Util::IsFoggy();
    bool shouldRenderFog = legacy || isFoggy || !LightsConfig::Get().bFoglightTiedToHeadlight || isHeadlightsOn;
    bool isFogLightOn = (data.bFogLightsOn || isFoggy) && (legacy || !LightsConfig::Get().bFoglightTiedToHeadlight || !CarUtil::IsLightsForcedOff(pVeh));

    if (isFogLightOn && shouldRenderFog) {
        for (eMaterialType type : {eMaterialType::FogLightLeft, eMaterialType::FogLightRight}) {
            if (!LightManager::IsDummyAvailable(data, type) || !data.bLightStates[type]) continue;

            bool isLeft = (type == eMaterialType::FogLightLeft);
            eLights lightEnum = isLeft ? eLights::LIGHT_FRONT_LEFT : eLights::LIGHT_FRONT_RIGHT;
            ePanels wingEnum = isLeft ? ePanels::WING_FRONT_LEFT : ePanels::WING_FRONT_RIGHT;
            if (Util::IsLightDamaged(pVeh, lightEnum) || Util::IsPanelDamaged(pVeh, wingEnum) || Util::IsPanelDamaged(pVeh, ePanels::BUMP_FRONT)) {
                continue;
            }

            for (auto& e : data.dummies[type]) {
                e->Update();
                RenderUtil::RegisterPointLight(&e->Get(), e->Get().corona.color, 8.5f, true);
            }
        }
    }
}
