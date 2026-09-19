#include "pch.h"
#include "manager.h"
#include "utils/modelinfomgr.h"
#include "utils/render.h"
#include "utils/util.h"
#include "utils/car.h"
#include "utils/datamgr.h"
#include "defines.h"
#include "components/headlight.h"
#include "components/indicator.h"
#include "components/reverse_light.h"
#include "components/brake_light.h"
#include "components/tail_light.h"
#include "components/stt_light.h"
#include "components/nabrake_light.h"
#include "components/fog_light.h"
#include "components/strobe_light.h"
#include "components/drl_light.h"
#include "components/side_light.h"
#include "components/spot_light.h"
#include "damage.h"
#include <CAudioEngine.h>
#include <Fx_c.h>
#include <CModelInfo.h>
#include <CVehicleModelInfo.h>
#include "enums/vehdummy.h"
#include "utils/mathutil.h"

void LightManager::Init() {
    m_Components.clear();
    m_MaterialMap.clear();

    m_Components.push_back(std::make_unique<HeadlightComponent>());
    m_Components.push_back(std::make_unique<IndicatorComponent>());
    m_Components.push_back(std::make_unique<ReverseLightComponent>());
    m_Components.push_back(std::make_unique<BrakeLightComponent>());
    m_Components.push_back(std::make_unique<TailLightComponent>());
    m_Components.push_back(std::make_unique<STTLightComponent>());
    m_Components.push_back(std::make_unique<NABrakeLightComponent>());
    m_Components.push_back(std::make_unique<FogLightComponent>());
    m_Components.push_back(std::make_unique<StrobeLightComponent>());
    m_Components.push_back(std::make_unique<DRLLightComponent>());
    m_Components.push_back(std::make_unique<SideLightComponent>());
    m_Components.push_back(std::make_unique<SpotLightComponent>());

    for (const auto& comp : m_Components) {
        comp->RegisterMaterials(m_MaterialMap);
    }
}

DummyConfig LightManager::CreateBaseConfig(CVehicle* pVeh, RwFrame* pFrame) {
    DummyConfig c;
    c.frame = pFrame;
    c.position = pFrame->modelling.pos;
    c.pVeh = pVeh;
    c.corona.size = LightsConfig::Get().gfGlobalCoronaSize;
    c.corona.color = {255, 255, 255, static_cast<unsigned char>(LightsConfig::Get().gGlobalCoronaIntensity)};
    c.corona.lightingType = eLightingMode::NonDirectional;
    return c;
}

eMaterialType LightManager::GetMatType(RpMaterial* pMat) {
    if (Util::IsAntiPatternLightMaterial(pMat)) {
        return eMaterialType::UnknownMaterial;
    }

    CRGBA matCol = *reinterpret_cast<CRGBA*>(RpMaterialGetColor(pMat));
    matCol.a = 255;

    auto it = m_MaterialMap.find(matCol.ToInt());
    if (it != m_MaterialMap.end()) {
        return it->second;
    }

    return eMaterialType::UnknownMaterial;
}

void LightManager::RegisterDummy(CVehicle* pVeh, RwFrame* pFrame, const std::string_view name) {
    if (!pVeh || !pFrame) return;

    VehLightData& data = m_VehData.Get(pVeh);

    if (!rwLinkListEmpty(&pFrame->objectList)) {
        // Pop-up headlights are animated geometry frames (RpAtomic attached), not empty dummies.
        // Only HeadlightComponent inspects non-empty frames to detect pop-up lights.
        for (const auto& comp : m_Components) {
            if (dynamic_cast<HeadlightComponent*>(comp.get())) {
                comp->TryRegisterDummy(pVeh, pFrame, name, data);
                break;
            }
        }
        return;
    }

    for (const auto& comp : m_Components) {
        if (comp->TryRegisterDummy(pVeh, pFrame, name, data)) return;
    }
}

