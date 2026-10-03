#include "pch.h"
#include "parentdamage.h"
#include "car.h"
#include "frameextension.h"
#include "features/core/base.h"
#include "meevents.h"

namespace {
struct HiddenAtomic {
    RpAtomic *atomic = nullptr;
    HiddenAtomic *previous = nullptr;
    HiddenAtomic *next = nullptr;
    bool hidden = false;
};

RwInt32 pluginOffset = -1;
HiddenAtomic *pending = nullptr;

HiddenAtomic *GetHiddenAtomic(RpAtomic *atomic) {
    return reinterpret_cast<HiddenAtomic *>(reinterpret_cast<uintptr_t>(atomic) + pluginOffset);
}

void Unlink(HiddenAtomic &entry) {
    if (!entry.hidden) return;
    if (entry.previous) entry.previous->next = entry.next;
    else pending = entry.next;
    if (entry.next) entry.next->previous = entry.previous;
    entry.previous = entry.next = nullptr;
    entry.hidden = false;
}

void Restore() {
    while (pending) {
        HiddenAtomic &entry = *pending;
        RpAtomicSetFlags(entry.atomic, RpAtomicGetFlags(entry.atomic) | rpATOMICRENDER);
        Unlink(entry);
    }
}

RwObject *HideAtomic(RwObject *object, void *) {
    if (!object || RwObjectGetType(object) != rpATOMIC) return object;
    auto *atomic = reinterpret_cast<RpAtomic *>(object);
    const auto flags = RpAtomicGetFlags(atomic);
    if (!(flags & rpATOMICRENDER)) return object;
    HiddenAtomic &entry = *GetHiddenAtomic(atomic);
    if (!entry.hidden) {
        entry.atomic = atomic;
        entry.previous = nullptr;
        entry.next = pending;
        if (pending) pending->previous = &entry;
        pending = &entry;
        entry.hidden = true;
    }
    RpAtomicSetFlags(atomic, flags & ~rpATOMICRENDER);
    return object;
}

void ApplyFrame(CVehicle *vehicle, RwFrame *frame, bool inherited) {
    if (!frame) return;
    const char *name = GetFrameNodeName(frame);
    const std::string_view node = name ? name : "";
    const bool extra = node.starts_with("extra") && (node.size() == 5 || node[5] != '_');
    const bool spoiler = node.starts_with("movspoiler") && CBaseFeature::IsEnabled(eFeatureMatrix::AnimatedSpoiler);
    const bool damaged = inherited || ((extra || spoiler) && CarUtil::IsLegacyParentDamaged(vehicle, frame));
    if (damaged) RwFrameForAllObjects(frame, HideAtomic, nullptr);
    for (RwFrame *child = frame->child; child; child = child->next) ApplyFrame(vehicle, child, damaged);
}
}

void ParentDamageVisibility::Apply(CVehicle *vehicle) {
    if (pluginOffset < 0 || !vehicle || !vehicle->m_pRwClump ||
        (vehicle->m_nVehicleSubClass != VEHICLE_AUTOMOBILE && vehicle->m_nVehicleSubClass != VEHICLE_MTRUCK &&
         vehicle->m_nVehicleSubClass != VEHICLE_QUAD)) return;
    ApplyFrame(vehicle, RpClumpGetFrame(vehicle->m_pRwClump), false);
}

void ParentDamageVisibility::Init() {
    plugin::Events::attachRwPluginsEvent += [] {
        pluginOffset = RpAtomicRegisterPlugin(sizeof(HiddenAtomic), PLUGIN_ID_NUM,
            [](void *object, RwInt32 offset, RwInt32) -> void * {
                *reinterpret_cast<HiddenAtomic *>(reinterpret_cast<uintptr_t>(object) + offset) = {};
                return object;
            },
            [](void *object, RwInt32 offset, RwInt32) -> void * {
                Unlink(*reinterpret_cast<HiddenAtomic *>(reinterpret_cast<uintptr_t>(object) + offset));
                return object;
            },
            [](void *object, const void *source, RwInt32 offset, RwInt32) -> void * {
                *reinterpret_cast<HiddenAtomic *>(reinterpret_cast<uintptr_t>(object) + offset) = {};
                const auto *original = reinterpret_cast<const HiddenAtomic *>(reinterpret_cast<uintptr_t>(source) + offset);
                if (original->hidden) {
                    auto *atomic = static_cast<RpAtomic *>(object);
                    RpAtomicSetFlags(atomic, RpAtomicGetFlags(atomic) | rpATOMICRENDER);
                }
                return object;
            });
    };
    plugin::Events::processScriptsEvent.before += Restore;
    plugin::Events::drawingEvent.after += Restore;
    plugin::Events::shutdownRwEvent += Restore;
    MEEvents::vehPreRenderEvent.after += Apply;
}
