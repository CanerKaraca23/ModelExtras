#pragma once
#include <Events.h>
#include <game_vc/CModelInfo.h>
#include <Patch.h>

namespace MEEvents
{
    using namespace plugin;
    // Heli render
    static inline ThiscallEvent<AddressList<0x5B5B20, H_JUMP>, PRIORITY_AFTER, ArgPickN<CVehicle *, 0>, void(CVehicle *)> heliRenderEvent;
    // Vehicle render (CAutomobile::Render, CBike::Render, CBoat::Render, CPlane::Render)
    static inline ThiscallEvent<AddressList<0x589880, H_JUMP, 0x60D4D0, H_JUMP, 0x5C4E50, H_JUMP, 0x5C63B0, H_JUMP>, PRIORITY_BEFORE, ArgPickN<CVehicle *, 0>, void(CVehicle *)> vehRenderEvent;
    // PreRender (CAutomobile::PreRender, CBike::PreRender)
    static inline ThiscallEvent<AddressList<0x58D5F0, H_JUMP, 0x60DDA0, H_JUMP>, PRIORITY_AFTER, ArgPickN<CVehicle *, 0>, void(CVehicle *)> vehPreRenderEvent;
}