static CVector GetLightWorldPosition(CVehicle* pVeh, const VehLightData& data, int lightIdx) {
    if (!pVeh) return CVector(0.0f, 0.0f, 0.0f);

    auto getDummyPos = [&](eMaterialType type, bool isLeft) -> std::optional<CVector> {
        if (type >= 0 && type < eMaterialType::TotalMaterial && !data.dummies[type].empty()) {
            for (const auto& dummy : data.dummies[type]) {
                const auto& c = dummy.GetRef();
                if (c.frame) {
                    RwFrameGetLTM(c.frame);
                    if (c.mirroredX) {
                        CVector localPos = c.position;
                        if (localPos.Magnitude() < 0.001f) {
                            localPos = c.frame->modelling.pos;
                        }
                        localPos.x = isLeft ? -std::abs(localPos.x) : std::abs(localPos.x);
                        return pVeh->TransformFromObjectSpace(localPos);
                    } else {
                        CVector worldPos = reinterpret_cast<const CVector&>(c.frame->ltm.pos);
                        if (worldPos.Magnitude() > 0.01f) {
                            return worldPos;
                        }
                    }
                }
            }
        }
        return std::nullopt;
    };

    std::optional<CVector> pos;
    switch (lightIdx) {
    case 0: // Front-Left
        pos = getDummyPos(eMaterialType::HeadLightLeft, true);
        break;
    case 1: // Front-Right
        pos = getDummyPos(eMaterialType::HeadLightRight, false);
        break;
    case 2: // Rear-Left
        pos = getDummyPos(eMaterialType::TailLightLeft, true);
        if (!pos) pos = getDummyPos(eMaterialType::BrakeLightLeft, true);
        if (!pos) pos = getDummyPos(eMaterialType::STTLightLeft, true);
        break;
    case 3: // Rear-Right
        pos = getDummyPos(eMaterialType::TailLightRight, false);
        if (!pos) pos = getDummyPos(eMaterialType::BrakeLightRight, false);
        if (!pos) pos = getDummyPos(eMaterialType::STTLightRight, false);
        break;
    }

    if (pos && !pos->IsZero()) {
        return *pos;
    }

    CVehicleModelInfo* pInfo = static_cast<CVehicleModelInfo*>(CModelInfo::GetModelInfo(pVeh->m_nModelIndex));
    if (pInfo && pInfo->m_pVehicleStruct) {
        if (lightIdx == 0 || lightIdx == 1) {
            CVector dPos = pInfo->m_pVehicleStruct->m_avDummyPos[eVehicleDummies::LIGHT_FRONT_MAIN];
            if (!dPos.IsZero()) {
                dPos.x = (lightIdx == 0) ? -std::abs(dPos.x) : std::abs(dPos.x);
                return pVeh->TransformFromObjectSpace(dPos);
            }
        } else {
            CVector dPos = pInfo->m_pVehicleStruct->m_avDummyPos[eVehicleDummies::LIGHT_REAR_MAIN];
            if (!dPos.IsZero()) {
                dPos.x = (lightIdx == 2) ? -std::abs(dPos.x) : std::abs(dPos.x);
                return pVeh->TransformFromObjectSpace(dPos);
            }
        }
    }

    if (pInfo && pInfo->m_pColModel) {
        const auto& box = pInfo->m_pColModel->m_boundBox;
        float x = (lightIdx == 0 || lightIdx == 2) ? box.m_vecMin.x * 0.75f : box.m_vecMax.x * 0.75f;
        float y = (lightIdx == 0 || lightIdx == 1) ? box.m_vecMax.y * 0.95f : box.m_vecMin.y * 0.95f;
        float z = (box.m_vecMin.z + box.m_vecMax.z) * 0.5f;
        return pVeh->TransformFromObjectSpace(CVector(x, y, z));
    }

    return pVeh->GetPosition();
}

static std::optional<CVector> GetSpecificDummyPos(CVehicle* pVeh, const VehLightData& data, eMaterialType type, bool isLeft) {
    if (!pVeh || type < 0 || type >= eMaterialType::TotalMaterial || data.dummies[type].empty()) {
        return std::nullopt;
    }
    for (const auto& dummy : data.dummies[type]) {
        const auto& c = dummy.GetRef();
        if (c.frame) {
            RwFrameGetLTM(c.frame);
            if (c.mirroredX) {
                CVector localPos = c.position;
                if (localPos.Magnitude() < 0.001f) {
                    localPos = c.frame->modelling.pos;
                }
                localPos.x = isLeft ? -std::abs(localPos.x) : std::abs(localPos.x);
                return pVeh->TransformFromObjectSpace(localPos);
            } else {
                CVector worldPos = reinterpret_cast<const CVector&>(c.frame->ltm.pos);
                if (worldPos.Magnitude() > 0.01f) {
                    return worldPos;
                }
            }
        }
    }
    return std::nullopt;
}

static RwRGBA ResolveLightColor(CVehicle* pVeh, const VehLightData& data, eMaterialType type, RwRGBA defaultCol) {
    if (!pVeh) return defaultCol;

    if (type >= 0 && type < eMaterialType::TotalMaterial && !data.dummies[type].empty()) {
        const auto& c = data.dummies[type].front().GetRef();
        if (c.hasCustomColor) {
            return { c.corona.color.r, c.corona.color.g, c.corona.color.b, 255 };
        }
    }

    MatStateColor matCol = LightManager::GetMaterialColor(pVeh, type);
    if (matCol.on != DEFAULT_MAT_COL) {
        return { matCol.on.r, matCol.on.g, matCol.on.b, 255 };
    }

    return defaultCol;
}

