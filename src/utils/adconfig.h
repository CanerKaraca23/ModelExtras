#pragma once
#include <unordered_map>
#include <string>
#include <cstdint>

struct ADVehicleSettings {
    float steerMul = 1.0f;         // col 2: руль
    float speedoMul = 1.0f;        // col 3: спидометр
    float idleRpmMul = 1.0f;       // col 4: хол.обороты
    float driveRpmMul = 1.0f;      // col 5: движ.обороты
    float petrolMul = 1.0f;        // col 6: бензин (unused)
    float wiperMul = 1.0f;         // col 7: дворники
    float tipperAngle = 45.0f;     // col 8: самосвал (max bed lift angle in degrees)
    float oilMul = 1.0f;           // col 9: масло
    float tempMul = 1.0f;          // col 10: температур
    float voltAmpMul = 1.0f;       // col 11: ампер/вольт
    float craneBoomMul = 1.0f;     // col 12: стрела
    float frontSteerRatio = 0.0f;  // col 13: перед.мост (ratio to front steer)
    float rearSteerRatio = 0.0f;   // col 14: задн.мост (ratio to rear steer)
    float cardan1Angle = 0.0f;     // col 15: кард1.угол
    float cardan2Angle = 0.0f;     // col 16: кард2.угол
};

struct ADKeyBindings {
    int keyTrailer = 'P';            // Hitch / unhitch trailer
    int keyDriveFWD = '0';           // FWD mode
    int keyDriveAWD = 189;           // AWD mode ('-')
    int keyDriveRWD = 187;           // RWD mode ('=')
    int keyLowRange = '9';           // Low range gear
    int keyWindowLeftUp = VK_INSERT;
    int keyWindowLeftDown = VK_DELETE;
    int keyWindowRightUp = VK_PRIOR; // Page Up
    int keyWindowRightDown = VK_NEXT;// Page Down
    int keyLiftAxle = VK_OEM_4;      // '[' by default, or Numpad 9/3
    int keyTipperUp = VK_NUMPAD8;
    int keyTipperDown = VK_NUMPAD2;
    int keyCraneRotateLeft = VK_NUMPAD4;
    int keyCraneRotateRight = VK_NUMPAD6;
    int keyCraneBoomUp = VK_NUMPAD8;
    int keyCraneBoomDown = VK_NUMPAD2;
    int keyCraneBoomExtend = VK_NUMPAD7;
    int keyCraneBoomRetract = VK_NUMPAD9;
    int keyCraneOutriggersDeploy = VK_NUMPAD1;
    int keyCraneOutriggersRetract = VK_NUMPAD3;
    int keyCraneHookUp = VK_ADD;
    int keyCraneHookDown = VK_SUBTRACT;
    int keyCabTiltUp = VK_NUMPAD8;
    int keyCabTiltDown = VK_NUMPAD2;
    int keyCargoDoorsToggle = VK_NUMPAD5;
};

class ADConfig {
public:
    static void Init();
    static void Reload();
    static const ADVehicleSettings* Get(int modelId);
    static bool Has(int modelId);
    static const ADKeyBindings& Keys() { return m_Keys; }

private:
    static void LoadVehsets();
    static void LoadControlKeys();

    static inline std::unordered_map<int, ADVehicleSettings> m_Settings;
    static inline ADKeyBindings m_Keys;
    static inline bool m_bInitialized = false;
};
