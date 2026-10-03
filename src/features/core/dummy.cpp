#include "pch.h"
#include "dummy.h"
#include "defines.h"
#include "features/lights/manager.h"
#include "utils/datamgr.h"
#include "enums/dummypos.h"
#include <cstdint>
#include <CWorld.h>
#include <CBike.h>
#include <CAutomobile.h>
#include <CModelInfo.h>

extern float gfGlobalCoronaSize;
extern int gGlobalCoronaIntensity;
extern int gGlobalShadowIntensity;

static int ReadHexDigit(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1;
}

int ReadHex(char a, char b)
{
    int high = ReadHexDigit(a), low = ReadHexDigit(b);
    return high >= 0 && low >= 0 ? (high << 4) + low : -1;
}

VehicleDummy::VehicleDummy(const DummyConfig& config)
{
    data = config;
    float angleVal = 0.0f;

    // Calculate the angle based on the frame's orientation
    data.rotation.angle = static_cast<float>(Util::RadToDeg(CGeneral::GetATanOfXY(data.frame->modelling.right.x, data.frame->modelling.right.y)));

    auto &jsonData = DataMgr::Get(data.pVeh->m_nModelIndex);
    std::string_view name = GetSafeFrameNodeName(data.frame);

    std::string_view parentName = GetSafeFrameNodeName(RwFrameGetParent(data.frame));
    if (parentName.ends_with("_dummy")) {
        data.isParentDummy = true;
    }

    // Pre-resolve damage component and bike lean inheritance in a single hierarchy walk
    for (RwFrame *pParent = RwFrameGetParent(data.frame); pParent; pParent = RwFrameGetParent(pParent)) {
        std::string_view pName = GetSafeFrameNodeName(pParent);
        if (pName.ends_with("_dummy")) {
            data.leanAffected = true;
        }
        if (data.damagePanel == -1 && data.damageDoor == -1) {
            if (pName.starts_with("bump_front") || pName.starts_with("bump_f")) {
                data.damagePanel = static_cast<int8_t>(ePanels::BUMP_FRONT);
            } else if (pName.starts_with("bump_rear") || pName.starts_with("bump_r")) {
                data.damagePanel = static_cast<int8_t>(ePanels::BUMP_REAR);
            } else if (pName.starts_with("wing_lf")) {
                data.damagePanel = static_cast<int8_t>(ePanels::WING_FRONT_LEFT);
            } else if (pName.starts_with("wing_rf")) {
                data.damagePanel = static_cast<int8_t>(ePanels::WING_FRONT_RIGHT);
            } else if (pName.starts_with("wing_lr")) {
                data.damagePanel = static_cast<int8_t>(ePanels::WING_REAR_LEFT);
            } else if (pName.starts_with("wing_rr")) {
                data.damagePanel = static_cast<int8_t>(ePanels::WING_REAR_RIGHT);
            } else if (pName.starts_with("bonnet")) {
                data.damageDoor = static_cast<int8_t>(eDoors::BONNET);
            } else if (pName.starts_with("boot")) {
                data.damageDoor = static_cast<int8_t>(eDoors::BOOT);
            } else if (pName.starts_with("windscreen")) {
                data.damagePanel = static_cast<int8_t>(ePanels::WINDSCREEN);
            } else if (pName.starts_with("door_lf")) {
                data.damageDoor = static_cast<int8_t>(eDoors::DOOR_FRONT_LEFT);
            } else if (pName.starts_with("door_rf")) {
                data.damageDoor = static_cast<int8_t>(eDoors::DOOR_FRONT_RIGHT);
            } else if (pName.starts_with("door_lr")) {
                data.damageDoor = static_cast<int8_t>(eDoors::DOOR_REAR_LEFT);
            } else if (pName.starts_with("door_rr")) {
                data.damageDoor = static_cast<int8_t>(eDoors::DOOR_REAR_RIGHT);
            }
        }
    }

    bool legacyDefaults = false;
    if (LightsConfig::Get().bLegacyDummyDefaults) {
        const bool indicator = name.starts_with("turnl") || name.starts_with("indicator");
        const bool brake = name.starts_with("breakl");
        const bool fog = name.starts_with("fogl");
        const bool reverse = name.starts_with("revl") || name.starts_with("reversingl");
        if (indicator || brake || fog || reverse) {
            legacyDefaults = true;
            unsigned char red = 255, green = 255, blue = 255;
            if (indicator) { red = 240; green = 180; blue = 0; }
            else if (brake) { red = 200; green = 0; blue = 0; }
            else if (fog) { red = 200; green = 200; blue = 220; }
            data.corona.color = {red, green, blue, data.corona.color.a};
            data.shadow.color = {red, green, blue, data.shadow.color.a};
            data.corona.lightingType = (indicator || fog) ? eLightingMode::Directional : eLightingMode::Inversed;
            data.corona.size = static_cast<float>((fog ? 6.0 : 3.0) / 15.0 - 0.05);
            data.shadow.render = indicator;
        }
    }

    // Legacy support for ImVehFt vehicles
    size_t prmPos = name.find("prm");
    if (prmPos != std::string::npos)
    {
        const auto prm = name.substr(prmPos + 3);
        if (prm.size() >= 6)
        {
            int red = ReadHex(prm[0], prm[1]);
            int green = ReadHex(prm[2], prm[3]);
            int blue = ReadHex(prm[4], prm[5]);
            if (red >= 0 && green >= 0 && blue >= 0) {
                data.shadow.color.r = data.corona.color.r = red;
                data.shadow.color.g = data.corona.color.g = green;
                data.shadow.color.b = data.corona.color.b = blue;
            }
        }
        else
        {
            LOG_VERBOSE("Model {} has issue with node `{}`: invalid color format", data.pVeh->m_nModelIndex, name);
        }

        if (prm.size() > 6 && ReadHexDigit(prm[6]) >= 0)
        {
            int type = ReadHexDigit(prm[6]);
            if (type == 2)
                data.corona.lightingType = eLightingMode::NonDirectional;
            else if (type == 1)
                data.corona.lightingType = eLightingMode::Inversed;
            else
                data.corona.lightingType = eLightingMode::Directional;
        }

        if (prm.size() > 7 && ReadHexDigit(prm[7]) >= 0)
        {
            const int size = ReadHexDigit(prm[7]);
            data.corona.size = legacyDefaults ? (size == 0 ? 0.0f : static_cast<float>(size / 15.0 - 0.05)) : static_cast<float>(size) / 10.0f;
        }

        if (prm.size() > 8 && ReadHexDigit(prm[8]) >= 0)
        {
            data.shadow.size = static_cast<float>(ReadHexDigit(prm[8])) / 7.5f;

            if (legacyDefaults || data.shadow.size > 0.0f) {
                data.shadow.render = data.shadow.size > 0.0f;
            }
        }
    }

    if (jsonData.contains("lights"))
    {
        size_t nameEnd = prmPos;
        if (nameEnd != std::string_view::npos && nameEnd > 0 && name[nameEnd - 1] == '_') --nameEnd;
        std::string newName(name.substr(0, nameEnd));
        const nlohmann::json* pLightsSec = nullptr;
        if (jsonData["lights"].contains(newName))
        {
            pLightsSec = &jsonData["lights"][newName];
        }
        else
        {
            const char* fallbackKey = LightManager::GetLightGroupKey(data.lightType);
            if (fallbackKey && jsonData["lights"].contains(fallbackKey))
            {
                pLightsSec = &jsonData["lights"][fallbackKey];
            }
        }

        if (pLightsSec)
        {
            auto &lights = *pLightsSec;

            if (lights.contains("corona"))
            {
                auto &coronaSec = lights["corona"];
                if (coronaSec.contains("color"))
                {
                    data.hasCustomColor = true;
                    data.corona.color.r = coronaSec["color"].value("red", data.corona.color.r);
                    data.corona.color.g = coronaSec["color"].value("green", data.corona.color.g);
                    data.corona.color.b = coronaSec["color"].value("blue", data.corona.color.b);
                    data.corona.color.a = coronaSec["color"].value("alpha", gGlobalCoronaIntensity);
                }
                data.corona.size = coronaSec.value("size", prmPos != std::string_view::npos || legacyDefaults ? data.corona.size : gfGlobalCoronaSize);
                if (coronaSec.contains("type") || (prmPos == std::string_view::npos && !legacyDefaults))
                    data.corona.lightingType = GetLightingMode(coronaSec.value("type", "directional"));
            }

            if (lights.contains("shadow"))
            {
                auto &shadow = lights["shadow"];
                if (shadow.contains("color"))
                {
                    data.shadow.color.r = shadow["color"].value("red", data.shadow.color.r);
                    data.shadow.color.g = shadow["color"].value("green", data.shadow.color.g);
                    data.shadow.color.b = shadow["color"].value("blue", data.shadow.color.b);
                    data.shadow.color.a = shadow["color"].value("alpha", gGlobalShadowIntensity);
                }
                data.shadow.size = shadow.value("size", prmPos != std::string_view::npos ? data.shadow.size : 1.0f);
                data.shadow.texture = shadow.value("texture", "");
                data.shadow.rotationChecks = shadow.value("rotationchecks", true);

                // shadows will be force enabled if there is JSON data for it.
                data.shadow.render = true;
            }

            if (lights.contains("inertia"))
            {
                data.inertia = lights.value("inertia", 0.0f);
            }

            // Inherit corona color to shadow if no explicit shadow color was set
            if (data.hasCustomColor && !(lights.contains("shadow") && lights["shadow"].contains("color")))
            {
                data.shadow.color.r = data.corona.color.r;
                data.shadow.color.g = data.corona.color.g;
                data.shadow.color.b = data.corona.color.b;
            }

            // Only for StrobeLights
            if (lights.contains("strobedelay"))
            {
                data.strobe.delay = lights.value("strobedelay", 1000);
            }
        }
    }
}

