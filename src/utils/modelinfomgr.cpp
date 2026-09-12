#include "modelinfomgr.h"
#include "pch.h"

#include <CCamera.h>
#include <CTxdStore.h>
#include <NodeName.h>
#include <RenderWare.h>
#include <rpworld.h>
#include <rwcore.h>
#include <rwplcore.h>
#include <string_view>
#include <winuser.h>

#include "features/carcols.h"
#include "features/remap.h"
#include "features/lights/manager.h"
#include "defines.h"
#include "utils/meevents.h"
#include "utils/texmgr.h"

using namespace plugin;

extern int GetSirenIndex(CVehicle *pVeh, RpMaterial *pMat);
extern int GetStrobeIndex(CVehicle *pVeh, RpMaterial *pMat);

static CVehicle *pCurVeh = nullptr;

void ModelInfoMgr::ResetEditableMaterials() {
  for (auto it = m_RestoreEntries.rbegin(); it != m_RestoreEntries.rend(); ++it) {
    if (it->m_pAddress) {
      *reinterpret_cast<void **>(it->m_pAddress) = it->m_pValue;
    }
  }
  m_RestoreEntries.clear();

  for (auto it = m_SurfPropsRestoreEntries.rbegin(); it != m_SurfPropsRestoreEntries.rend(); ++it) {
    if (it->m_pMaterial) {
      it->m_pMaterial->surfaceProps = it->m_SurfProps;
    }
  }
  m_SurfPropsRestoreEntries.clear();
}

void ModelInfoMgr::ReloadConfig() {
  gfMaterialAmbientMul = std::max(0.0f, gConfig.ReadFloat("LIGHTS", "MaterialAmbientMul", 1.0f));
  RwSurfaceProperties baseProps{1.0f, 1.0f, 1.0f};
  baseProps.ambient = std::max(0.0f, baseProps.ambient * gfMaterialAmbientMul);
  ms_LightSurfaceProps = baseProps;
}

void ModelInfoMgr::Init() {
  m_RestoreEntries.reserve(256);
  m_SurfPropsRestoreEntries.reserve(256);

  ReloadConfig();

  // Hook VC 1.0 CVehicleModelInfo::SetEditableMaterialsCB (0x579AE0) and ResetEditableMaterials (0x5799D0)
  patch::ReplaceFunction(
      0x579AE0, reinterpret_cast<void *>(ModelInfoMgr::SetEditableMaterialsCB));
  patch::ReplaceFunction(
      0x5799D0, reinterpret_cast<void *>(ModelInfoMgr::ResetEditableMaterials));

  MEEvents::vehRenderEvent.before += [](CVehicle *pVeh) {
    if (!pVeh || !pVeh->m_pRwClump) {
      return;
    }

    auto &data = m_VehData.Get(pVeh);
    if (data.nFrameCount > 10) {
      ModelInfoMgr::OnRender(pVeh);
    } else if (data.nFrameCount == 10) {
      ModelInfoMgr::FindDummies(
          pVeh, reinterpret_cast<RwFrame *>(pVeh->m_pRwClump->object.parent));
      data.nFrameCount++;
    } else {
      data.nFrameCount++;
    }
  };

  MEEvents::heliRenderEvent.after += [](CVehicle *pVeh) {
    if (!pVeh || !pVeh->m_pRwClump) {
      return;
    }

    if (CModelInfo::IsHeliModel(pVeh->m_nModelIndex)) {
      auto &data = m_VehData.Get(pVeh);
      if (data.nFrameCount > 10) {
        ModelInfoMgr::OnRender(pVeh);
      } else if (data.nFrameCount == 10) {
        ModelInfoMgr::FindDummies(
            pVeh, reinterpret_cast<RwFrame *>(pVeh->m_pRwClump->object.parent));
        data.nFrameCount++;
      } else {
        data.nFrameCount++;
      }
    }
  };
}

void ModelInfoMgr::RegisterRender(const RenderCallback_t &render) {
  renders.push_back(render);
}

