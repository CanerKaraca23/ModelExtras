#include "pch.h"
#include "utils/datamgr.h"
#include "utils/samp.h"
#include "carcols.h"
#include <rwcore.h>
#include <rpworld.h>
#include <RenderWare.h>
#include "core/colors.h"
#include <CModelInfo.h>

static std::array<unsigned char, 33> ReadModelColors(CVehicleModelInfo *info)
{
    std::array<unsigned char, 33> colors;
    const unsigned char *channels[] = {info->m_anPrimaryColors, info->m_anSecondaryColors,
        info->m_anTertiaryColors, info->m_anQuaternaryColors};
    for (size_t channel = 0; channel < 4; ++channel)
        std::copy_n(channels[channel], 8, colors.begin() + channel * 8);
    colors[32] = info->m_nNumColorVariations;
    return colors;
}

static void WriteModelColors(CVehicleModelInfo *info, const std::array<unsigned char, 33> &colors)
{
    unsigned char *channels[] = {info->m_anPrimaryColors, info->m_anSecondaryColors,
        info->m_anTertiaryColors, info->m_anQuaternaryColors};
    for (size_t channel = 0; channel < 4; ++channel)
        std::copy_n(colors.begin() + channel * 8, 8, channels[channel]);
    info->m_nNumColorVariations = colors[32];
    if (info->m_nLastColorVariation >= colors[32]) info->m_nLastColorVariation = 0;
}

void Carcols::RestoreModelVariations()
{
    for (const auto &[model, saved] : modelColorOverrides) {
        auto *info = CModelInfo::GetModelInfo(model);
        if (info == saved.info && info && info->GetModelType() == MODEL_INFO_VEHICLE &&
            ReadModelColors(saved.info) == saved.after) WriteModelColors(saved.info, saved.before);
    }
    modelColorOverrides.clear();
}

bool Carcols::ApplyModelVariations(int model, const nlohmann::json &carcols, size_t paletteSize)
{
    if (!carcols.contains("variations") || !carcols["variations"].is_array()) return false;
    const auto &rows = carcols["variations"];
    if (rows.empty() || rows.size() > 8 || model <= 0 || model >= 20000) return false;
    std::array<unsigned char, 33> colors{};
    const char *keys[] = {"primary", "secondary", "tertiary", "quaternary"};
    for (size_t row = 0; row < rows.size(); ++row) {
        if (!rows[row].is_object()) return false;
        for (size_t channel = 0; channel < 4; ++channel) {
            const auto value = rows[row].value(keys[channel], nlohmann::json(0));
            if (!value.is_number_integer() || value < 0 || value > 255 || value >= paletteSize) return false;
            colors[channel * 8 + row] = value.get<unsigned char>();
        }
    }
    if (!m_bEnabled || m_bMultiplayer) return true;
    auto *base = CModelInfo::GetModelInfo(model);
    if (!base || base->GetModelType() != MODEL_INFO_VEHICLE) return false;
    auto *info = static_cast<CVehicleModelInfo *>(base);
    auto existing = modelColorOverrides.find(model);
    if (existing != modelColorOverrides.end()) return ReadModelColors(info) == existing->second.after;
    const auto before = ReadModelColors(info);
    for (size_t channel = 0; channel < 4; ++channel)
        std::copy(before.begin() + channel * 8 + rows.size(), before.begin() + (channel + 1) * 8,
            colors.begin() + channel * 8 + rows.size());
    colors[32] = static_cast<unsigned char>(rows.size());
    modelColorOverrides.emplace(model, ModelColorOverride{info, before, colors});
    WriteModelColors(info, colors);
    return true;
}

#define IS_SAME_COLOR(type, VEHCOL) \
    ((type.r == VEHCOL.r) &&        \
     (type.g == VEHCOL.g) &&        \
     (type.b == VEHCOL.b))