void LightManager::ProcessLightBreakEffects(CVehicle* pVeh, VehLightData& data) {
    if (!LightsConfig::Get().bLightBreakEffect || !pVeh || pVeh->m_nVehicleSubClass != VEHICLE_AUTOMOBILE) {
        return;
    }

    LightDamageState dmg = LightDamageState::Get(pVeh, pVeh);
    bool curDamaged[4] = {
        !dmg.isHeadlightLeftOk,
        !dmg.isHeadlightRightOk,
        !dmg.isRearLeftOk,
        !dmg.isRearRightOk
    };

    if (!data.bDamageInit) {
        for (int i = 0; i < 4; ++i) {
            data.bPrevLightDamaged[i] = curDamaged[i];
        }
        data.bDamageInit = true;
        return;
    }

    for (int i = 0; i < 4; ++i) {
        if (!data.bPrevLightDamaged[i] && curDamaged[i]) {
            if (pVeh->GetIsOnScreen()) {
                CVector vehPos = pVeh->GetPosition();
                float distSq = MathUtil::DistanceSquared(TheCamera.GetPosition(), vehPos);
                if (distSq < (150.0f * 150.0f)) {
                    CVector cornerPos = GetLightWorldPosition(pVeh, data, i);
                    if (!cornerPos.IsZero()) {
                        if (i == 0 || i == 1) { // Front corners
                            bool isLeft = (i == 0);
                            eMaterialType headType = isLeft ? eMaterialType::HeadLightLeft : eMaterialType::HeadLightRight;
                            eMaterialType indType = isLeft ? eMaterialType::IndicatorLightLeftFront : eMaterialType::IndicatorLightRightFront;
                            eMaterialType fogType = isLeft ? eMaterialType::FogLightLeft : eMaterialType::FogLightRight;

                            // 1. Headlight (Clear/White or custom)
                            RwRGBA headCol = ResolveLightColor(pVeh, data, headType, { 255, 255, 255, 255 });
                            CVector headPos = GetSpecificDummyPos(pVeh, data, headType, isLeft).value_or(cornerPos);
                            g_fx.AddGlass(headPos, headCol, 0.40f, 22);

                            // 2. Front Turn Indicator (Amber or custom)
                            RwRGBA indCol = ResolveLightColor(pVeh, data, indType, { 255, 160, 20, 255 });
                            CVector indPos = GetSpecificDummyPos(pVeh, data, indType, isLeft).value_or(cornerPos);
                            g_fx.AddGlass(indPos, indCol, 0.35f, 14);

                            // 3. Fog Light (if present)
                            if (!data.dummies[fogType].empty() || LightManager::IsMaterialAvailable(pVeh, fogType)) {
                                RwRGBA fogCol = ResolveLightColor(pVeh, data, fogType, { 255, 255, 230, 255 });
                                CVector fogPos = GetSpecificDummyPos(pVeh, data, fogType, isLeft).value_or(cornerPos);
                                g_fx.AddGlass(fogPos, fogCol, 0.35f, 10);
                            }
                        } else { // Rear corners
                            bool isLeft = (i == 2);
                            eMaterialType tailType = isLeft ? eMaterialType::TailLightLeft : eMaterialType::TailLightRight;
                            eMaterialType brakeType = isLeft ? eMaterialType::BrakeLightLeft : eMaterialType::BrakeLightRight;
                            eMaterialType sttType = isLeft ? eMaterialType::STTLightLeft : eMaterialType::STTLightRight;
                            eMaterialType indType = isLeft ? eMaterialType::IndicatorLightLeftRear : eMaterialType::IndicatorLightRightRear;
                            eMaterialType revType = isLeft ? eMaterialType::ReverseLightLeft : eMaterialType::ReverseLightRight;

                            // 1. Taillight / Brake light (Red or custom)
                            RwRGBA tailCol = ResolveLightColor(pVeh, data, tailType, { 240, 25, 25, 255 });
                            CVector tailPos = GetSpecificDummyPos(pVeh, data, tailType, isLeft)
                                .value_or(GetSpecificDummyPos(pVeh, data, brakeType, isLeft)
                                .value_or(GetSpecificDummyPos(pVeh, data, sttType, isLeft)
                                .value_or(cornerPos)));
                            g_fx.AddGlass(tailPos, tailCol, 0.40f, 20);

                            // 2. Rear Turn Indicator (Amber or custom)
                            RwRGBA indCol = ResolveLightColor(pVeh, data, indType, { 255, 160, 20, 255 });
                            CVector indPos = GetSpecificDummyPos(pVeh, data, indType, isLeft).value_or(cornerPos);
                            g_fx.AddGlass(indPos, indCol, 0.35f, 12);

                            // 3. Reverse Light (Clear/White or custom)
                            RwRGBA revCol = ResolveLightColor(pVeh, data, revType, { 255, 255, 255, 255 });
                            CVector revPos = GetSpecificDummyPos(pVeh, data, revType, isLeft).value_or(cornerPos);
                            g_fx.AddGlass(revPos, revCol, 0.35f, 8);
                        }

                        CVector forward = pVeh->GetForward();
                        CVector sparkDir = (i == 0 || i == 1) ? (forward + CVector(0.0f, 0.0f, 0.3f)) : (-forward + CVector(0.0f, 0.0f, 0.3f));
                        sparkDir.Normalize();
                        CVector across = CVector(0.0f, 0.0f, 1.0f);
                        g_fx.AddSparks(cornerPos, sparkDir, 2.5f, 12, across, 0, 0.5f, 0.5f);

                        AudioEngine.ReportMissionAudioEvent(eAudioEvents::AE_GLASS_BREAK_FAST, &cornerPos);
                    }
                }
            }
        }
        data.bPrevLightDamaged[i] = curDamaged[i];
    }
}

void LightManager::Process(CVehicle* pVeh) {
    if (!pVeh) return;

    VehLightData& data = m_VehData.Get(pVeh);
    for (const auto& comp : m_Components) {
        comp->Process(pVeh, data);
    }
    ProcessLightBreakEffects(pVeh, data);
}

