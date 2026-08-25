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
#include "features/dirtfx.h"
#include "features/plate.h"
#include "features/remap.h"
#include "defines.h"
#include "utils/meevents.h"
#include "utils/texmgr.h"
#include <CAutomobile.h>
#include <CVisibilityPlugins.h>

using namespace plugin;

extern int GetSirenIndex(CVehicle *pVeh, RpMaterial *pMat);
extern int GetStrobeIndex(CVehicle *pVeh, RpMaterial *pMat);

static CVehicle *pCurVeh = nullptr;
RwSurfaceProperties gLightSurfProps = {1.0f, 0.0f, 0.0f};
RwSurfaceProperties gLightSurfPropsOff = {0.45f, 0.0f, 0.0f};

static constexpr uint32_t RwFrameForAllObjectsAddr = 0x7F1200;
static constexpr uint32_t RwFrameAddChildAddr = 0x7F0B00;
static constexpr uint32_t RwFrameForAllChildrenAddr = 0x7F0DC0;
static constexpr uint32_t GetCurrentAtomicObjectCBAddr = 0x6D33B0;

static RwFrame *FindFrameById(RwFrame *frame, int id) {
  if (!frame) {
    return nullptr;
  }
  if (CVisibilityPlugins::GetFrameHierarchyId(frame) == id) {
    return frame;
  }
  if (RwFrame *child = frame->child) {
    if (RwFrame *found = FindFrameById(child, id)) {
      return found;
    }
  }
  if (RwFrame *next = frame->next) {
    if (RwFrame *found = FindFrameById(next, id)) {
      return found;
    }
  }
  return nullptr;
}

static int __fastcall hkAddReplacementUpgrade(CVehicle *pVeh, void *edx, int nodeId, int modelId) {
  if (!pVeh || !pVeh->m_pRwClump || nodeId < 0) {
    return -1;
  }
  RwFrame *pRoot = reinterpret_cast<RwFrame *>(pVeh->m_pRwClump->object.parent);
  if (!pRoot) {
    return -1;
  }
  RwFrame *pFrame = FindFrameById(pRoot, nodeId);
  if (!pFrame) {
    return -1;
  }
  return reinterpret_cast<int(__thiscall *)(CVehicle *, int, int)>(0x6D3830)(pVeh, nodeId, modelId);
}

static RwFrame *UpgradeFrameForAllObjects(RwFrame *frame,
                                          RwObjectCallBack callback,
                                          void *data) {
  if (!frame) {
    if (data && callback == reinterpret_cast<RwObjectCallBack>(
                                GetCurrentAtomicObjectCBAddr)) {
      *reinterpret_cast<void **>(data) = nullptr;
    }
    return frame;
  }
  return RwFrameForAllObjects(frame, callback, data);
}

static RwFrame *UpgradeFrameAddChild(RwFrame *parent, RwFrame *child) {
  if (!parent) {
    return parent;
  }
  return RwFrameAddChild(parent, child);
}

static RwFrame *UpgradeFrameForAllChildren(RwFrame *frame,
                                           RwFrameCallBack callback,
                                           void *data) {
  if (!frame) {
    return frame;
  }
  return RwFrameForAllChildren(frame, callback, data);
}

static size_t GuardUpgradeFrameCalls(uint32_t start, uint32_t end) {
  size_t patched = 0;
  for (uint32_t addr = start; addr < end; ++addr) {
    if (*reinterpret_cast<uint8_t *>(addr) != 0xE8) {
      continue;
    }

    uint32_t target = addr + 5 + *reinterpret_cast<int32_t *>(addr + 1);
    if (target == RwFrameForAllObjectsAddr) {
      patch::ReplaceFunctionCall(
          addr, reinterpret_cast<void *>(UpgradeFrameForAllObjects));
      ++patched;
    } else if (target == RwFrameAddChildAddr) {
      patch::ReplaceFunctionCall(
          addr, reinterpret_cast<void *>(UpgradeFrameAddChild));
      ++patched;
    } else if (target == RwFrameForAllChildrenAddr) {
      patch::ReplaceFunctionCall(
          addr, reinterpret_cast<void *>(UpgradeFrameForAllChildren));
      ++patched;
    }
  }
  return patched;
}

static size_t GuardAddReplacementUpgradeCalls(uint32_t start, uint32_t end) {
  size_t patched = 0;
  for (uint32_t addr = start; addr < end; ++addr) {
    if (*reinterpret_cast<uint8_t *>(addr) != 0xE8) {
      continue;
    }
    uint32_t target = addr + 5 + *reinterpret_cast<int32_t *>(addr + 1);
    if (target == 0x6D3830) {
      patch::ReplaceFunctionCall(
          addr, reinterpret_cast<void *>(hkAddReplacementUpgrade));
      ++patched;
    }
  }
  return patched;
}

void ModelInfoMgr::ResetEditableMaterials() {
  for (auto it = m_RestoreEntries.rbegin(); it != m_RestoreEntries.rend(); ++it) {
    if (it->m_pAddress) {
      *reinterpret_cast<void **>(it->m_pAddress) = it->m_pValue;
    }
  }
  m_RestoreEntries.clear();
}

void ModelInfoMgr::Init() {
  m_RestoreEntries.reserve(256);

  patch::Nop(0x4C8E53, 5);
  patch::Nop(0x4C8F6E, 5);

  GuardAddReplacementUpgradeCalls(0x6E3200, 0x6E3400);
  GuardAddReplacementUpgradeCalls(0x6DF900, 0x6DFC00);

  GuardUpgradeFrameCalls(0x6D3300, 0x6D3C00);
  GuardUpgradeFrameCalls(0x6DF900, 0x6DFC00);

  patch::ReplaceFunctionCall(
      0x5532A9, reinterpret_cast<void *>(ModelInfoMgr::SetupRender));
  patch::ReplaceFunction(
      0x4C8220, reinterpret_cast<void *>(ModelInfoMgr::SetEditableMaterialsCB));
  patch::ReplaceFunction(
      0x4C8460, reinterpret_cast<void *>(ModelInfoMgr::ResetEditableMaterials));

  Events::initScriptsEvent += []() {
    gLightSurfProps.ambient = gConfig.ReadFloat("VISUAL", "MaterialAmbientOn",
                                                gLightSurfProps.ambient);
    gLightSurfProps.diffuse =
        gConfig.ReadFloat("VISUAL", "MaterialDiffuseOn", 0.0f);
    gLightSurfProps.specular = 0.0f;
    gLightSurfPropsOff.ambient = gConfig.ReadFloat(
        "VISUAL", "MaterialAmbientOff", gLightSurfPropsOff.ambient);
    gLightSurfPropsOff.diffuse = 0.0f;
    gLightSurfPropsOff.specular = 0.0f;
  };

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
    if (pVeh && CModelInfo::IsHeliModel(pVeh->m_nModelIndex)) {
      ModelInfoMgr::OnRender(pVeh);
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
      DirtFx::ProcessTextures(pCurVeh, material);
      LicensePlate::ProcessTextures(pCurVeh, material);
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
      m_RestoreEntries.push_back({&material->texture, material->texture});

      if (material->texture) {
        if (material->texture == TextureMgr::FindInDict("vehiclelights128", material->texture->dict, true)) {
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
      material->surfaceProps = gLightSurfProps;
    } else {
      pColor->red = matCol.off.r;
      pColor->green = matCol.off.g;
      pColor->blue = matCol.off.b;
      material->surfaceProps = gLightSurfPropsOff;
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