void Carcols::ReloadConfig()
{
    CBaseFeature::ReloadConfig();
    m_bEnabled = m_bActive;
    m_bMultiplayer = SAMP::IsPresent();
    if (!m_bEnabled || m_bMultiplayer) RestoreModelVariations();
    else for (const auto &[model, palette] : indexedPalettes) {
        const auto *config = DataMgr::Find(model);
        if (config && config->contains("carcols")) ApplyModelVariations(model, (*config)["carcols"], palette.size());
    }
}

void Carcols::Init()
{
    ReloadConfig();
    Events::initGameEvent.before += [] { modelColorOverrides.clear(); };
    DataMgr::RegisterListener("carcols", [](int model, const nlohmann::json &data) {
        Carcols::Parse(data, model);
    });
}

bool Carcols::GetColor(CVehicle *pVeh, RpMaterial *pMat, CRGBA &col)
{
    if (!pMat || !CVehicleModelInfo::ms_currentCol) return false;
    CRGBA *colorTable = *reinterpret_cast<CRGBA **>(0x4C8390);
    if (!colorTable) return false;
    CRGBA type = *reinterpret_cast<CRGBA *>(RpMaterialGetColor(pMat));
    type.a = 255;

    const auto *config = pVeh ? DataMgr::Find(pVeh->m_nModelIndex) : nullptr;
    const bool hasCarcols = config && config->contains("carcols");
    if (pVeh && m_bEnabled && !m_bMultiplayer && hasCarcols &&
        !indexedPalettes.contains(pVeh->m_nModelIndex) &&
        variations.contains(pVeh->m_nModelIndex) && !variations[pVeh->m_nModelIndex].empty())
    {
        int model = pVeh->m_nModelIndex;
        auto &data = m_VehData.Get(pVeh);
        if (data.randId < 0 || static_cast<size_t>(data.randId) >= variations[model].size())
        {
            data.m_bPri = data.m_bSec = data.m_bTer = data.m_bQuat = false;
            data.randId = rand() % variations[model].size();
        }
        auto storeCol = variations[model][data.randId];

        if (type.r == VEHCOL_PRIMARY.r && type.g == VEHCOL_PRIMARY.g)
        { // blue can be anything
            if (!data.m_bPri)
            {
                data.m_Colors.primary = storeCol.primary;
                data.m_bPri = true;
            }
            col = data.m_Colors.primary;
        }
        else if (IS_SAME_COLOR(type, VEHCOL_SECONDARY))
        {
            if (!data.m_bSec)
            {
                data.m_Colors.secondary = storeCol.secondary;
                data.m_bSec = true;
            }
            col = data.m_Colors.secondary;
        }
        else if (IS_SAME_COLOR(type, VEHCOL_TERTIARY))
        {
            if (!data.m_bTer)
            {
                data.m_Colors.tert = storeCol.tert;
                data.m_bTer = true;
            }
            col = data.m_Colors.tert;
        }
        else if (IS_SAME_COLOR(type, VEHCOL_QUATARNARY))
        {
            if (!data.m_bQuat)
            {
                data.m_Colors.quart = storeCol.quart;
                data.m_bQuat = true;
            }
            col = data.m_Colors.quart;
        }
        else
        {
            return false;
        }
    }
    else
    {
        int idx = 0;
        if (type.r == VEHCOL_PRIMARY.r && type.g == VEHCOL_PRIMARY.g)
        { // blue can be anything
            idx = CVehicleModelInfo::ms_currentCol[0];
        }
        else if (IS_SAME_COLOR(type, VEHCOL_SECONDARY))
        {
            idx = CVehicleModelInfo::ms_currentCol[1];
        }
        else if (IS_SAME_COLOR(type, VEHCOL_TERTIARY))
        {
            idx = CVehicleModelInfo::ms_currentCol[2];
        }
        else if (IS_SAME_COLOR(type, VEHCOL_QUATARNARY))
        {
            idx = CVehicleModelInfo::ms_currentCol[3];
        }
        else
        {
            return false;
        }
        if (pVeh && m_bEnabled && hasCarcols) {
            auto palette = indexedPalettes.find(pVeh->m_nModelIndex);
            if (palette != indexedPalettes.end() && static_cast<size_t>(idx) < palette->second.size()) {
                col = palette->second[idx];
                return true;
            }
        }
        if (idx >= 128 && colorTable == CVehicleModelInfo::ms_vehicleColourTable) return false;
        col = colorTable[idx];
    }

    return true;
}