void LightManager::Render(CVehicle* pControlVeh, CVehicle* pTowedVeh) {
    VehLightData& data = m_VehData.Get(pControlVeh);
    eIndicatorState indState = data.nIndicatorState;

    data.bLightRenderedThisFrame.fill(false);
    if (pControlVeh != pTowedVeh) {
        m_VehData.Get(pTowedVeh).bLightRenderedThisFrame.fill(false);
    }

    bool isAlarmActive = pControlVeh->m_nAlarmState != 0 && pControlVeh->m_nAlarmState != 0xFFFF;

    // Fix for UIF SAMP server https://github.com/user-grinch/ModelExtras/issues/112
    // Don't clear light state when lights are forced on/already on via SAMP or alarm is active
    if (((Util::IsEngineOff(pControlVeh) && indState == eIndicatorState::Off && !isAlarmActive) && !CarUtil::IsLightsForcedOn(pControlVeh) && !pControlVeh->bLightsOn) || CarUtil::IsLightsForcedOff(pControlVeh)) {
        pControlVeh->bLightsOn = false;
        pControlVeh->m_renderLights.m_bLeftFront = false;
        pControlVeh->m_renderLights.m_bRightFront = false;
        pControlVeh->m_renderLights.m_bLeftRear = false;
        pControlVeh->m_renderLights.m_bRightRear = false;
    }

    // Fix for park car alarm lights
    // Allow through if lights, indicators, or alarm are explicitly on
    if (pControlVeh->m_fHealth <= 0.0f || ((Util::IsEngineOff(pControlVeh) && indState == eIndicatorState::Off && !isAlarmActive) && !CarUtil::IsLightsForcedOn(pControlVeh) && !pControlVeh->bLightsOn)) {
        return;
    }

    for (const auto& comp : m_Components) {
        comp->Render(pControlVeh, pTowedVeh, data);
    }

    auto ProcessFadeOut = [](CVehicle* pVeh, VehLightData& vData) {
        for (int t = 0; t < eMaterialType::TotalMaterial; ++t) {
            eMaterialType type = static_cast<eMaterialType>(t);
            if (!vData.bLightRenderedThisFrame[type] && vData.fLightFactor[type] > 0.001f) {
                float inertia = GetLightInertia(pVeh, vData, type);
                if (inertia > 0.0001f) {
                    float step = (CTimer::ms_fTimeStep / 50.0f) / inertia;
                    vData.fLightFactor[type] = std::max(0.0f, vData.fLightFactor[type] - step);
                } else {
                    vData.fLightFactor[type] = 0.0f;
                }

                if (vData.fLightFactor[type] > 0.001f) {
                    float factor = vData.fLightFactor[type];
                    ModelInfoMgr::EnableMaterial(pVeh, type);

                    int id = static_cast<int>(type) * 1000;
                    for (auto& dummy : vData.dummies[type]) {
                        const DummyConfig& c = dummy->GetRef();
                        dummy->Update();
                        RwFrame *parent = RwFrameGetParent(dummy->Get().frame);
                        bool isBike = pVeh->m_nVehicleSubClass == VEHICLE_BIKE;
                        bool isDamaged = Util::IsFrameDamaged(pVeh, parent) || !FrameUtil::IsOkAtomicVisible(parent);
                        bool atomicCheck = !isBike && pVeh->GetIsOnScreen() && type != eMaterialType::HeadLightLeft && type != eMaterialType::HeadLightRight && isDamaged;
                        if (atomicCheck || (c.dummyPos == eDummyPos::Rear && pVeh->m_pTrailer)) continue;

                        float szMul = 1.0f;
                        if (type == eMaterialType::HeadLightLeft || type == eMaterialType::HeadLightRight) {
                            szMul = 1.0f + 2.0f * vData.fHighBeamFactor;
                        }
                        EnableDummy((int)pVeh + 42 + id++, &dummy, pVeh, szMul, factor);

                        if (c.shadow.render && factor > 0.01f) {
                            std::string tex = c.shadow.texture.empty() ? (type == eMaterialType::HeadLightLeft || type == eMaterialType::HeadLightRight ? ((vData.fHighBeamFactor > 0.3f) ? "headlight_long" : "headlight_short") : "") : c.shadow.texture;
                            if (!tex.empty()) {
                                float sz = (type == eMaterialType::HeadLightLeft || type == eMaterialType::HeadLightRight) ? LightsConfig::Get().headlightSz : 1.0f;
                                RenderUtil::RegisterShadowDirectional(&dummy->Get(), tex, sz * c.shadow.size, factor);
                            }
                        }
                    }
                } else {
                    if (type == eMaterialType::HeadLightLeft || type == eMaterialType::HeadLightRight) {
                        vData.fHighBeamFactor = 0.0f;
                        vData.bLongLightsOn = false;
                    }
                }
            }
        }
    };

    ProcessFadeOut(pControlVeh, data);
    if (pControlVeh != pTowedVeh) {
        ProcessFadeOut(pTowedVeh, m_VehData.Get(pTowedVeh));
    }
}

void LightManager::EnableDummy(int id, VehicleDummy *dummy, CVehicle *pVeh, float szMul, float alphaMul) {
    if (LightsConfig::Get().gbLightCoronasFeature && alphaMul > 0.01f) {
        DummyConfig &c = dummy->Get();
        CRGBA origColor = c.corona.color;
        c.corona.color.a = static_cast<unsigned char>(std::clamp(static_cast<float>(origColor.a) * alphaMul, 0.0f, 255.0f));
        if (c.corona.lightingType == eLightingMode::NonDirectional) {
            RenderUtil::RegisterCorona(pVeh, (reinterpret_cast<unsigned int>(pVeh) * 255) + 255 + id, c.position, c.corona.color, c.corona.size * szMul);
        } else {
            RenderUtil::RegisterCoronaDirectional(&dummy->Get(), c.rotation.angle, 180.0f, szMul, c.corona.lightingType == eLightingMode::Inversed, false);
        }
        c.corona.color = origColor;
    }
}