void VehicleDummy::Update() {
    if (!data.frame || !data.pVeh) return;
    RwFrameGetLTM(data.frame);

    // The corona is expanded again through the entity matrix, so the offset has to be
    // taken apart with the matrix that placed the frame. On a bike the lights sit under
    // chassis_dummy, which is rolled by m_mLeanMatrix, so using the entity matrix here
    // leaves the lean in the offset and the corona tilts with it. The lean matrix is
    // only valid for the frame that calculated it, so refresh it when the flag is down
    // and put the flag back so the game still recalculates it when it needs to.
    const CMatrix *pBasis = &data.pVeh->GetMatrix();
    CMatrix leanBasis;
    if (data.pVeh->m_nVehicleSubClass == VEHICLE_BIKE && data.leanAffected)
    {
        CBike *pBike = static_cast<CBike *>(data.pVeh);
        bool wasCalculated = pBike->m_bLeanMatrixCalculated;
        if (!wasCalculated)
        {
            pBike->CalculateLeanMatrix();
        }

        leanBasis = pBike->m_mLeanMatrix;
        pBasis = &leanBasis;
        pBike->m_bLeanMatrixCalculated = wasCalculated;
    }
    const CMatrix &basis = *pBasis;

    CVector offset = data.frame->ltm.pos - basis.pos;

    // Transform to local space using  transpose of the rotation matrix
    data.shadow.position.x = data.position.x = basis.right.x * offset.x + basis.right.y * offset.y + basis.right.z * offset.z;
    data.shadow.position.y = data.position.y = basis.up.x * offset.x + basis.up.y * offset.y + basis.up.z * offset.z;
    data.shadow.position.z = data.position.z = basis.at.x * offset.x + basis.at.y * offset.y + basis.at.z * offset.z;

    // Protection against buggy DFF models where child dummies are exported with root-space coordinates
    // (causing RenderWare hierarchy translation to double-offset the position far beyond the vehicle body)
    CVehicleModelInfo *pInfo = static_cast<CVehicleModelInfo *>(CModelInfo::GetModelInfo(data.pVeh->m_nModelIndex));
    if (pInfo && pInfo->m_pColModel)
    {
        const auto &box = pInfo->m_pColModel->m_boundBox;
        const float MARGIN = 0.5f;
        if (data.position.y > box.m_vecMax.y + MARGIN || data.position.y < box.m_vecMin.y - MARGIN
            || data.position.x > box.m_vecMax.x + MARGIN || data.position.x < box.m_vecMin.x - MARGIN
            || data.position.z > box.m_vecMax.z + MARGIN || data.position.z < box.m_vecMin.z - MARGIN)
        {
            data.position = data.frame->modelling.pos;
            data.shadow.position = data.frame->modelling.pos;
        }
    }

    if (data.mirroredX)
    {
        data.position.x *= -1;
        data.shadow.position.x *= -1;
    }
}
