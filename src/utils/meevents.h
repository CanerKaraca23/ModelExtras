#pragma once
#include <Events.h>
#include <game_vc/CModelInfo.h>
#include <Patch.h>

namespace MEEvents
{
    using namespace plugin;
    // Vehicle render event (covers all vehicle types safely in Vice City)
    static inline auto &heliRenderEvent = Events::vehicleRenderEvent;
    static inline auto &vehRenderEvent = Events::vehicleRenderEvent;
    static inline auto &vehPreRenderEvent = Events::vehicleRenderEvent;
}