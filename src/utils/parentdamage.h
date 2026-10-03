#pragma once

class CVehicle;

class ParentDamageVisibility {
public:
    static void Init();
    static void Apply(CVehicle *vehicle);
};
