#ifndef __LANG_H__
#define __LANG_H__

#include <Arduino.h>

#define FONT(x) x##_tf

//Temperature - Humidity - Forecast
static const String TXT_TEMPERATURE_C    = "Temperature (°C)";
static const String TXT_TEMPERATURE_F    = "Temperature (°F)";
static const String TXT_HUMIDITY         = "Humidity";
static const String TXT_HILO             = "Hi/Lo";
static const String TXT_FEELSLIKE        = "Feels Like";

// Pressure
static const String TXT_PRESSURE     = "Pressure";

//RainFall / SnowFall
static const String TXT_RAINFALL_MM = "Rainfall (mm)";
static const String TXT_RAINFALL_IN = "Rainfall (in)";
static const String TXT_SNOWFALL_MM = "Snowfall (mm)";
static const String TXT_SNOWFALL_IN = "Snowfall (in)";

//Moon
static const String TXT_MOON_NEW             = "New";
static const String TXT_MOON_WAXING_CRESCENT = "Waxing Crescent";
static const String TXT_MOON_FIRST_QUARTER   = "First Quarter";
static const String TXT_MOON_WAXING_GIBBOUS  = "Waxing Gibbous";
static const String TXT_MOON_FULL            = "Full";
static const String TXT_MOON_WANING_GIBBOUS  = "Waning Gibbous";
static const String TXT_MOON_THIRD_QUARTER   = "Third Quarter";
static const String TXT_MOON_WANING_CRESCENT = "Waning Crescent";

//Wind
static const String TXT_WIND = "Wind";
static const String TXT_N   = "N";
static const String TXT_NNE = "NNE";
static const String TXT_NE  = "NE";
static const String TXT_ENE = "ENE";
static const String TXT_E   = "E";
static const String TXT_ESE = "ESE";
static const String TXT_SE  = "SE";
static const String TXT_SSE = "SSE";
static const String TXT_S   = "S";
static const String TXT_SSW = "SSW";
static const String TXT_SW  = "SW";
static const String TXT_WSW = "WSW";
static const String TXT_W   = "W";
static const String TXT_WNW = "WNW";
static const String TXT_NW  = "NW";
static const String TXT_NNW = "NNW";

//Sun
static const String TXT_SUNRISE = "Sunrise";
static const String TXT_SUNSET  = "Sunset";

// UV
static const String TXT_UV_LOW = "(L)";
static const String TXT_UV_MEDIUM = "(M)";
static const String TXT_UV_HIGH = "(H)";
static const String TXT_UV_VERYHIGH = "(VH)";
static const String TXT_UV_EXTREME = "(EX)";

//Day of the week
static const char* weekday_D[] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };

//Month
static const char* month_M[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };

#endif // __LANG_H__