void LightManager::RenderLight(CVehicle* pVeh, VehLightData& data, eMaterialType type, bool isOn, const std::string& texture, float sz, bool highlight, bool isDummyOk, bool materialsOnly) {
    if (!isOn || !data.bLightStates[type]) return;

    float inertia = GetLightInertia(pVeh, data, type);
    if (!data.bLightRenderedThisFrame[type]) {
        data.bLightRenderedThisFrame[type] = true;
        if (inertia > 0.0001f) {
            float step = (CTimer::ms_fTimeStep / 50.0f) / inertia;
            data.fLightFactor[type] = std::min(1.0f, data.fLightFactor[type] + step);
        } else {
            data.fLightFactor[type] = 1.0f;
        }
    }
    bool isAvailable = IsDummyAvailable(data, type);

    if (type == eMaterialType::HeadLightLeft) {
        float target = (data.bLongLightsOn && isOn) ? 1.0f : 0.0f;
        if (inertia > 0.0001f) {
            float hbStep = (CTimer::ms_fTimeStep / 50.0f) / inertia;
            if (data.fHighBeamFactor < target) {
                data.fHighBeamFactor = std::min(target, data.fHighBeamFactor + hbStep);
            } else if (data.fHighBeamFactor > target) {
                data.fHighBeamFactor = std::max(target, data.fHighBeamFactor - hbStep);
            }
        } else {
            data.fHighBeamFactor = target;
        }
    }

    float factor = data.fLightFactor[type];
    int id = static_cast<int>(type) * 1000;
    bool hasActiveDummy = false;

    if (isAvailable) {
        for (auto& dummy : data.dummies[type]) {
            const DummyConfig& c = dummy->GetRef();
            dummy->Update();
            RwFrame *parent = RwFrameGetParent(dummy->Get().frame);
            bool isBike = pVeh->m_nVehicleSubClass == VEHICLE_BIKE;
            bool isDamaged = false;
            if (c.damagePanel != -1 || c.damageDoor != -1) {
                isDamaged = CarUtil::IsDummyDamaged(pVeh, c);
            } else if (parent) {
                isDamaged = Util::IsFrameDamaged(pVeh, parent);
            }
            if (!isDamaged && parent) {
                isDamaged = !FrameUtil::IsOkAtomicVisible(parent);
            }
            bool atomicCheck = !isBike && pVeh->GetIsOnScreen() && type != eMaterialType::HeadLightLeft && type != eMaterialType::HeadLightRight && isDamaged;

            if (atomicCheck || (c.dummyPos == eDummyPos::Rear && pVeh->m_pTrailer) || !isDummyOk) {
                continue;
            }

            hasActiveDummy = true;

            if (type == eMaterialType::StrobeLight) {
                size_t timer = CTimer::m_snTimeInMilliseconds;
                if (timer - c.strobe.timer > c.strobe.delay) {
                    dummy->Get().strobe.enabled = !c.strobe.enabled;
                    dummy->Get().strobe.timer = timer;
                }

                if (c.strobe.enabled) {
                    ModelInfoMgr::EnableStrobeMaterial(pVeh, c.dummyIdx);
                } else {
                    continue;
                }
            }

            if (materialsOnly) {
                continue;
            }

            float szMul = 1.0f;
            if (type == eMaterialType::HeadLightLeft || type == eMaterialType::HeadLightRight) {
                szMul = 1.0f + 2.0f * data.fHighBeamFactor;
                if (highlight && !data.bLongLightsOn) {
                    szMul = std::max(szMul, 3.00f);
                }
            } else if (highlight) {
                szMul = (type == eMaterialType::TailLightLeft || type == eMaterialType::TailLightRight) ? 1.50f : 3.00f;
            }
            EnableDummy((int)pVeh + 42 + id++, &dummy, pVeh, szMul, factor);

            // Skip front shadows on bike wheelie
            if (c.dummyPos == eDummyPos::Front && Util::IsVehicleDoingWheelie(pVeh)) {
                continue;
            }

            if (c.shadow.render && factor > 0.01f) {
                std::string tex = c.shadow.texture.empty() ? texture : c.shadow.texture;
                if ((type == eMaterialType::HeadLightLeft || type == eMaterialType::HeadLightRight) && data.fHighBeamFactor > 0.3f) {
                    tex = "headlight_long";
                }
                if (!tex.empty()) {
                    RenderUtil::RegisterShadowDirectional(&dummy->Get(), tex, sz * c.shadow.size, factor);
                }
            }
        }
    }

    if (!isAvailable || hasActiveDummy) {
        ModelInfoMgr::EnableMaterial(pVeh, type);
    }
}

void LightManager::RenderLights(CVehicle* pControlVeh, CVehicle* pTowedVeh, VehLightData& data, eMaterialType type, bool isOn, const std::string& texture, float sz, bool highlight, bool isDummyOk, bool materialsOnly) {
    if (data.bLightStates[type]) {
        RenderLight(pControlVeh, data, type, isOn, texture, sz, highlight, isDummyOk, materialsOnly);
    }

    if (pControlVeh != pTowedVeh && m_VehData.Get(pTowedVeh).bLightStates[type]) {
        RenderLight(pTowedVeh, m_VehData.Get(pTowedVeh), type, isOn, texture, sz, highlight, isDummyOk, materialsOnly);
    }
}

bool LightManager::IsDummyAvailable(VehLightData& data, eMaterialType type) {
    if (type < 0 || type >= eMaterialType::TotalMaterial) return false;
    return !data.dummies[type].empty();
}

