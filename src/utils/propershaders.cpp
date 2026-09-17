#include "pch.h"
#include "propershaders.h"
#include "defines.h"
#include <algorithm>

extern bool gbProperShadersDetected;

void ProperShadersMgr::Init()
{
    if (m_bInitialized)
    {
        return;
    }
    m_bInitialized = true;

    if (PS_ApiLoad(&m_api))
    {
        m_bAvailable = true;
        gbProperShadersDetected = true;
        int modVer = m_api.GetVersion ? m_api.GetVersion() : 0;
        LOG(INFO) << "ProperShaders API v" << m_api.version << " initialized successfully (mod version: " << modVer << ").";
    }
    else
    {
        m_bAvailable = false;
    }
}

bool ProperShadersMgr::IsAvailable()
{
    return m_bAvailable;
}

const PS_Api& ProperShadersMgr::GetApi()
{
    return m_api;
}

bool ProperShadersMgr::IsMainScenePass()
{
    if (!m_bAvailable || !m_api.IsMostCommonRenderCall)
    {
        return true;
    }
    return m_api.IsMostCommonRenderCall() != 0;
}

bool ProperShadersMgr::IsDeferredEnabled()
{
    if (!m_bAvailable || !m_api.IsUsingDeferredRenderer)
    {
        return false;
    }
    return m_api.IsUsingDeferredRenderer() != 0;
}

bool ProperShadersMgr::IsGlobalShadowsActive()
{
    if (!m_bAvailable || !m_api.GetGlobalShadowsInfluence)
    {
        return false;
    }
    return m_api.GetGlobalShadowsInfluence() > 0.01f;
}

bool ProperShadersMgr::IsHDRActive()
{
    if (!m_bAvailable || !m_api.IsUsingHDR)
    {
        return false;
    }
    return m_api.IsUsingHDR() != 0;
}

PS_LightHandle ProperShadersMgr::CreateSpotLight(
    const CVector& pos,
    const CVector& dir,
    float radius,
    float spotAngle,
    const CRGBA& color,
    float intensity,
    bool bVolumetricBeam,
    bool bFog,
    unsigned int flags,
    unsigned int lifetimeMs,
    unsigned int fadeOutMs)
{
    if (!m_bAvailable || !m_api.LightCreate)
    {
        return PS_INVALID_LIGHT;
    }

    PS_LightDesc d;
    PS_LightDescInit(&d);

    d.type = PS_LIGHT_SPOT;
    d.flags = flags;
    d.position[0] = pos.x;
    d.position[1] = pos.y;
    d.position[2] = pos.z;

    d.direction[0] = dir.x;
    d.direction[1] = dir.y;
    d.direction[2] = dir.z;

    d.radius = radius;
    d.spotAngle = std::clamp(spotAngle, 1.0f, 89.0f);

    d.color[0] = static_cast<float>(color.r) / 255.0f;
    d.color[1] = static_cast<float>(color.g) / 255.0f;
    d.color[2] = static_cast<float>(color.b) / 255.0f;
    d.intensity = intensity;

    d.beamMode = bVolumetricBeam ? PS_BEAMMODE_ALWAYS : PS_BEAMMODE_NONE;
    d.beamIntensity = 1.0f;
    d.fogMode = bFog ? PS_FOGMODE_NORMAL : PS_FOGMODE_NONE;
    d.fogIntensity = 1.0f;

    d.lifetimeMs = lifetimeMs;
    d.fadeOutMs = fadeOutMs;

    return m_api.LightCreate(&d);
}

PS_LightHandle ProperShadersMgr::CreatePointLight(
    const CVector& pos,
    float radius,
    const CRGBA& color,
    float intensity,
    bool bFog,
    unsigned int flags,
    unsigned int lifetimeMs,
    unsigned int fadeOutMs)
{
    if (!m_bAvailable || !m_api.LightCreate)
    {
        return PS_INVALID_LIGHT;
    }

    PS_LightDesc d;
    PS_LightDescInit(&d);

    d.type = PS_LIGHT_POINT;
    d.flags = flags;
    d.position[0] = pos.x;
    d.position[1] = pos.y;
    d.position[2] = pos.z;

    d.radius = radius;
    d.color[0] = static_cast<float>(color.r) / 255.0f;
    d.color[1] = static_cast<float>(color.g) / 255.0f;
    d.color[2] = static_cast<float>(color.b) / 255.0f;
    d.intensity = intensity;

    d.fogMode = bFog ? PS_FOGMODE_NORMAL : PS_FOGMODE_NONE;
    d.fogIntensity = 1.0f;
    d.beamMode = PS_BEAMMODE_NONE;

    d.lifetimeMs = lifetimeMs;
    d.fadeOutMs = fadeOutMs;

    return m_api.LightCreate(&d);
}

PS_LightHandle ProperShadersMgr::CreateOneShotPointLight(
    const CVector& pos,
    float radius,
    const CRGBA& color,
    float intensity,
    unsigned int lifetimeMs,
    unsigned int fadeOutMs,
    bool bFog,
    unsigned int flags)
{
    return CreatePointLight(pos, radius, color, intensity, bFog, flags, lifetimeMs, fadeOutMs);
}

bool ProperShadersMgr::DestroyLight(PS_LightHandle& hLight)
{
    if (hLight == PS_INVALID_LIGHT)
    {
        return false;
    }

    bool success = false;
    if (m_bAvailable && m_api.LightDestroy)
    {
        success = (m_api.LightDestroy(hLight) != 0);
    }
    hLight = PS_INVALID_LIGHT;
    return success;
}

bool ProperShadersMgr::SetPosition(PS_LightHandle hLight, const CVector& pos)
{
    if (!m_bAvailable || !m_api.LightSetPosition || hLight == PS_INVALID_LIGHT)
    {
        return false;
    }
    return m_api.LightSetPosition(hLight, pos.x, pos.y, pos.z) != 0;
}

bool ProperShadersMgr::SetDirection(PS_LightHandle hLight, const CVector& dir)
{
    if (!m_bAvailable || !m_api.LightSetDirection || hLight == PS_INVALID_LIGHT)
    {
        return false;
    }
    return m_api.LightSetDirection(hLight, dir.x, dir.y, dir.z) != 0;
}

bool ProperShadersMgr::SetColor(PS_LightHandle hLight, const CRGBA& color, float intensity)
{
    if (!m_bAvailable || !m_api.LightSetColor || hLight == PS_INVALID_LIGHT)
    {
        return false;
    }
    float r = static_cast<float>(color.r) / 255.0f;
    float g = static_cast<float>(color.g) / 255.0f;
    float b = static_cast<float>(color.b) / 255.0f;
    return m_api.LightSetColor(hLight, r, g, b, intensity) != 0;
}

bool ProperShadersMgr::SetRadius(PS_LightHandle hLight, float radius)
{
    if (!m_bAvailable || !m_api.LightSetRadius || hLight == PS_INVALID_LIGHT)
    {
        return false;
    }
    return m_api.LightSetRadius(hLight, radius) != 0;
}

bool ProperShadersMgr::SetEnabled(PS_LightHandle hLight, bool bEnabled)
{
    if (!m_bAvailable || !m_api.LightSetEnabled || hLight == PS_INVALID_LIGHT)
    {
        return false;
    }
    return m_api.LightSetEnabled(hLight, bEnabled ? 1 : 0) != 0;
}
