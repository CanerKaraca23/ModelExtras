#pragma once
#include <plugin.h>
#include "core/base.h"

struct TrailerHitchData {
    RwFrame *hitchFrame = nullptr; // trhook or f_hitch
    TrailerHitchData(CVehicle *pVeh) {}
    ~TrailerHitchData() {}
};

class TrailerHitchFeature : public CVehFeature<TrailerHitchData>
{
protected:
    void Init() override;

public:
    TrailerHitchFeature() : CVehFeature<TrailerHitchData>("TrailerHitch", "FEATURES", eFeatureMatrix::TrailerHitch) {}
};