bool LightManager::IsDummyAvailable(VehLightData& data, std::initializer_list<eMaterialType> types) {
    for (eMaterialType type : types) {
        if (IsDummyAvailable(data, type)) return true;
    }
    return false;
}

bool LightManager::IsMaterialAvailable(CVehicle* pVeh, eMaterialType type) {
    return ModelInfoMgr::IsMaterialAvailable(pVeh, type);
}

bool LightManager::IsMaterialAvailable(CVehicle* pVeh, std::initializer_list<eMaterialType> types) {
    for (eMaterialType type : types) {
        if (IsMaterialAvailable(pVeh, type)) return true;
    }
    return false;
}

bool LightManager::IsIndicatorOn(CVehicle* pVeh) {
    if (!pVeh || pVeh->m_fHealth <= 0.0f || !BlinkerState::Get().bIndicatorsDelay) {
        return false;
    }
    if ((pVeh->m_nVehicleSubClass != VEHICLE_AUTOMOBILE && pVeh->m_nVehicleSubClass != VEHICLE_MTRUCK) || CModelInfo::IsBikeModel(pVeh->m_nModelIndex)) {
        return false;
    }
    VehLightData& data = m_VehData.Get(pVeh);
    if (data.nIndicatorState == eIndicatorState::Off) {
        return false;
    }
    return data.bUsingGlobalIndicators ||
           IsMaterialAvailable(pVeh, INDICATOR_LIGHTS_TYPE) ||
           IsDummyAvailable(data, INDICATOR_LIGHTS_TYPE) ||
           IsMaterialAvailable(pVeh, {eMaterialType::STTLightLeft, eMaterialType::STTLightRight});
}

void LightManager::ProcessPointLights(CVehicle *pVeh) {
    if (!LightsConfig::Get().gbLightPointLights || !pVeh || pVeh->m_fHealth <= 0.0f || pVeh->m_nVehicleSubClass == VEHICLE_BMX || pVeh->m_nVehicleSubClass == VEHICLE_BOAT || pVeh->m_nVehicleSubClass == VEHICLE_TRAILER) {
        return;
    }

    if (MathUtil::DistanceSquared(pVeh->GetPosition(), TheCamera.GetPosition()) > (75.0f * 75.0f)) {
        return;
    }

    pVeh->UpdateRwFrame();

    VehLightData &data = m_VehData.Get(pVeh);
    for (const auto& comp : m_Components) {
        comp->ProcessPointLights(pVeh, data);
    }
}

void LightManager::Reload(CVehicle* pVeh) {
    LightsConfig::Get().InitConfig();
    if (pVeh) {
        m_VehData.Get(pVeh).ClearDummies();
        DataMgr::Reload(pVeh->m_nModelIndex);
    }
}

bool LightManager::IsBraking(CVehicle* pVeh) {
    if (!pVeh || !pVeh->m_pDriver) {
        return false;
    }

    if (pVeh->m_fBreakPedal > 0.05f) {
        return true;
    }

    if (LightsConfig::Get().bPlayerIdleBrakeLights) {
        CPed* pPlayer = FindPlayerPed();
        if (pPlayer && pVeh->IsDriver(pPlayer)) {
            if (pVeh->m_nVehicleSubClass == VEHICLE_AUTOMOBILE || pVeh->m_nVehicleSubClass == VEHICLE_MTRUCK || 
                pVeh->m_nVehicleSubClass == VEHICLE_QUAD || pVeh->m_nVehicleSubClass == VEHICLE_BIKE) 
            {
                if (!CarUtil::IsEngineOff(pVeh) && pVeh->m_fHealth > 0.0f) {
                    if (pVeh->m_fGasPedal <= 0.05f && CarUtil::GetVehicleSpeed(pVeh) < 0.5f) {
                        return true;
                    }
                }
            }
        }
    }

    return false;
}

const char* LightManager::GetLightGroupKey(eMaterialType type) {
    switch (type) {
    case eMaterialType::HeadLightLeft:
    case eMaterialType::HeadLightRight:
        return "headlights";
    case eMaterialType::TailLightLeft:
    case eMaterialType::TailLightRight:
    case eMaterialType::STTLightLeft:
    case eMaterialType::STTLightRight:
        return "taillights";
    case eMaterialType::BrakeLightLeft:
    case eMaterialType::BrakeLightRight:
    case eMaterialType::NABrakeLightLeft:
    case eMaterialType::NABrakeLightRight:
        return "brakelights";
    case eMaterialType::ReverseLightLeft:
    case eMaterialType::ReverseLightRight:
        return "reverselights";
    case eMaterialType::IndicatorLightLeftFront:
    case eMaterialType::IndicatorLightRightFront:
    case eMaterialType::IndicatorLightLeftRear:
    case eMaterialType::IndicatorLightRightRear:
    case eMaterialType::IndicatorLightLeftMiddle:
    case eMaterialType::IndicatorLightRightMiddle:
        return "indicators";
    case eMaterialType::FogLightLeft:
    case eMaterialType::FogLightRight:
        return "foglights";
    case eMaterialType::SideLightLeft:
    case eMaterialType::SideLightRight:
        return "sidelights";
    case eMaterialType::DayLight:
        return "daylights";
    case eMaterialType::NightLight:
        return "nightlights";
    case eMaterialType::AllDayLight:
        return "alldaylights";
    case eMaterialType::SpotLight:
        return "spotlights";
    case eMaterialType::StrobeLight:
        return "strobelights";
    case eMaterialType::EngineOnLed:
    case eMaterialType::EngineBrokenLed:
    case eMaterialType::FogLightLed:
    case eMaterialType::HighBeamLed:
    case eMaterialType::LowBeamLed:
    case eMaterialType::IndicatorLeftLed:
    case eMaterialType::IndicatorRightLed:
    case eMaterialType::SirenLed:
    case eMaterialType::BootOpenLed:
    case eMaterialType::BonnetOpenLed:
    case eMaterialType::DoorOpenLed:
    case eMaterialType::RoofOpenLed:
        return "leds";
    default:
        return nullptr;
    }
}

