#include "pch.h"
#include "defines.h"
#include "utils/texmgr.h"
#include <CTxdStore.h>
#include <rwcore.h>
#include <rwplcore.h>
#include <rpworld.h>
#include <RenderWare.h>
#include <CFileLoader.h>
#include <filesystem>

static const char *g_ShadowNames[] = {"headlight_short", "headlight_long", "taillight", "pointlight", "backfire"};
static const char *g_ShadowFiles[] = {"headlight_s.png", "headlight_l.png", "taillight.png", "pointlight.png", "backfire.png"};
static RwTexture *g_LegacyShadows[5]{};
static bool g_UseLegacyShadows = false;

void TextureMgr::Init()
{
    Events::initGameEvent.after += [] {
        g_UseLegacyShadows = gConfig.ReadBoolean("VISUAL", "UseLegacyShadows", false);
        const std::filesystem::path adjacent = PLUGIN_PATH((char *)"ImVehFt\\shadows");
        const std::filesystem::path game = GAME_PATH((char *)"ImVehFt\\shadows");
        for (size_t i = 0; i < 5; ++i) {
            if (g_LegacyShadows[i]) continue;
            for (const auto &root : {adjacent, game}) {
                const auto path = root / g_ShadowFiles[i];
                std::error_code ec;
                if (!std::filesystem::is_regular_file(path, ec)) continue;
                RwTexture *texture = LoadFromFile(path.string().c_str());
                if (!texture) continue;
                RwTextureSetName(texture, g_ShadowNames[i]);
                RwTextureSetFilterMode(texture, rwFILTERLINEAR);
                RwTextureSetAddressingU(texture, rwTEXTUREADDRESSCLAMP);
                RwTextureSetAddressingV(texture, rwTEXTUREADDRESSCLAMP);
                g_LegacyShadows[i] = texture;
                break;
            }
        }
    };
    Events::shutdownRwEvent += [] {
        for (auto &texture : g_LegacyShadows) {
            if (texture) RwTextureDestroy(texture);
            texture = nullptr;
        }
    };
}

RwTexture *TextureMgr::LoadFromFile(const char *filename, RwUInt8 alpha)
{
    if (!filename) return nullptr;
    RwImage *image = RtPNGImageRead(filename);
    if (!image)
    {
        return nullptr;
    }

    const int width = RwImageGetWidth(image), height = RwImageGetHeight(image);
    RwRaster *raster = width > 0 && height > 0 ? RwRasterCreate(width, height, 32, rwRASTERTYPETEXTURE | rwRASTERFORMAT8888) : nullptr;
    if (!raster)
    {
        RwImageDestroy(image);
        return nullptr;
    }
    if (!RwRasterSetFromImage(raster, image))
    {
        RwImageDestroy(image);
        RwRasterDestroy(raster);
        return nullptr;
    }
    RwImageDestroy(image);
    if (alpha != 255)
    {
        auto *pixels = RwRasterLock(raster, 0, rwRASTERLOCKREAD | rwRASTERLOCKWRITE);
        const int stride = RwRasterGetStride(raster);
        if (!pixels || stride <= 0 || width > stride / 4) {
            if (pixels) RwRasterUnlock(raster);
            RwRasterDestroy(raster);
            return nullptr;
        }
        for (RwInt32 y = 0; y < height; y++)
        {
            for (RwInt32 x = 0; x < width; x++)
            {
                RwRGBA *pixel = reinterpret_cast<RwRGBA *>(pixels + y * stride + x * 4);
                pixel->red = (pixel->red * alpha) / 255;
                pixel->green = (pixel->green * alpha) / 255;
                pixel->blue = (pixel->blue * alpha) / 255;
                pixel->alpha = alpha;
            }
        }
        RwRasterUnlock(raster);
    }

    RwTexture *texture = RwTextureCreate(raster);
    if (!texture) RwRasterDestroy(raster);
    return texture;
}

RwTexture *TextureMgr::RwReadTexture(const char *name, char *Maskname)
{
    return ((RwTexture * (__cdecl *)(char const *, char const *))0x4C7510)(name, Maskname);
}

RwTexture *TextureMgr::Get(std::string_view name, RwUInt8 alpha)
{
    RwTexture *legacy = nullptr;
    if (alpha == 255) {
        for (size_t i = 0; i < 5; ++i) if (name == g_ShadowNames[i]) { legacy = g_LegacyShadows[i]; break; }
    }
    if (g_UseLegacyShadows && legacy) return legacy;
    auto it = Textures.find(name);
    if (it != Textures.end())
    {
        auto itAlpha = it->second.find(alpha);
        if (itAlpha != it->second.end() && itAlpha->second)
        {
            return itAlpha->second;
        }
    }

    static auto pDict = CFileLoader::LoadTexDictionary(MOD_DATA_PATH("ME_TEXDB.TXD"));
    char nameBuf[64];
    size_t copyLen = std::min(name.size(), sizeof(nameBuf) - 1);
    std::memcpy(nameBuf, name.data(), copyLen);
    nameBuf[copyLen] = '\0';

    RwTexture *pTex = pDict ? RwTexDictionaryFindNamedTexture(pDict, nameBuf) : nullptr;
    if (pTex == nullptr) {
        return legacy;
    }

    std::string keyStr(name);
    Textures[keyStr][alpha] = pTex;

    if (alpha != 255)
    {
        SetAlpha(Textures[keyStr][alpha], alpha);
    }
    return Textures[keyStr][alpha];
}

