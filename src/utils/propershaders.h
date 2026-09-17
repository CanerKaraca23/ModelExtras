#pragma once
#include <plugin.h>
#include <CRGBA.h>
#include <CVector.h>
#include <ProperShadersAPI.h>

class ProperShadersMgr
{
private:
    static inline PS_Api m_api{};
    static inline bool m_bInitialized = false;
    static inline bool m_bAvailable = false;

public:
    static void Init();
    static bool IsAvailable();
    static const PS_Api& GetApi();

    // Scene & Render Pass Queries
    static bool IsMainScenePass();
    static bool IsDeferredEnabled();
    static bool IsGlobalShadowsActive();
    static bool IsHDRActive();

    // High-Level Light Wrappers
    static PS_LightHandle CreateSpotLight(
        const CVector& pos,
        const CVector& dir,
        float radius,
        float spotAngle,
        const CRGBA& color,
        float intensity = 1.0f,
        bool bVolumetricBeam = true,
        bool bFog = true,
        unsigned int flags = PS_LIGHTFLAG_NONE,
        unsigned int lifetimeMs = 0,
        unsigned int fadeOutMs = 0
    );

    static PS_LightHandle CreatePointLight(
        const CVector& pos,
        float radius,
        const CRGBA& color,
        float intensity = 1.0f,
        bool bFog = false,
        unsigned int flags = PS_LIGHTFLAG_NONE,
        unsigned int lifetimeMs = 0,
        unsigned int fadeOutMs = 0
    );

    static PS_LightHandle CreateOneShotPointLight(
        const CVector& pos,
        float radius,
        const CRGBA& color,
        float intensity,
        unsigned int lifetimeMs,
        unsigned int fadeOutMs,
        bool bFog = true,
        unsigned int flags = PS_LIGHTFLAG_NONE
    );

    static bool DestroyLight(PS_LightHandle& hLight);
    static bool SetPosition(PS_LightHandle hLight, const CVector& pos);
    static bool SetDirection(PS_LightHandle hLight, const CVector& dir);
    static bool SetColor(PS_LightHandle hLight, const CRGBA& color, float intensity);
    static bool SetRadius(PS_LightHandle hLight, float radius);
    static bool SetEnabled(PS_LightHandle hLight, bool bEnabled);
};
