#include "pch.h"
#include "ModelExtrasAPI.h"
#include "defines.h"
#include "loader.h"
#include "features/sirens.h"

extern "C" {
int ME_GetAPIVersion() { return ME_API_VERSION; }

int ME_GetVersion() { return MOD_VERSION_NUMBER; }

bool ME_IsFeatureAvail(ME_FeatureID featureId) {
  auto idx = static_cast<int>(featureId);
  if (idx < 0 || idx >= static_cast<int>(eFeatureMatrix::FeatureCount)) {
    return false;
  }
  return ModelExtras::m_bEnabledFeatures.test(idx);
}

// Dummy function to show on crash logs
int __declspec(dllexport) ignore1(int i) { return 1; }

// Sirens API
int ME_GetSirenStateCount(CVehicle *pVeh) {
  if (!pVeh) return 0;
  auto *pModelData = Sirens::GetModelData(pVeh->m_nModelIndex);
  if (!pModelData) return 0;
  return static_cast<int>(pModelData->States.size());
}

int ME_GetSirenStateCountByModel(int modelIndex) {
  if (modelIndex < 0 || modelIndex >= 20000) return 0;
  auto *pModelData = Sirens::GetModelData(modelIndex);
  if (!pModelData) return 0;
  return static_cast<int>(pModelData->States.size());
}

int ME_GetSirenState(CVehicle *pVeh) {
  if (!pVeh) return -1;
  auto *pModelData = Sirens::GetModelData(pVeh->m_nModelIndex);
  if (!pModelData || pModelData->States.empty()) return -1;
  auto *pData = Sirens::GetVehicleData(pVeh);
  if (!pData) return -1;
  return pData->State;
}

bool ME_SetSirenState(CVehicle *pVeh, int state) {
  if (!pVeh) return false;
  auto *pModelData = Sirens::GetModelData(pVeh->m_nModelIndex);
  if (!pModelData || pModelData->States.empty()) return false;
  if (state < 0 || state >= static_cast<int>(pModelData->States.size())) return false;
  auto *pData = Sirens::GetVehicleData(pVeh);
  if (!pData) return false;
  pData->vehicle = pVeh;
  pData->State = state;
  return true;
}

bool ME_GetSirenMute(CVehicle *pVeh) {
  if (!pVeh) return false;
  auto *pData = Sirens::GetVehicleData(pVeh);
  if (!pData) return false;
  return pData->Mute;
}

void ME_SetSirenMute(CVehicle *pVeh, bool mute) {
  if (!pVeh) return;
  auto *pData = Sirens::GetVehicleData(pVeh);
  if (!pData) return;
  pData->vehicle = pVeh;
  pData->Mute = mute;
  if (mute) {
    pVeh->bSirenOrAlarm = false;
  }
}

bool ME_IsSirenActive(CVehicle *pVeh) {
  if (!pVeh) return false;
  auto *pData = Sirens::GetVehicleData(pVeh);
  if (!pData) return false;
  return pData->GetSirenState();
}

bool ME_IsSirenVehicle(CVehicle *pVeh) {
  if (!pVeh) return false;
  return Sirens::IsSirenModel(pVeh->m_nModelIndex);
}
}
