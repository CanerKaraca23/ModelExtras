#pragma once
#include <plugin.h>
#include "core/base.h"
#include <vector>

#include "utils/propershaders.h"

struct SpotlightData
{
	RwFrame *pFrame = nullptr;
	RwV3d origPos{0.0f, 0.0f, 0.0f};
	bool bHasOrigPos = false;
	bool bEnabled = false;
	unsigned int nLastFrame = 0;
	PS_LightHandle hPsLight = PS_INVALID_LIGHT;
	SpotlightData(CVehicle *pVeh) {}
	~SpotlightData()
	{
		ProperShadersMgr::DestroyLight(hPsLight);
	}
};

class SpotLights : public CVehFeature<SpotlightData>
{
protected:
    void Init() override;

public:
	static inline RwTexture *pSpotlightTex = nullptr;

	static void OnHudRender();
	static void OnVehicleRender(CVehicle *pVeh);
	static void ProcessPointLights(CVehicle *pVeh);

    SpotLights() : CVehFeature<SpotlightData>("SpotLights", "LIGHTS", eFeatureMatrix::SpotLights) {}
	static bool IsEnabled(CVehicle *pVeh);
	void ReloadConfig() override;
	void Reload(CVehicle *pVeh) override;
};