void ModelInfoMgr::RegisterDummy(const DummyCallback_t &function) {
  dummies.push_back(function);
}

void ModelInfoMgr::EnableMaterial(CVehicle *pVeh, eMaterialType type) {
  if (type >= 0 && type < eMaterialType::TotalMaterial) {
    auto &data = m_VehData.Get(pVeh);
    data.m_MatStatus[type] = true;
  }
}

void ModelInfoMgr::EnableSirenMaterial(CVehicle *pVeh, int idx) {
  if (idx >= 0 && idx < static_cast<int>(MAX_LIGHTS)) {
    auto &data = m_VehData.Get(pVeh);
    data.m_SirenStatus[idx] = true;
  }
}

void ModelInfoMgr::EnableStrobeMaterial(CVehicle *pVeh, int idx) {
  if (idx >= 0 && idx < static_cast<int>(MAX_LIGHTS)) {
    auto &data = m_VehData.Get(pVeh);
    data.m_StrobeStatus[idx] = true;
  }
}

void ModelInfoMgr::FindDummies(CVehicle *vehicle, RwFrame *frame) {
  if (!frame) {
    return;
  }

  if (RwFrame *nextFrame = frame->child) {
    FindDummies(vehicle, nextFrame);
  }

  if (RwFrame *nextFrame = frame->next) {
    FindDummies(vehicle, nextFrame);
  }

  if (!dummies.empty()) {
    std::string_view nodeName = GetFrameNodeName(frame);
    for (const auto &e : dummies) {
      e(vehicle, frame, nodeName);
    }
  }
}

void ModelInfoMgr::Reload(CVehicle *pVeh) {
  ReloadConfig();
  if (pVeh && pVeh->m_pRwClump) {
    RwFrame *frame =
        reinterpret_cast<RwFrame *>(pVeh->m_pRwClump->object.parent);
    FindDummies(pVeh, frame);
  }
}

void ModelInfoMgr::OnRender(CVehicle *vehicle) {
  for (const auto &e : renders) {
    e(vehicle);
  }
}

void ModelInfoMgr::RegisterMaterial(const MaterialCallback_t &mat) {
  materials.push_back(mat);
}

void ModelInfoMgr::RegisterMaterialColProvider(
    const MaterialColProviderCallback_t &mat) {
  matColProviders.push_back(mat);
}

void ModelInfoMgr::SetupRender(CVehicle *ptr) {
  if (!ptr) {
    return;
  }
  pCurVeh = ptr;
  auto &data = m_VehData.Get(pCurVeh);
  ptr->SetupRender();

  data.m_MatStatus.fill(false);
  data.m_SirenStatus.fill(false);
  data.m_StrobeStatus.fill(false);
}

MatStateColor ModelInfoMgr::FetchMaterialCol(CVehicle *pVeh, RpMaterial *pMat,
                                             eMaterialType type) {
  MatStateColor col = {DEFAULT_MAT_COL, DEFAULT_MAT_COL};
  for (const auto &e : matColProviders) {
    col = e(pVeh, pMat, type);
    if (col.on != DEFAULT_MAT_COL || col.off != DEFAULT_MAT_COL) {
      break;
    }
  }
  return col;
}

eMaterialType ModelInfoMgr::FetchMaterialType(CVehicle *pVeh,
                                              RpMaterial *pMat) {
  for (const auto &e : materials) {
    eMaterialType type = e(pVeh, pMat);
    if (type != eMaterialType::UnknownMaterial) {
      return type;
    }
  }
  return eMaterialType::UnknownMaterial;
}

