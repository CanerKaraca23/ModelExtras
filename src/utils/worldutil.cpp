#include "pch.h"
#include "worldutil.h"
#include <CClock.h>
#include <CWeather.h>
#include <game_vc/enums/eWeather.h>

bool WorldUtil::IsNightTime()
{
    return CClock::GetIsTimeInRange(20, 6);
}

bool WorldUtil::IsFoggy()
{
    return CWeather::NewWeatherType == WEATHER_FOGGY || CWeather::OldWeatherType == WEATHER_FOGGY;
}

bool WorldUtil::IsRainy()
{
    return CWeather::NewWeatherType == WEATHER_RAINY || CWeather::OldWeatherType == WEATHER_RAINY ||
           CWeather::NewWeatherType == WEATHER_HURRICANE || CWeather::OldWeatherType == WEATHER_HURRICANE;
}
