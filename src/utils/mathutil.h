#pragma once

#include <cmath>
#include <CVector.h>

class MathUtil
{
private:
    static constexpr double PI_VAL = 3.14159265358979323846;
    static constexpr double RAD_TO_DEG = 180.0 / PI_VAL;
    static constexpr double DEG_TO_RAD = PI_VAL / 180.0;

public:
    static inline float DistanceSquared(const CVector &a, const CVector &b)
    {
        const float dx = a.x - b.x;
        const float dy = a.y - b.y;
        const float dz = a.z - b.z;
        return dx * dx + dy * dy + dz * dz;
    }

    static inline float Distance(const CVector &a, const CVector &b)
    {
        return std::sqrt(DistanceSquared(a, b));
    }

    static inline float Dot(const CVector &a, const CVector &b)
    {
        return a.x * b.x + a.y * b.y + a.z * b.z;
    }

    static inline float GetSquaredMagnitude(const CVector &v)
    {
        return v.x * v.x + v.y * v.y + v.z * v.z;
    }

    static inline float NormalizeAngle(float angle)
    {
        if (angle >= 0.0f && angle < 360.0f)
        {
            return angle;
        }
        angle = std::fmod(angle, 360.0f);
        if (angle < 0.0f)
        {
            angle += 360.0f;
        }
        return angle;
    }

    static inline double RadToDeg(double rad)
    {
        return rad * RAD_TO_DEG;
    }

    static inline double DegToRad(double deg)
    {
        return deg * DEG_TO_RAD;
    }

    static inline float GetRandomNumberInRange(float min, float max)
    {
        return min + static_cast<float>(rand()) / (static_cast<float>(RAND_MAX / (max - min)));
    }
};