const char* LightManager::GetLightSpecificKey(eMaterialType type) {
    switch (type) {
    case eMaterialType::HeadLightLeft: return "headlight_l";
    case eMaterialType::HeadLightRight: return "headlight_r";
    case eMaterialType::TailLightLeft: return "taillight_l";
    case eMaterialType::TailLightRight: return "taillight_r";
    case eMaterialType::STTLightLeft: return "sttlight_l";
    case eMaterialType::STTLightRight: return "sttlight_r";
    case eMaterialType::BrakeLightLeft:
    case eMaterialType::NABrakeLightLeft: return "brakelight_l";
    case eMaterialType::BrakeLightRight:
    case eMaterialType::NABrakeLightRight: return "brakelight_r";
    case eMaterialType::ReverseLightLeft: return "reverselight_l";
    case eMaterialType::ReverseLightRight: return "reverselight_r";
    case eMaterialType::IndicatorLightLeftFront: return "indicator_lf";
    case eMaterialType::IndicatorLightRightFront: return "indicator_rf";
    case eMaterialType::IndicatorLightLeftRear: return "indicator_lr";
    case eMaterialType::IndicatorLightRightRear: return "indicator_rr";
    case eMaterialType::IndicatorLightLeftMiddle: return "indicator_lm";
    case eMaterialType::IndicatorLightRightMiddle: return "indicator_rm";
    case eMaterialType::FogLightLeft: return "foglight_l";
    case eMaterialType::FogLightRight: return "foglight_r";
    case eMaterialType::SideLightLeft: return "sidelight_l";
    case eMaterialType::SideLightRight: return "sidelight_r";
    case eMaterialType::DayLight: return "daylight";
    case eMaterialType::NightLight: return "nightlight";
    case eMaterialType::AllDayLight: return "alldaylight";
    case eMaterialType::SpotLight: return "spotlight";
    case eMaterialType::StrobeLight: return "strobelight";
    case eMaterialType::EngineOnLed: return "engine_on";
    case eMaterialType::EngineBrokenLed: return "engine_broken";
    case eMaterialType::FogLightLed: return "fog_light";
    case eMaterialType::HighBeamLed: return "high_beam";
    case eMaterialType::LowBeamLed: return "low_beam";
    case eMaterialType::IndicatorLeftLed: return "indicator_left";
    case eMaterialType::IndicatorRightLed: return "indicator_right";
    case eMaterialType::SirenLed: return "siren";
    case eMaterialType::BootOpenLed: return "boot_open";
    case eMaterialType::BonnetOpenLed: return "bonnet_open";
    case eMaterialType::DoorOpenLed: return "door_open";
    case eMaterialType::RoofOpenLed: return "roof_open";
    default: return nullptr;
    }
}

float LightManager::GetLightInertia(CVehicle* pVeh, VehLightData& data, eMaterialType type) {
    if (type >= 0 && type < eMaterialType::TotalMaterial && !data.dummies[type].empty()) {
        float dInertia = data.dummies[type][0].GetRef().inertia;
        if (dInertia > 0.0f) return dInertia;
    }

    if (!pVeh) return 0.0f;
    auto& json = DataMgr::Get(pVeh->m_nModelIndex);
    if (!json.contains("lights")) return 0.0f;
    auto& lights = json["lights"];

    const char* specKey = GetLightSpecificKey(type);
    if (specKey && lights.contains(specKey) && lights[specKey].contains("inertia")) {
        return lights[specKey].value("inertia", 0.0f);
    }

    const char* grpKey = GetLightGroupKey(type);
    if (grpKey && lights.contains(grpKey) && lights[grpKey].contains("inertia")) {
        return lights[grpKey].value("inertia", 0.0f);
    }

    return lights.value("inertia", 0.0f);
}

static std::optional<CRGBA> Helper_ParseLightColor(const nlohmann::json& val, const nlohmann::json* pRoot = nullptr) {
    if (val.is_string()) {
        std::string str = val.get<std::string>();
        if (pRoot && pRoot->contains("colors") && (*pRoot)["colors"].contains(str)) {
            return Helper_ParseLightColor((*pRoot)["colors"][str], pRoot);
        }
        std::string hexStr = str;
        if (hexStr.length() >= 6) {
            if (hexStr[0] == '#') hexStr = hexStr.substr(1);
            else if (hexStr.rfind("0x", 0) == 0 || hexStr.rfind("0X", 0) == 0) hexStr = hexStr.substr(2);
            if (hexStr.length() == 6 || hexStr.length() == 8) {
                unsigned int hexVal = 0;
                std::stringstream ss;
                ss << std::hex << hexStr;
                if (ss >> hexVal) {
                    if (hexStr.length() == 6) {
                        return CRGBA((hexVal >> 16) & 0xFF, (hexVal >> 8) & 0xFF, hexVal & 0xFF, 255);
                    } else {
                        return CRGBA((hexVal >> 24) & 0xFF, (hexVal >> 16) & 0xFF, (hexVal >> 8) & 0xFF, hexVal & 0xFF);
                    }
                }
            }
        }
        return std::nullopt;
    }
    if (val.is_object()) {
        uint8_t r = val.value("red", val.value("r", 255));
        uint8_t g = val.value("green", val.value("g", 255));
        uint8_t b = val.value("blue", val.value("b", 255));
        uint8_t a = val.value("alpha", val.value("a", 255));
        return CRGBA(r, g, b, a);
    }
    if (val.is_array() && val.size() >= 3) {
        return CRGBA(val[0].get<uint8_t>(), val[1].get<uint8_t>(), val[2].get<uint8_t>(),
                     val.size() >= 4 ? val[3].get<uint8_t>() : 255);
    }
    return std::nullopt;
}

