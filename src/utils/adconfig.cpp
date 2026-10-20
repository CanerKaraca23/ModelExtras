#include "pch.h"
#include "adconfig.h"
#include "defines.h"
#include <fstream>
#include <sstream>
#include <filesystem>
#include <vector>

namespace fs = std::filesystem;

void ADConfig::Init()
{
    if (m_bInitialized) return;
    m_bInitialized = true;
    Reload();
}

void ADConfig::Reload()
{
    m_Settings.clear();
    LoadControlKeys();
    LoadVehsets();
}

const ADVehicleSettings* ADConfig::Get(int modelId)
{
    auto it = m_Settings.find(modelId);
    if (it != m_Settings.end()) {
        return &it->second;
    }
    return nullptr;
}

bool ADConfig::Has(int modelId)
{
    return m_Settings.find(modelId) != m_Settings.end();
}

void ADConfig::LoadControlKeys()
{
    // First, set defaults from INI if present
    m_Keys.keyTrailer = gConfig.ReadInteger("KEYS", "TrailerHitchKey", 'P');
    m_Keys.keyDriveFWD = gConfig.ReadInteger("KEYS", "DriveFWDKey", '0');
    m_Keys.keyDriveAWD = gConfig.ReadInteger("KEYS", "DriveAWDKey", 189); // '-'
    m_Keys.keyDriveRWD = gConfig.ReadInteger("KEYS", "DriveRWDKey", 187); // '='
    m_Keys.keyLowRange = gConfig.ReadInteger("KEYS", "DriveLowRangeKey", '9');
    m_Keys.keyWindowLeftUp = gConfig.ReadInteger("KEYS", "WindowLeftUpKey", VK_INSERT);
    m_Keys.keyWindowLeftDown = gConfig.ReadInteger("KEYS", "WindowLeftDownKey", VK_DELETE);
    m_Keys.keyWindowRightUp = gConfig.ReadInteger("KEYS", "WindowRightUpKey", VK_PRIOR);
    m_Keys.keyWindowRightDown = gConfig.ReadInteger("KEYS", "WindowRightDownKey", VK_NEXT);
    m_Keys.keyLiftAxle = gConfig.ReadInteger("KEYS", "LiftAxleToggleKey", VK_OEM_4); // '['
    m_Keys.keyTipperUp = gConfig.ReadInteger("KEYS", "TipperUpKey", VK_NUMPAD8);
    m_Keys.keyTipperDown = gConfig.ReadInteger("KEYS", "TipperDownKey", VK_NUMPAD2);
    m_Keys.keyCabTiltUp = gConfig.ReadInteger("KEYS", "CabTiltUpKey", VK_NUMPAD8);
    m_Keys.keyCabTiltDown = gConfig.ReadInteger("KEYS", "CabTiltDownKey", VK_NUMPAD2);
    m_Keys.keyCargoDoorsToggle = gConfig.ReadInteger("KEYS", "CargoDoorsToggleKey", VK_NUMPAD5);

    // Look for legacy control_keys.cfg in data/
    std::vector<fs::path> searchPaths = {
        fs::path("data/control_keys.cfg"),
        fs::path("DATA/CONTROL_KEYS.CFG"),
        fs::path("ModelExtras/control_keys.cfg")
    };

    for (const auto& path : searchPaths) {
        if (!fs::exists(path)) continue;
        std::ifstream file(path);
        if (!file.is_open()) continue;

        std::string line;
        std::vector<int> keyCodes;
        while (std::getline(file, line)) {
            // Trim whitespace
            size_t first = line.find_first_not_of(" \t\r\n");
            if (first == std::string::npos || line[first] == '#') continue;
            try {
                int code = std::stoi(line.substr(first));
                keyCodes.push_back(code);
            } catch (...) {}
        }

        // Active Dashboard control_keys.cfg layout:
        // [0] engine (ignored)
        // [1] hazard
        // [2] left turn
        // [3] right turn
        // [4] trailer hitch
        // [5] handbrake (ignored)
        // [6] RWD
        // [7] AWD
        // [8] FWD
        // [9] low range
        // [10] wipers
        if (keyCodes.size() >= 11) {
            if (keyCodes[4] > 0) m_Keys.keyTrailer = keyCodes[4];
            if (keyCodes[6] > 0) m_Keys.keyDriveRWD = keyCodes[6];
            if (keyCodes[7] > 0) m_Keys.keyDriveAWD = keyCodes[7];
            if (keyCodes[8] > 0) m_Keys.keyDriveFWD = keyCodes[8];
            if (keyCodes[9] > 0) m_Keys.keyLowRange = keyCodes[9];
        }
        break;
    }
}

void ADConfig::LoadVehsets()
{
    std::vector<fs::path> searchPaths = {
        fs::path("data/vehsets.dat"),
        fs::path("DATA/VEHSETS.DAT"),
        fs::path("ModelExtras/vehsets.dat")
    };

    for (const auto& path : searchPaths) {
        if (!fs::exists(path)) continue;
        std::ifstream file(path);
        if (!file.is_open()) continue;

        std::string line;
        while (std::getline(file, line)) {
            size_t first = line.find_first_not_of(" \t\r\n");
            if (first == std::string::npos || line[first] == '#') continue;

            std::stringstream ss(line);
            int modelId = 0;
            if (!(ss >> modelId) || modelId <= 0) continue;

            ADVehicleSettings s;
            ss >> s.steerMul
               >> s.speedoMul
               >> s.idleRpmMul
               >> s.driveRpmMul
               >> s.petrolMul
               >> s.wiperMul
               >> s.tipperAngle
               >> s.oilMul
               >> s.tempMul
               >> s.voltAmpMul
               >> s.craneBoomMul
               >> s.frontSteerRatio
               >> s.rearSteerRatio
               >> s.cardan1Angle
               >> s.cardan2Angle;

            m_Settings[modelId] = s;
        }
        LOG(INFO) << "ADConfig: Loaded " << m_Settings.size() << " vehicle configurations from " << path.string();
        break;
    }
}
