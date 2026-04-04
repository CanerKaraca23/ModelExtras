#pragma once
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>
#include <deque>

typedef unsigned char BYTE;
#ifndef NULL
#define NULL 0
#endif

// Mocking CIniReader
class CIniReader {
public:
    bool boolValue = false;
    bool ReadBoolean(const char* section, const char* key, bool defaultValue) {
        if (std::string(section) == "FEATURES" && std::string(key) == "SoundEffects") return boolValue;
        return defaultValue;
    }
    // Matching the exact signature in audiomgr.cpp
    bool ReadBoolean(std::string_view section, std::string_view key, bool defaultValue) {
        if (section == "FEATURES" && key == "SoundEffects") return boolValue;
        return defaultValue;
    }
    float ReadFloat(std::string_view section, std::string_view key, float defaultValue) { return defaultValue; }
};

extern CIniReader gConfig;

// Mocking plugin-sdk stuff
namespace plugin {
    template<unsigned int ID, typename... Args>
    void Command(Args...) {}

    class Events {
    public:
        struct Event {
            template<typename T>
            void operator+=(T) {}
        };
        static inline Event reInitGameEvent;
        static inline Event processScriptsEvent;
    };
}

#define LOG_VERBOSE(...)
#define MOD_DATA_PATH(x) (x)

// GTA classes mocks
struct CVector { float x, y, z; };
class CEntity {
public:
    unsigned char m_nType;
    CVector GetPosition() { return {0,0,0}; }
};
class CVehicle : public CEntity {};
class CPed : public CEntity {};
class CObject : public CEntity {};

enum eEntityType {
    ENTITY_TYPE_PED = 0, // Simplified
    ENTITY_TYPE_VEHICLE = 1,
    ENTITY_TYPE_OBJECT = 2,
};

class CPools {
public:
    static int GetVehicleRef(CVehicle*) { return 0; }
    static int GetPedRef(CPed*) { return 0; }
    static int GetObjectRef(CObject*) { return 0; }
};

class CTimer {
public:
    static inline unsigned int m_snTimeInMilliseconds = 0;
};

class CAudioEngine {
public:
    void ReportFrontendAudioEvent(int, float, float) {}
};
extern CAudioEngine AudioEngine;

#define AE_FRONTEND_RADIO_CLICK_ON 1

inline CPed* FindPlayerPed() { return nullptr; }

// Other things needed for audiomgr.cpp compilation
using StreamHandle = int;
#include "utils/audiomgr.h"