static std::optional<CRGBA> GetJsonCoronaColor(const nlohmann::json& lights, const char* key, const nlohmann::json* pRoot = nullptr) {
    if (!key || !lights.contains(key)) return std::nullopt;
    const auto& sec = lights[key];
    const auto* pCol = sec.contains("corona") && sec["corona"].contains("color") ? &sec["corona"]["color"]
                     : sec.contains("color") ? &sec["color"]
                     : sec.contains("material") && sec["material"].contains("color") ? &sec["material"]["color"]
                     : nullptr;
    if (pCol) {
        return Helper_ParseLightColor(*pCol, pRoot);
    }
    return std::nullopt;
}

static std::optional<CRGBA> GetJsonOffColor(const nlohmann::json& lights, const char* key, const nlohmann::json* pRoot = nullptr) {
    if (!key || !lights.contains(key)) return std::nullopt;
    const auto& sec = lights[key];
    const auto* pSec = sec.contains("material") && sec["material"].contains("color_off") ? &sec["material"]["color_off"]
                     : sec.contains("color_off") ? &sec["color_off"]
                     : nullptr;
    if (pSec) {
        return Helper_ParseLightColor(*pSec, pRoot);
    }
    return std::nullopt;
}

MatStateColor LightManager::GetMaterialColor(CVehicle* pVeh, eMaterialType type) {
    if (type < 0 || type >= eMaterialType::TotalMaterial || !pVeh) {
        return MatStateColor{DEFAULT_MAT_COL, DEFAULT_MAT_COL};
    }
    VehLightData& data = m_VehData.Get(pVeh);

    std::optional<CRGBA> onCol;
    std::optional<CRGBA> offCol;

    if (IsDummyAvailable(data, type)) {
        const DummyConfig& c = data.dummies[type][0].GetRef();
        if (c.hasCustomColor) {
            onCol = c.corona.color;
        }
    }

    auto& json = DataMgr::Get(pVeh->m_nModelIndex);
    const char* specKey = GetLightSpecificKey(type);
    const char* grpKey = GetLightGroupKey(type);

    auto checkSec = [&](const nlohmann::json& container, const char* k) {
        if (!k || !container.contains(k)) return;
        if (!onCol) onCol = GetJsonCoronaColor(container, k, &json);
        if (!offCol) offCol = GetJsonOffColor(container, k, &json);
    };

    if (json.contains("lights")) {
        const auto& lights = json["lights"];
        if (specKey) checkSec(lights, specKey);
        if (type == eMaterialType::FogLightLeft) checkSec(lights, "fogl_l");
        if (type == eMaterialType::FogLightRight) checkSec(lights, "fogl_r");
        if (type == eMaterialType::ReverseLightLeft) { checkSec(lights, "revl_l"); checkSec(lights, "rev_l"); }
        if (type == eMaterialType::ReverseLightRight) { checkSec(lights, "revl_r"); checkSec(lights, "rev_r"); }
        if (type == eMaterialType::SpotLight) { checkSec(lights, "spotlights"); checkSec(lights, "spot_light"); }
        if (type == eMaterialType::StrobeLight) { checkSec(lights, "strobes"); checkSec(lights, "strobe"); checkSec(lights, "strobe_light"); }
        if (grpKey) checkSec(lights, grpKey);
    }

    if (type == eMaterialType::SpotLight) {
        if (json.contains("spotlights")) {
            if (!onCol) onCol = Helper_ParseLightColor(json["spotlights"].contains("color") ? json["spotlights"]["color"] : json["spotlights"], &json);
            if (!offCol && json["spotlights"].contains("color_off")) offCol = Helper_ParseLightColor(json["spotlights"]["color_off"], &json);
        }
        if (json.contains("spotlight")) {
            if (!onCol) onCol = Helper_ParseLightColor(json["spotlight"].contains("color") ? json["spotlight"]["color"] : json["spotlight"], &json);
            if (!offCol && json["spotlight"].contains("color_off")) offCol = Helper_ParseLightColor(json["spotlight"]["color_off"], &json);
        }
    }

    if (json.contains("leds") && specKey) {
        checkSec(json["leds"], specKey);
        if (grpKey) checkSec(json["leds"], grpKey);
    }

    if (onCol || offCol) {
        return MatStateColor{onCol.value_or(DEFAULT_MAT_COL), offCol.value_or(DEFAULT_MAT_COL)};
    }

    return MatStateColor{DEFAULT_MAT_COL, DEFAULT_MAT_COL};
}