void Carcols::Parse(const nlohmann::json &data, int model)
{
    m_bMultiplayer = SAMP::IsPresent();
    variations.erase(model);
    indexedPalettes.erase(model);
    if (data.contains("carcols"))
    {
        const auto &carcols = data["carcols"];
        if (!carcols.is_object() || !carcols.contains("colors") || !carcols["colors"].is_array()) return;
        auto &cols = data["carcols"]["colors"];
        const bool converted = data.contains("metadata") && data["metadata"].is_object() &&
            data["metadata"].contains("desc") && data["metadata"]["desc"] == "Converted from IVF";
        const bool explicitMode = carcols.contains("use_game_indices");
        const bool indexed = explicitMode ?
            (carcols["use_game_indices"].is_boolean() && carcols["use_game_indices"].get<bool>()) : converted;
        if (indexed) {
            std::vector<CRGBA> palette;
            palette.reserve(cols.size());
            for (const auto &color : cols) {
                if (!color.is_object() || !color.contains("red") || !color["red"].is_number_integer() ||
                    !color.contains("green") || !color["green"].is_number_integer() ||
                    !color.contains("blue") || !color["blue"].is_number_integer()) return;
                palette.emplace_back(color["red"], color["green"], color["blue"], 255);
            }
            const bool ready = ApplyModelVariations(model, carcols, palette.size());
            if (explicitMode || m_bMultiplayer || ready) {
                indexedPalettes[model] = std::move(palette);
                return;
            }
        }
        if (m_bMultiplayer || !carcols.contains("variations") || !carcols["variations"].is_array()) return;
        auto &var = data["carcols"]["variations"];

        for (auto &e : var)
        {
            if (!e.is_object() ||
                (e.contains("primary") && !e["primary"].is_number_integer()) ||
                (e.contains("secondary") && !e["secondary"].is_number_integer()) ||
                (e.contains("tertiary") && !e["tertiary"].is_number_integer()) ||
                (e.contains("quaternary") && !e["quaternary"].is_number_integer())) continue;
            int pIdx = e.value("primary", 0);
            int sIdx = e.value("secondary", 0);
            int tIdx = e.value("tertiary", 0);
            int qIdx = e.value("quaternary", 0);

            auto maxIdx = cols.size();
            if (pIdx < 0 || sIdx < 0 || tIdx < 0 || qIdx < 0 ||
                static_cast<size_t>(pIdx) >= maxIdx || static_cast<size_t>(sIdx) >= maxIdx ||
                static_cast<size_t>(tIdx) >= maxIdx || static_cast<size_t>(qIdx) >= maxIdx)
            {
                LOG(ERROR) << std::format(
                    "Carcols index out of bounds for model '{}': "
                    "primary={}, secondary={}, tertiary={}, quaternary={}, max={}",
                    model, pIdx, sIdx, tIdx, qIdx, maxIdx);
                continue;
            }

            auto &pCol = cols.at(pIdx);
            auto &sCol = cols.at(sIdx);
            auto &tCol = cols.at(tIdx);
            auto &qCol = cols.at(qIdx);

            CRGBA primaryColor = CRGBA(pCol["red"], pCol["green"], pCol["blue"], 255);
            CRGBA secondaryColor = CRGBA(sCol["red"], sCol["green"], sCol["blue"], 255);
            CRGBA tertiaryColor = CRGBA(tCol["red"], tCol["green"], tCol["blue"], 255);
            CRGBA quaternaryColor = CRGBA(qCol["red"], qCol["green"], qCol["blue"], 255);

            variations[model]
                .push_back({primaryColor, secondaryColor, tertiaryColor, quaternaryColor});
        }
    }
}