RpMaterial *ModelInfoMgr::SetEditableMaterialsCB(RpMaterial *material,
                                                 void *data) {
  (void)data;
  if (!material) {
    return material;
  }

  if (material->texture) {
    const char *texName = material->texture->name;
    bool isRemapTex = (texName && texName[0] == '#');
    if (isRemapTex) {
      if (CVehicleModelInfo::ms_pRemapTexture) {
        m_RestoreEntries.push_back({&material->texture, material->texture});
        material->texture = CVehicleModelInfo::ms_pRemapTexture;
      }
    } else if (pCurVeh) {
      Remap::ProcessTextures(pCurVeh, material);
    }
  }

  if (!pCurVeh) {
    return material;
  }

  eMaterialType iLightIndex = FetchMaterialType(pCurVeh, material);

  if (iLightIndex != eMaterialType::UnknownMaterial && iLightIndex >= 0 &&
      iLightIndex < eMaterialType::TotalMaterial) {
    auto &vData = m_VehData.Get(pCurVeh);

    bool lightOn = false;
    vData.m_MatAvail[iLightIndex] = true;

    if (iLightIndex == eMaterialType::SirenLight) {
      int idx = GetSirenIndex(pCurVeh, material);
      if (idx >= 0 && idx < static_cast<int>(MAX_LIGHTS)) {
        lightOn = vData.m_SirenStatus[idx];
      }
    } else if (iLightIndex == eMaterialType::StrobeLight) {
      int idx = GetStrobeIndex(pCurVeh, material);
      if (idx >= 0 && idx < static_cast<int>(MAX_LIGHTS)) {
        lightOn = vData.m_StrobeStatus[idx];
      }
    } else {
      lightOn = vData.m_MatStatus[iLightIndex];
    }

    MatStateColor matCol = FetchMaterialCol(pCurVeh, material, iLightIndex);

    RwRGBA *pColor = RpMaterialGetColor(material);
    m_RestoreEntries.push_back({pColor, *reinterpret_cast<void **>(pColor)});

    pColor->red = matCol.on.r;
    pColor->green = matCol.on.g;
    pColor->blue = matCol.on.b;

    if (lightOn) {
      float factor = 1.0f;
      if (iLightIndex != eMaterialType::SirenLight &&
          iLightIndex != eMaterialType::SpotLight &&
          iLightIndex < eMaterialType::EngineOnLed &&
          iLightIndex >= 0 && iLightIndex < eMaterialType::TotalMaterial) {
        VehLightData &lData = LightManager::m_VehData.Get(pCurVeh);
        if (lData.fLightFactor[iLightIndex] > 0.001f) {
          factor = lData.fLightFactor[iLightIndex];
        }
      }
      m_RestoreEntries.push_back({&material->texture, material->texture});

      if (material->texture) {
        const char *matTexName = material->texture->name;
        if (matTexName && strcmp(matTexName, "vehiclelights128") == 0) {
          material->texture = TextureMgr::FindInDict("vehiclelightson128", material->texture->dict, true);
        } else if (material->texture == TextureMgr::FindInDict("vehiclelights128", material->texture->dict, true)) {
          material->texture = TextureMgr::FindInDict("vehiclelightson128", material->texture->dict, true);
        } else {
          RwTexture *pTex = TextureMgr::FindOnTextureInDict(
              material, material->texture->dict);
          if (pTex) {
            material->texture = pTex;
          } else {
            LOG_VERBOSE("Expected an 'on' texture for {} but none found",
                        material->texture->name);
          }
        }
      }
      m_SurfPropsRestoreEntries.push_back({material, material->surfaceProps});
      material->surfaceProps = GetLightSurfaceProps(factor);
    }
  } else {
    CRGBA col = {255, 255, 255, 255};
    if (Carcols::GetColor(pCurVeh, material, col)) {
      RwRGBA *pColor = RpMaterialGetColor(material);
      m_RestoreEntries.push_back({pColor, *reinterpret_cast<void **>(pColor)});

      pColor->red = col.r;
      pColor->green = col.g;
      pColor->blue = col.b;
    }
  }

  return material;
}

bool ModelInfoMgr::IsMaterialAvailable(CVehicle *pVeh, eMaterialType type) {
  if (type >= 0 && type < eMaterialType::TotalMaterial) {
    auto &data = m_VehData.Get(pVeh);
    return data.m_MatAvail[type];
  }
  return false;
}
