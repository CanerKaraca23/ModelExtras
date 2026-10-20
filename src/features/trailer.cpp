#include "pch.h"
#include "trailer.h"
#include "utils/adconfig.h"
#include "utils/inputmgr.h"
#include "utils/modelinfomgr.h"
#include <extensions/ScriptCommands.h>
#include <CMessages.h>
#include <CPools.h>

void TrailerHitchFeature::Init()
{
    LOG_VERBOSE("Init {}", __FUNCTION__);
    ModelInfoMgr::RegisterDummy([](CVehicle *pVeh, RwFrame *pFrame, const std::string_view nodeName)
    {
        if (!pVeh || !pFrame) return;

        if (nodeName == "trhook" || nodeName == "f_hitch" || nodeName.starts_with("towbar") ||
            nodeName.starts_with("trailer_hitch") || nodeName == "prizer") {
            TrailerHitchData &data = m_VehData.Get(pVeh);
            data.hitchFrame = pFrame;
        }
    });

    Events::processScriptsEvent += []()
    {
        if (!CBaseFeature::IsEnabled(eFeatureMatrix::TrailerHitch)) return;
        if (!CPools::ms_pVehiclePool) return;

        CVehicle *pVeh = FindPlayerVehicle(-1, false);
        if (!pVeh || pVeh->m_nVehicleSubClass == VEHICLE_TRAILER) return;

        const auto &keys = ADConfig::Keys();
        if (InputMgr::IsKeyJustDown(keys.keyTrailer)) {
            if (pVeh->m_pTrailer) {
                // Detach
                CVehicle *trailer = pVeh->m_pTrailer;
                plugin::Command<0x07AC>(CPools::GetVehicleRef(trailer), CPools::GetVehicleRef(pVeh));
                static char msg[] = "~y~Trailer detached";
                CMessages::AddMessageWithString(msg, 1500, false, nullptr, true);
            } else {
                // Find trailer nearby
                CVector vehPos = pVeh->GetPosition();
                CVehicle *bestTrailer = nullptr;
                float bestDistSq = 36.0f; // 6m max

                for (CVehicle *other : CPools::ms_pVehiclePool) {
                    if (!other || other == pVeh || other->m_nVehicleSubClass != VEHICLE_TRAILER) continue;
                    if (other->m_pTrailer || other->m_pTractor) continue;

                    float distSq = (other->GetPosition() - vehPos).MagnitudeSqr();
                    if (distSq < bestDistSq) {
                        bestDistSq = distSq;
                        bestTrailer = other;
                    }
                }

                if (bestTrailer) {
                    plugin::Command<0x0893>(CPools::GetVehicleRef(bestTrailer), CPools::GetVehicleRef(pVeh));
                    static char msg[] = "~g~Trailer hitched";
                    CMessages::AddMessageWithString(msg, 1500, false, nullptr, true);
                }
            }
        }
    };
}
