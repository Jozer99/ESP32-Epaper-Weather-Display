#ifndef FORECAST_RECORD_H_
#define FORECAST_RECORD_H_

#include <Arduino.h>

typedef struct { // For current Day and Day 1, 2, 3, etc
  int    Dt;
  String Icon;
  String Trend;
  String Forecast0;
  String Description;
  float  Temperature;
  float  FeelsLike;
  float  Humidity;
  float  High;
  float  Low;
  float  Winddir;
  float  Windspeed;
  float  Rainfall;
  float  Snowfall;
  float  Pressure;
  int    Cloudcover;
  float  Pop;  // Probability of precipitation (0.0 to 1.0)
  int    Visibility;
  int    Sunrise;
  int    Sunset;
  int    FTimezone;  
} Forecast_record_type;

#endif /* ifndef FORECAST_RECORD_H_ */