RwTexture *TextureMgr::FindOnTextureInDict(RpMaterial *pMat, RwTexDictionary *pDict, bool fallback)
{
    if ((pMat == nullptr) || (pMat->texture == nullptr) || (pMat->texture->name == nullptr)) {
        return nullptr;
    }

    const char *baseName = pMat->texture->name;
    size_t baseLen = std::strlen(baseName);
    if (baseLen == 0 || baseLen >= 58) {
        return nullptr;
    }

    char texBuf[64];
    // Try baseName + "on"
    std::memcpy(texBuf, baseName, baseLen);
    texBuf[baseLen] = 'o';
    texBuf[baseLen + 1] = 'n';
    texBuf[baseLen + 2] = '\0';

    RwTexture *pTex = TextureMgr::FindInDict(std::string_view(texBuf, baseLen + 2), pDict);
    if (pTex != nullptr) {
        return pTex;
    }

    // Try baseName + "_on"
    texBuf[baseLen] = '_';
    texBuf[baseLen + 1] = 'o';
    texBuf[baseLen + 2] = 'n';
    texBuf[baseLen + 3] = '\0';

    pTex = TextureMgr::FindInDict(std::string_view(texBuf, baseLen + 3), pDict);
    if (pTex || !fallback) return pTex;

    // Prefer either local spelling before searching shared dictionaries.
    texBuf[baseLen] = 'o';
    texBuf[baseLen + 1] = 'n';
    texBuf[baseLen + 2] = '\0';
    pTex = TextureMgr::FindInDict(std::string_view(texBuf, baseLen + 2), pDict, true, false);
    if (pTex) return pTex;

    texBuf[baseLen] = '_';
    texBuf[baseLen + 1] = 'o';
    texBuf[baseLen + 2] = 'n';
    texBuf[baseLen + 3] = '\0';
    return TextureMgr::FindInDict(std::string_view(texBuf, baseLen + 3), pDict, true, false);
}

void TextureMgr::SetAlpha(RwTexture *texture, RwUInt8 alpha)
{
    if (texture == nullptr) {
        return;
    }

    RwRaster *oldRaster = RwTextureGetRaster(texture);
    if (oldRaster == nullptr) {
        return;
    }

    int width = RwRasterGetWidth(oldRaster);
    int height = RwRasterGetHeight(oldRaster);

    RwImage *image = RwImageCreate(width, height, 32); // 32-bit = supports RGBA
    RwImageAllocatePixels(image);
    RwImageSetFromRaster(image, oldRaster);

    RwRGBA *pixels = (RwRGBA *)RwImageGetPixels(image);
    for (int y = 0; y < height; ++y)
    {
        for (int x = 0; x < width; ++x)
        {
            RwRGBA *pixel = &pixels[y * width + x];
            pixel->red = (pixel->red * alpha) / 255;
            pixel->green = (pixel->green * alpha) / 255;
            pixel->blue = (pixel->blue * alpha) / 255;
            pixel->alpha = alpha;
        }
    }

    RwRasterDestroy(oldRaster);
    RwRaster *newRaster = RwRasterCreate(width, height, 32, rwRASTERTYPETEXTURE | rwRASTERFORMAT8888);
    RwRasterSetFromImage(newRaster, image);
    texture->raster = newRaster;
    RwImageDestroy(image);
}

// Priority
// 1. Vehicle's txd 
// 2. ModelExtras txd
// 3. vehicle.txd (Supports vehfuncs additional txds)
RwTexture *TextureMgr::FindInDict(std::string_view name, RwTexDictionary *pDict, bool fallback, bool useDefault)
{
    if (name.empty()) return nullptr;

    char nameBuf[64];
    size_t copyLen = std::min(name.size(), sizeof(nameBuf) - 1);
    std::memcpy(nameBuf, name.data(), copyLen);
    nameBuf[copyLen] = '\0';

    RwTexture *pTex = nullptr;

    if (pDict)
    {
        pTex = RwTexDictionaryFindNamedTexture(pDict, nameBuf);
    }

    if (fallback) {
        if (!pTex) {
            LOG_VERBOSE("TextureMgr: Unable to find '{}' in the vehicle's TXD file. Searching in the ModelExtras TXD file instead.", name);
            pTex = TextureMgr::Get(name);
        }

        if (!pTex) {
            LOG_VERBOSE("TextureMgr: Unable to find '{}' in the ModelExtras TXD file. Searching in the vehicle TXD file instead.", name);
            if (CVehicleModelInfo::ms_pVehicleTxd)
                pTex = RwTexDictionaryFindNamedTexture(CVehicleModelInfo::ms_pVehicleTxd, nameBuf);
        }

        if (!pTex && useDefault) {
            LOG_VERBOSE("TextureMgr: Unable to find '{}' in the vehicle TXD file. Using the default white texture", name);
            pTex = CVehicleModelInfo::ms_pWhiteTexture;
        }
    }

    return pTex;
}
