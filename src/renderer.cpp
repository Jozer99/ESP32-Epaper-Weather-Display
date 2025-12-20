/**
 * Renderer implementation for LilyGo T5 4.7 E-Paper Display
 * 
 * Provides drawing functions for weather information display including:
 * - Current conditions with large weather icon
 * - 5-day forecast with daily statistics
 * - 24-hour temperature and precipitation graph
 * - Status bar with WiFi and battery indicators
 * - Low battery warning screen
 */

#include "renderer.h"
#include "lang.h"
#include "settings.h"
#include "OpenSans8B.h"
#include "OpenSans10B.h"
#include "OpenSans12B.h"
#include "OpenSans18B.h"
#include "OpenSans24B.h"
#include "sunrise.h"
#include "sunset.h"
#include <math.h>

// Color definitions
#define White         0xFF
#define LightGrey     0xBB
#define Grey          0x88
#define DarkGrey      0x44
#define Black         0x00

// Icon size constants for weather condition icons
#define Large  25    // Large icon size (for current conditions)
#define Small  15    // Small icon size (for forecast)
#define LargeIcon   true
#define SmallIcon   false

// External variables from main.ino
extern uint8_t *framebuffer;
extern GFXfont currentFont;
extern int globalTimezoneOffset;  // Timezone offset in seconds (positive = east of UTC)

// Helper drawing functions - wrappers around EPD driver functions
void fillCircle(int x, int y, int r, uint8_t color) {
  epd_fill_circle(x, y, r, color, framebuffer);
}

void drawFastHLine(int16_t x0, int16_t y0, int length, uint16_t color) {
  epd_draw_hline(x0, y0, length, color, framebuffer);
}

void drawFastVLine(int16_t x0, int16_t y0, int length, uint16_t color) {
  epd_draw_vline(x0, y0, length, color, framebuffer);
}

void drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color) {
  epd_write_line(x0, y0, x1, y1, color, framebuffer);
}

void drawCircle(int x0, int y0, int r, uint8_t color) {
  epd_draw_circle(x0, y0, r, color, framebuffer);
}

void drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
  epd_draw_rect(x, y, w, h, color, framebuffer);
}

void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
  epd_fill_rect(x, y, w, h, color, framebuffer);
}

void fillTriangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                  int16_t x2, int16_t y2, uint16_t color) {
  epd_fill_triangle(x0, y0, x1, y1, x2, y2, color, framebuffer);
}

void drawPixel(int x, int y, uint8_t color) {
  epd_draw_pixel(x, y, color, framebuffer);
}

void DrawAngledLine(int x, int y, int x1, int y1, int size, int color) {
  float dist = sqrt((float)((x - x1) * (x - x1) + (y - y1) * (y - y1)));
  if (dist == 0) return;
  int dx = (size / 2.0) * (x - x1) / dist;
  int dy = (size / 2.0) * (y - y1) / dist;
  fillTriangle(x + dx, y - dy, x - dx,  y + dy,  x1 + dx, y1 - dy, color);
  fillTriangle(x - dx, y + dy, x1 - dx, y1 + dy, x1 + dx, y1 - dy, color);
}

// Icon building blocks (from source repo)
void addcloud(int x, int y, int scale, int linesize) {
  fillCircle(x - scale * 3, y, scale, Black);
  fillCircle(x + scale * 3, y, scale, Black);
  fillCircle(x - scale, y - scale, scale * 1.4, Black);
  fillCircle(x + scale * 1.5, y - scale * 1.3, scale * 1.75, Black);
  fillRect(x - scale * 3 - 1, y - scale, scale * 6, scale * 2 + 1, Black);
  fillCircle(x - scale * 3, y, scale - linesize, White);
  fillCircle(x + scale * 3, y, scale - linesize, White);
  fillCircle(x - scale, y - scale, scale * 1.4 - linesize, White);
  fillCircle(x + scale * 1.5, y - scale * 1.3, scale * 1.75 - linesize, White);
  fillRect(x - scale * 3 + 2, y - scale + linesize - 1, scale * 5.9, scale * 2 - linesize * 2 + 2, White);
}

void addrain(int x, int y, int scale, bool IconSize) {
  if (IconSize == SmallIcon) {
    setFont(OpenSans8B);
    drawString(x - 25, y + 12, "///////", LEFT, Black);
  } else {
    setFont(OpenSans18B);
    drawString(x - 60, y + 25, "///////", LEFT, Black);
  }
}

void addsnow(int x, int y, int scale, bool IconSize) {
  if (IconSize == SmallIcon) {
    setFont(OpenSans8B);
    drawString(x - 25, y + 15, "* * * *", LEFT, Black);
  } else {
    setFont(OpenSans18B);
    drawString(x - 60, y + 30, "* * * *", LEFT, Black);
  }
}

void addtstorm(int x, int y, int scale) {
  y = y + scale / 2;
  for (int i = 1; i < 5; i++) {
    drawLine(x - scale * 4 + scale * i * 1.5 + 0, y + scale * 1.5, x - scale * 3.5 + scale * i * 1.5 + 0, y + scale, Black);
    drawLine(x - scale * 4 + scale * i * 1.5 + 1, y + scale * 1.5, x - scale * 3.5 + scale * i * 1.5 + 1, y + scale, Black);
    drawLine(x - scale * 4 + scale * i * 1.5 + 2, y + scale * 1.5, x - scale * 3.5 + scale * i * 1.5 + 2, y + scale, Black);
    drawLine(x - scale * 4 + scale * i * 1.5, y + scale * 1.5 + 0, x - scale * 3 + scale * i * 1.5 + 0, y + scale * 1.5 + 0, Black);
    drawLine(x - scale * 4 + scale * i * 1.5, y + scale * 1.5 + 1, x - scale * 3 + scale * i * 1.5 + 0, y + scale * 1.5 + 1, Black);
    drawLine(x - scale * 4 + scale * i * 1.5, y + scale * 1.5 + 2, x - scale * 3 + scale * i * 1.5 + 0, y + scale * 1.5 + 2, Black);
    drawLine(x - scale * 3.5 + scale * i * 1.4 + 0, y + scale * 2.5, x - scale * 3 + scale * i * 1.5 + 0, y + scale * 1.5, Black);
    drawLine(x - scale * 3.5 + scale * i * 1.4 + 1, y + scale * 2.5, x - scale * 3 + scale * i * 1.5 + 1, y + scale * 1.5, Black);
    drawLine(x - scale * 3.5 + scale * i * 1.4 + 2, y + scale * 2.5, x - scale * 3 + scale * i * 1.5 + 2, y + scale * 1.5, Black);
  }
}

void addsun(int x, int y, int scale, bool IconSize) {
  int linesize = 5;
  fillRect(x - scale * 2, y, scale * 4, linesize, Black);
  fillRect(x, y - scale * 2, linesize, scale * 4, Black);
  DrawAngledLine(x + scale * 1.4, y + scale * 1.4, (x - scale * 1.4), (y - scale * 1.4), linesize * 1.5, Black);
  DrawAngledLine(x - scale * 1.4, y + scale * 1.4, (x + scale * 1.4), (y - scale * 1.4), linesize * 1.5, Black);
  fillCircle(x, y, scale * 1.3, White);
  fillCircle(x, y, scale, Black);
  fillCircle(x, y, scale - linesize, White);
}

void addfog(int x, int y, int scale, int linesize, bool IconSize) {
  if (IconSize == SmallIcon) linesize = 3;
  for (int i = 0; i < 6; i++) {
    fillRect(x - scale * 3, y + scale * 1.5, scale * 6, linesize, Black);
    fillRect(x - scale * 3, y + scale * 2.0, scale * 6, linesize, Black);
    fillRect(x - scale * 3, y + scale * 2.5, scale * 6, linesize, Black);
  }
}

void addmoon(int x, int y, bool IconSize) {
  int xOffset = 65;
  int yOffset = 12;
  if (IconSize == LargeIcon) {
    xOffset = 130;
    yOffset = -40;
  }
  fillCircle(x - 28 + xOffset, y - 37 + yOffset, uint16_t(Small * 1.0), Black);
  fillCircle(x - 16 + xOffset, y - 37 + yOffset, uint16_t(Small * 1.6), White);
}

// Main icon functions (from source repo)
void ClearSky(int x, int y, bool IconSize, String IconName) {
  int scale = Small;
  if (IconName.endsWith("n")) addmoon(x, y, IconSize);
  if (IconSize == LargeIcon) scale = Large;
  y += (IconSize ? 0 : 10);
  addsun(x, y, scale * (IconSize ? 1.7 : 1.2), IconSize);
}

void BrokenClouds(int x, int y, bool IconSize, String IconName) {
  int scale = Small, linesize = 5;
  if (IconName.endsWith("n")) addmoon(x, y, IconSize);
  y += 15;
  if (IconSize == LargeIcon) scale = Large;
  addsun(x - scale * 1.8, y - scale * 1.8, scale, IconSize);
  addcloud(x, y, scale * (IconSize ? 1 : 0.75), linesize);
}

void FewClouds(int x, int y, bool IconSize, String IconName) {
  int scale = Small, linesize = 5;
  if (IconName.endsWith("n")) addmoon(x, y, IconSize);
  y += 15;
  if (IconSize == LargeIcon) scale = Large;
  addcloud(x + (IconSize ? 10 : 0), y, scale * (IconSize ? 0.9 : 0.8), linesize);
  addsun((x + (IconSize ? 10 : 0)) - scale * 1.8, y - scale * 1.6, scale, IconSize);
}

void ScatteredClouds(int x, int y, bool IconSize, String IconName) {
  int scale = Small, linesize = 5;
  if (IconName.endsWith("n")) addmoon(x, y, IconSize);
  y += 15;
  if (IconSize == LargeIcon) scale = Large;
  addcloud(x - (IconSize ? 35 : 0), y * (IconSize ? 0.75 : 0.93), scale / 2, linesize);
  addcloud(x, y, scale * 0.9, linesize);
}

void Rain(int x, int y, bool IconSize, String IconName) {
  int scale = Small, linesize = 5;
  if (IconName.endsWith("n")) addmoon(x, y, IconSize);
  y += 15;
  if (IconSize == LargeIcon) scale = Large;
  addcloud(x, y, scale * (IconSize ? 1 : 0.75), linesize);
  addrain(x, y, scale, IconSize);
}

void ChanceRain(int x, int y, bool IconSize, String IconName) {
  int scale = Small, linesize = 5;
  if (IconName.endsWith("n")) addmoon(x, y, IconSize);
  if (IconSize == LargeIcon) scale = Large;
  y += 15;
  addsun(x - scale * 1.8, y - scale * 1.8, scale, IconSize);
  addcloud(x, y, scale * (IconSize ? 1 : 0.65), linesize);
  addrain(x, y, scale, IconSize);
}

void Thunderstorms(int x, int y, bool IconSize, String IconName) {
  int scale = Small, linesize = 5;
  if (IconName.endsWith("n")) addmoon(x, y, IconSize);
  if (IconSize == LargeIcon) scale = Large;
  y += 5;
  addcloud(x, y, scale * (IconSize ? 1 : 0.75), linesize);
  addtstorm(x, y, scale);
}

void Snow(int x, int y, bool IconSize, String IconName) {
  int scale = Small, linesize = 5;
  if (IconName.endsWith("n")) addmoon(x, y, IconSize);
  if (IconSize == LargeIcon) scale = Large;
  addcloud(x, y, scale * (IconSize ? 1 : 0.75), linesize);
  addsnow(x, y, scale, IconSize);
}

void Mist(int x, int y, bool IconSize, String IconName) {
  int scale = Small, linesize = 5;
  if (IconName.endsWith("n")) addmoon(x, y, IconSize);
  if (IconSize == LargeIcon) scale = Large;
  addsun(x, y, scale * (IconSize ? 1 : 0.75), linesize);
  addfog(x, y, scale, linesize, IconSize);
}

void Nodata(int x, int y, bool IconSize, String IconName) {
  if (IconSize == LargeIcon) setFont(OpenSans24B); else setFont(OpenSans12B);
  drawString(x - 3, y - 10, "?", CENTER, Black);
}

// Icon selection and display function (from source repo)
void DisplayConditionsSection(int x, int y, String IconName, bool IconSize) {
  Serial.println("Icon name: " + IconName);
  if      (IconName == "01d" || IconName == "01n") ClearSky(x, y, IconSize, IconName);
  else if (IconName == "02d" || IconName == "02n") FewClouds(x, y, IconSize, IconName);
  else if (IconName == "03d" || IconName == "03n") ScatteredClouds(x, y, IconSize, IconName);
  else if (IconName == "04d" || IconName == "04n") BrokenClouds(x, y, IconSize, IconName);
  else if (IconName == "09d" || IconName == "09n") ChanceRain(x, y, IconSize, IconName);
  else if (IconName == "10d" || IconName == "10n") Rain(x, y, IconSize, IconName);
  else if (IconName == "11d" || IconName == "11n") Thunderstorms(x, y, IconSize, IconName);
  else if (IconName == "13d" || IconName == "13n") Snow(x, y, IconSize, IconName);
  else if (IconName == "50d" || IconName == "50n") Mist(x, y, IconSize, IconName);
  else                                             Nodata(x, y, IconSize, IconName);
}

// Helper function to convert Unix time to formatted string (time only, for sunrise/sunset)
// Uses the timezone offset from the API to convert UTC to local time
static String ConvertUnixTime(int unix_time, int timezone_offset) {
  // Convert UTC timestamp to struct tm in UTC
  time_t utc_time = (time_t)unix_time;
  struct tm *utc_tm = gmtime(&utc_time);
  
  // Calculate local time by adding timezone offset (in seconds)
  // timezone_offset: positive = east of UTC (ahead), negative = west of UTC (behind)
  int local_seconds = utc_tm->tm_sec;
  int local_minutes = utc_tm->tm_min;
  int local_hours = utc_tm->tm_hour;
  
  // Add timezone offset (convert to hours, minutes, seconds)
  int offset_seconds = timezone_offset % 60;
  int offset_minutes = (timezone_offset / 60) % 60;
  int offset_hours = timezone_offset / 3600;
  
  local_seconds += offset_seconds;
  if (local_seconds >= 60) {
    local_seconds -= 60;
    local_minutes += 1;
  } else if (local_seconds < 0) {
    local_seconds += 60;
    local_minutes -= 1;
  }
  
  local_minutes += offset_minutes;
  if (local_minutes >= 60) {
    local_minutes -= 60;
    local_hours += 1;
  } else if (local_minutes < 0) {
    local_minutes += 60;
    local_hours -= 1;
  }
  
  local_hours += offset_hours;
  // Handle day rollover (hours can be 0-23, but offset might push outside)
  while (local_hours >= 24) {
    local_hours -= 24;
  }
  while (local_hours < 0) {
    local_hours += 24;
  }
  
  char output[40];
  if (String(settings.Units) == "M") {
    // Format as "6:44" (no leading zero on hour)
    snprintf(output, sizeof(output), "%d:%02d", local_hours, local_minutes);
  } else {
    // Format as "6:44AM" (no leading zero on hour)
    int hour12 = local_hours % 12;
    if (hour12 == 0) hour12 = 12;
    char ampm = (local_hours < 12) ? 'A' : 'P';
    snprintf(output, sizeof(output), "%d:%02d%cM", hour12, local_minutes, ampm);
  }
  return output;
}

// Helper function to convert wind direction to cardinal
String WindDegToOrdinalDirection(float winddirection) {
  if (winddirection >= 348.75 || winddirection < 11.25)  return TXT_N;
  if (winddirection >=  11.25 && winddirection < 33.75)  return TXT_NNE;
  if (winddirection >=  33.75 && winddirection < 56.25)  return TXT_NE;
  if (winddirection >=  56.25 && winddirection < 78.75)  return TXT_ENE;
  if (winddirection >=  78.75 && winddirection < 101.25) return TXT_E;
  if (winddirection >= 101.25 && winddirection < 123.75) return TXT_ESE;
  if (winddirection >= 123.75 && winddirection < 146.25) return TXT_SE;
  if (winddirection >= 146.25 && winddirection < 168.75) return TXT_SSE;
  if (winddirection >= 168.75 && winddirection < 191.25) return TXT_S;
  if (winddirection >= 191.25 && winddirection < 213.75) return TXT_SSW;
  if (winddirection >= 213.75 && winddirection < 236.25) return TXT_SW;
  if (winddirection >= 236.25 && winddirection < 258.75) return TXT_WSW;
  if (winddirection >= 258.75 && winddirection < 281.25) return TXT_W;
  if (winddirection >= 281.25 && winddirection < 303.75) return TXT_WNW;
  if (winddirection >= 303.75 && winddirection < 326.25) return TXT_NW;
  if (winddirection >= 326.25 && winddirection < 348.75) return TXT_NNW;
  return "?";
}

// Initialize display (already done in main.ino, this is placeholder)
void initDisplay() {
  // Display initialization is done in main.ino's InitialiseSystem()
}

// Power off display (already done in main.ino, this is placeholder)
void powerOffDisplay() {
  // Power off is done in main.ino
}

// Set font
void setFont(GFXfont const &font) {
  currentFont = font;
}

// Get string width
uint16_t getStringWidth(const String &text) {
  char *data = const_cast<char*>(text.c_str());
  int32_t x1, y1, w, h;
  int32_t x = 0, y = 0;
  get_text_bounds(&currentFont, data, &x, &y, &x1, &y1, &w, &h, NULL);
  return w;
}

// Get string height
uint16_t getStringHeight(const String &text) {
  char *data = const_cast<char*>(text.c_str());
  int32_t x1, y1, w, h;
  int32_t x = 0, y = 0;
  get_text_bounds(&currentFont, data, &x, &y, &x1, &y1, &w, &h, NULL);
  return h;
}

// Draw string with alignment
void drawString(int16_t x, int16_t y, const String &text, alignment_t alignment, uint8_t color) {
  char *data = const_cast<char*>(text.c_str());
  int32_t x1, y1, w, h;
  int32_t xx = x, yy = y;
  get_text_bounds(&currentFont, data, &xx, &yy, &x1, &y1, &w, &h, NULL);
  if (alignment == RIGHT)  x = x - w;
  if (alignment == CENTER) x = x - w / 2;
  int32_t cursor_y = y + h;
  int32_t cursor_x = x;
  write_string(&currentFont, data, &cursor_x, &cursor_y, framebuffer);
}

// Draw multi-line string (simplified version)
void drawMultiLnString(int16_t x, int16_t y, const String &text, alignment_t alignment, 
                       uint16_t max_width, uint16_t max_lines, int16_t line_spacing, uint8_t color) {
  uint16_t current_line = 0;
  String textRemaining = text;
  
  while (current_line < max_lines && !textRemaining.isEmpty()) {
    int spaceRemaining = 0;
    int p = 0;
    int charCount = 0;
    
    while (p < textRemaining.length() && charCount < max_width) {
      if (textRemaining.substring(p, p + 1) == " ") spaceRemaining = p;
      p++;
      charCount++;
    }
    
    String line = textRemaining;
    if (p < textRemaining.length()) {
      if (spaceRemaining > 0) {
        line = textRemaining.substring(0, spaceRemaining);
        textRemaining = textRemaining.substring(spaceRemaining + 1);
      } else {
        line = textRemaining.substring(0, p);
        textRemaining = textRemaining.substring(p);
      }
    } else {
      textRemaining = "";
    }
    
    drawString(x, y + (current_line * line_spacing), line, alignment, color);
    current_line++;
  }
}

/**
 * Draw current weather conditions section.
 * Displays large weather icon, current temperature, feels-like temperature,
 * and weather details (sunrise, sunset, humidity, pressure, wind) in a left column.
 * 
 * @param current Array containing current weather data (index 0)
 * @param wifi_signal WiFi signal strength (currently unused in this function)
 */
void drawCurrentConditions(Forecast_record_type *current, int wifi_signal) {
  String dataStr, unitStr;
  
  // Large weather icon positioned in upper left
  DisplayConditionsSection(122, 117, current[0].Icon, LargeIcon);
  
  // Current temperature display (large font)
  setFont(OpenSans24B);
  int tempX = 240; // Position to right of icon
  int tempY = 50;
  
  dataStr = String((int)round(current[0].Temperature));
  unitStr = (String(settings.Units) == "M") ? "°C" : "°F";
  drawString(tempX, tempY, dataStr, LEFT, Black);
  setFont(OpenSans12B);
  drawString(tempX + getStringWidth(dataStr) + 30, tempY - 5, unitStr, LEFT, Black);
  
  // Feels-like temperature (label + value)
  setFont(OpenSans12B);
  drawString(tempX, tempY + 40, TXT_FEELSLIKE, LEFT, Black);
  setFont(OpenSans24B);
  String feelsLikeNum = String((int)round(current[0].FeelsLike));
  drawString(tempX, tempY + 70, feelsLikeNum, LEFT, Black);
  setFont(OpenSans12B);
  String feelsLikeUnit = String(settings.Units) == "M" ? "°C" : "°F";
  drawString(tempX + getStringWidth(feelsLikeNum) + 30, tempY + 65, feelsLikeUnit, LEFT, Black);
  
  // Weather details column (left side of screen)
  int detailsX = 5;
  int gridY = 180;
  int rowHeight = 65;
  
  // Display weather details in vertical list
  setFont(OpenSans12B);
  drawString(detailsX, gridY + 12, TXT_SUNRISE, LEFT, Black);
  setFont(OpenSans18B);
  drawString(detailsX, gridY + 38, ConvertUnixTime(current[0].Sunrise, current[0].FTimezone), LEFT, Black);
  
  gridY += rowHeight;
  setFont(OpenSans12B);
  drawString(detailsX, gridY + 12, TXT_SUNSET, LEFT, Black);
  setFont(OpenSans18B);
  drawString(detailsX, gridY + 38, ConvertUnixTime(current[0].Sunset, current[0].FTimezone), LEFT, Black);
  
  gridY += rowHeight;
  setFont(OpenSans12B);
  drawString(detailsX, gridY + 12, TXT_HUMIDITY, LEFT, Black);
  setFont(OpenSans18B);
  drawString(detailsX, gridY + 41, String((int)round(current[0].Humidity)) + "%", LEFT, Black);
  
  gridY += rowHeight;
  setFont(OpenSans12B);
  drawString(detailsX, gridY + 12, TXT_PRESSURE, LEFT, Black);
  setFont(OpenSans18B);
  if (String(settings.Units) == "M") {
    dataStr = String((int)round(current[0].Pressure)) + " hPa";
  } else {
    float pressureInHg = current[0].Pressure * 0.02953; // Convert hPa to inches Hg
    dataStr = String(pressureInHg, 1) + " in";
  }
  drawString(detailsX, gridY + 38, dataStr, LEFT, Black);
  
  gridY += rowHeight;
  setFont(OpenSans12B);
  drawString(detailsX, gridY + 12, TXT_WIND, LEFT, Black);
  setFont(OpenSans18B);
  if (String(settings.Units) == "M") {
    dataStr = String((int)round(current[0].Windspeed)) + " m/s";
  } else {
    dataStr = String((int)round(current[0].Windspeed)) + " mph";
  }
  drawString(detailsX, gridY + 28, dataStr, LEFT, Black);
}

/**
 * Determine appropriate weather icon based on daily conditions.
 * Priority: snow > thunderstorm > rain > cloud cover.
 * 
 * @param avgCloudCover Average cloud cover percentage (0-100)
 * @param maxPop Maximum probability of precipitation (0.0-1.0)
 * @param totalRainfall Total rainfall for the day (mm or inches)
 * @param totalSnowfall Total snowfall for the day (mm or inches)
 * @param isDay true for day icons, false for night icons
 * @return OpenWeatherMap icon code (e.g., "01d", "10n")
 */
String getIconFromCloudCover(int avgCloudCover, float maxPop, float totalRainfall, float totalSnowfall, bool isDay) {
  char dayNight = isDay ? 'd' : 'n';
  
  // Priority 1: Snow (icon 13)
  if (totalSnowfall > 0.5) {
    return String("13") + dayNight;
  }
  
  // Priority 2: Thunderstorm (icon 11) - high probability or heavy rain
  if (maxPop > 0.5 || totalRainfall > 2.0) {
    return String("11") + dayNight;
  }
  
  // Priority 3: Rain (icon 10) - moderate probability or light rain
  if (maxPop > 0.3 || totalRainfall > 0.5) {
    return String("10") + dayNight;
  }
  
  // Priority 4: Cloud cover determines icon
  if (avgCloudCover <= 10) {
    return String("01") + dayNight; // Clear sky
  } else if (avgCloudCover <= 25) {
    return String("02") + dayNight; // Few clouds
  } else if (avgCloudCover <= 50) {
    return String("03") + dayNight; // Scattered clouds
  } else if (avgCloudCover <= 75) {
    return String("04") + dayNight; // Broken clouds
  } else {
    return String("04") + dayNight; // Overcast (uses broken clouds icon)
  }
}

/**
 * Draw 5-day forecast display.
 * Groups forecast periods by day, calculates daily statistics (high/low temp, avg cloud cover),
 * and displays one entry per day with appropriate weather icon.
 * 
 * @param forecast Array of daily forecast records
 * @param num_forecasts Number of forecast entries available
 * @param timeInfo Current time structure for day-of-week labels
 */
void drawForecast(Forecast_record_type *forecast, int num_forecasts, tm *timeInfo) {
  // Layout: 5 forecast boxes in top right area
  int forecastY = 200;
  int forecastWidth = 115; // Width per day (960px / 5 days with padding)
  int forecastXStart = 385;
  
  // Structure to hold aggregated daily statistics
  struct DailyForecast {
    float highTemp;        // Maximum temperature for the day
    float lowTemp;         // Minimum temperature for the day
    int totalCloudCover;   // Sum of cloud cover values (for averaging)
    int cloudCoverCount;   // Number of periods used for average
    float maxPop;          // Maximum probability of precipitation
    float totalRainfall;   // Total rainfall accumulation
    float totalSnowfall;   // Total snowfall accumulation
    int firstPeriodIndex; // Index of first forecast period for this day
    time_t dayTime;        // Timestamp for day-of-week calculation
  };
  
  DailyForecast dailyForecasts[5];
  int dayCount = 0;
  int lastDayOfYear = -1; // Track day of year to handle month boundaries correctly
  
  // Group forecast periods by day and aggregate statistics
  for (int i = 0; i < num_forecasts; i++) {
    time_t forecastTime = forecast[i].Dt;
    struct tm *forecastTm = localtime(&forecastTime);
    int forecastDayOfYear = forecastTm->tm_yday; // Day of year (0-365) handles month boundaries
    
    if (forecastDayOfYear != lastDayOfYear) {
      // New day detected
      if (dayCount < 5) {
        // Finalize previous day's cloud cover average before starting new day
        if (dayCount > 0 && dailyForecasts[dayCount - 1].cloudCoverCount > 0) {
          dailyForecasts[dayCount - 1].totalCloudCover /= dailyForecasts[dayCount - 1].cloudCoverCount;
        }
        
        // Initialize new day with first period's data
        dailyForecasts[dayCount].highTemp = forecast[i].High;
        dailyForecasts[dayCount].lowTemp = forecast[i].Low;
        dailyForecasts[dayCount].totalCloudCover = forecast[i].Cloudcover;
        dailyForecasts[dayCount].cloudCoverCount = 1;
        dailyForecasts[dayCount].maxPop = forecast[i].Pop;
        dailyForecasts[dayCount].totalRainfall = forecast[i].Rainfall;
        dailyForecasts[dayCount].totalSnowfall = forecast[i].Snowfall;
        dailyForecasts[dayCount].firstPeriodIndex = i;
        dailyForecasts[dayCount].dayTime = forecastTime;
        dayCount++;
        lastDayOfYear = forecastDayOfYear;
      } else {
        // Have 5 days, stop processing
        break;
      }
    } else {
      // Same day - aggregate statistics
      if (dayCount > 0) {
        int dayIdx = dayCount - 1;
        // Track daily min/max temperatures
        if (forecast[i].High > dailyForecasts[dayIdx].highTemp) {
          dailyForecasts[dayIdx].highTemp = forecast[i].High;
        }
        if (forecast[i].Low < dailyForecasts[dayIdx].lowTemp) {
          dailyForecasts[dayIdx].lowTemp = forecast[i].Low;
        }
        // Accumulate cloud cover for averaging
        dailyForecasts[dayIdx].totalCloudCover += forecast[i].Cloudcover;
        dailyForecasts[dayIdx].cloudCoverCount++;
        // Track maximum precipitation probability
        if (forecast[i].Pop > dailyForecasts[dayIdx].maxPop) {
          dailyForecasts[dayIdx].maxPop = forecast[i].Pop;
        }
        // Accumulate precipitation totals
        dailyForecasts[dayIdx].totalRainfall += forecast[i].Rainfall;
        dailyForecasts[dayIdx].totalSnowfall += forecast[i].Snowfall;
      }
    }
  }
  
  // Finalize last day's cloud cover average
  if (dayCount > 0 && dailyForecasts[dayCount - 1].cloudCoverCount > 0) {
    dailyForecasts[dayCount - 1].totalCloudCover /= dailyForecasts[dayCount - 1].cloudCoverCount;
  }
  
  // Display each day
  for (int day = 0; day < dayCount; day++) {
    int x = forecastXStart + (day * forecastWidth);
    
    // Get day of week from the day's timestamp
    struct tm *dayTm = localtime(&dailyForecasts[day].dayTime);
    setFont(OpenSans12B);
    char dayBuffer[8] = {};
    strftime(dayBuffer, sizeof(dayBuffer), "%a", dayTm);
    drawString(x + forecastWidth / 2, forecastY - 110, dayBuffer, CENTER, Black);
    
    // Determine day/night icon based on first period's hour (6 AM - 6 PM = day)
    struct tm *firstPeriodTm = localtime(&dailyForecasts[day].dayTime);
    int hour = firstPeriodTm->tm_hour;
    bool isDay = (hour >= 6 && hour < 18);
    
    // Get icon based on average cloud cover and precipitation
    String dayIcon = getIconFromCloudCover(
      dailyForecasts[day].totalCloudCover,
      dailyForecasts[day].maxPop,
      dailyForecasts[day].totalRainfall,
      dailyForecasts[day].totalSnowfall,
      isDay
    );
    
    // Weather icon - small size
    DisplayConditionsSection(x + forecastWidth / 2, forecastY - 40, dayIcon, SmallIcon);
    
    // High | Low temperatures - daily overall high/low
    setFont(OpenSans10B);
    String hiStr = String((int)round(dailyForecasts[day].highTemp)) + "°";
    String loStr = String((int)round(dailyForecasts[day].lowTemp)) + "°";
    String tempStr = hiStr + "|" + loStr;
    drawString(x + forecastWidth / 2, forecastY + 15, tempStr, CENTER, Black);
  }
}

/**
 * Draw location name and date in top right corner of display.
 * 
 * @param city City name string
 * @param date Formatted date string
 */
void drawLocationDate(const String &city, const String &date) {
  setFont(OpenSans18B);
  drawString(DISP_WIDTH - 7, 0, city, RIGHT, Black);
  setFont(OpenSans12B);
  drawString(DISP_WIDTH - 7, 42, date, RIGHT, Black);
}

/**
 * Draw 24-hour temperature and precipitation graph with dual Y-axes.
 * Filters forecast data to only include the next 24 hours based on timestamps.
 * Temperature line is drawn on top of precipitation bars.
 * 
 * @param forecast Array of hourly forecast records
 * @param num_forecasts Number of forecast entries available
 * @param timeInfo Current time structure (unused, kept for compatibility)
 */
void drawOutlookGraph(Forecast_record_type *forecast, int num_forecasts, tm *timeInfo) {
  const int HOURS_TO_SHOW = 24;
  const int SECONDS_PER_HOUR = 3600;
  
  // Filter forecasts to only include next 24 hours
  time_t now = time(NULL);
  time_t cutoffTime = now + (HOURS_TO_SHOW * SECONDS_PER_HOUR);
  
  int validForecastIndices[24]; // Maximum 24 hours of data
  int forecastCount = 0;
  
  // Include forecasts within 24-hour window (allow 1 hour in past for current period)
  for (int i = 0; i < num_forecasts && forecastCount < 24; i++) {
    time_t forecastTime = forecast[i].Dt;
    if (forecastTime >= (now - SECONDS_PER_HOUR) && forecastTime <= cutoffTime) {
      validForecastIndices[forecastCount] = i;
      forecastCount++;
    }
  }
  
  // Fallback if no valid forecasts found
  if (forecastCount == 0 && num_forecasts > 0) {
    validForecastIndices[0] = 0;
    forecastCount = 1;
  }
  
  // Graph dimensions and position
  int graphX = 240;
  int graphY = 255;
  int graphWidth = 665;
  int graphHeight = 230;
  
  // Calculate temperature range for left Y-axis scaling
  float tempMin = forecast[validForecastIndices[0]].Temperature;
  float tempMax = forecast[validForecastIndices[0]].Temperature;
  for (int i = 0; i < forecastCount; i++) {
    int idx = validForecastIndices[i];
    if (forecast[idx].Temperature < tempMin) tempMin = forecast[idx].Temperature;
    if (forecast[idx].Temperature > tempMax) tempMax = forecast[idx].Temperature;
  }
  
  // Add 10% padding above/below for better visualization
  float tempRange = tempMax - tempMin;
  if (tempRange < 1.0) tempRange = 10.0; // Ensure minimum range
  tempMin -= tempRange * 0.1;
  tempMax += tempRange * 0.1;
  
  // Precipitation scale (right Y-axis): 0% to 100%
  float rainMin = 0.0;
  float rainMax = 100.0;
  
  // Draw graph background border - only top and bottom horizontal lines (vertical axis lines removed)
  epd_draw_hline(graphX, graphY, graphWidth, Black, framebuffer); // Top border
  epd_draw_hline(graphX, graphY + graphHeight, graphWidth, Black, framebuffer); // Bottom border
  
  // Draw left Y-axis (temperature) with labels
  setFont(OpenSans8B);
  int leftAxisX = graphX - 5;
  int numTempTicks = 5;
  for (int i = 0; i <= numTempTicks; i++) {
    float tempValue = tempMin + (tempMax - tempMin) * (float)i / numTempTicks;
    int y = graphY + graphHeight - (i * graphHeight / numTempTicks);
    // Tick marks removed as requested
    // Draw label
    char tempLabel[15];
    if (String(settings.Units) == "M") {
      snprintf(tempLabel, sizeof(tempLabel), "%.0f°C", tempValue);
    } else {
      snprintf(tempLabel, sizeof(tempLabel), "%.0f°F", tempValue);
    }
    drawString(leftAxisX - 10, y, String(tempLabel), RIGHT, Black);
    // Draw horizontal grid line across entire graph width in grey
    epd_draw_hline(graphX, y, graphWidth, Grey, framebuffer);
  }
  
  // Draw right Y-axis (rain percentage) with labels
  int rightAxisX = graphX + graphWidth + 5;
  int numRainTicks = 5;
  for (int i = 0; i <= numRainTicks; i++) {
    int rainValue = (int)(rainMin + (rainMax - rainMin) * (float)i / numRainTicks);
    int y = graphY + graphHeight - (i * graphHeight / numRainTicks);
    // Tick marks removed as requested
    // Draw label
    char rainLabel[10];
    snprintf(rainLabel, sizeof(rainLabel), "%d%%", rainValue);
    drawString(rightAxisX + 10, y, String(rainLabel), LEFT, Black);
  }
  
  // Vertical axis lines removed as requested
  
  // Draw precipitation bars first (so temperature line appears on top)
  uint8_t greyColor = 0xDD; // Light grey for precipitation bars
  
  for (int i = 0; i < forecastCount; i++) {
    int idx = validForecastIndices[i];
    // Convert probability of precipitation (0.0-1.0) to percentage (0-100%)
    float rainPercent = forecast[idx].Pop * 100.0;
    rainPercent = (rainPercent > 100.0) ? 100.0 : ((rainPercent < 0.0) ? 0.0 : rainPercent);
    
    // Calculate bar dimensions with zero gap between bars
    // Each bar starts exactly where the previous one ends
    int barX = graphX + (i * graphWidth / forecastCount);
    // Calculate width to ensure bars fill entire graph width with no gaps
    int nextBarX = (i < forecastCount - 1) ? (graphX + ((i + 1) * graphWidth / forecastCount)) : (graphX + graphWidth);
    int currentBarWidth = nextBarX - barX;
    
    float rainRatio = rainPercent / 100.0;
    // For testing set barHeight to 50% of graphHeight
    int barHeight = (int)(rainRatio * graphHeight);
    int barY = graphY + graphHeight - barHeight;
    
    if (barHeight > 0) {
      epd_fill_rect(barX, barY, currentBarWidth, barHeight, greyColor, framebuffer);
    }
  }
  
  // Draw temperature line (2px thick) on top of bars
  if (forecastCount > 1) {
    for (int i = 0; i < forecastCount - 1; i++) {
      int idx1 = validForecastIndices[i];
      int idx2 = validForecastIndices[i + 1];
      
      // Calculate line endpoints
      int x1 = graphX + (i * graphWidth / forecastCount);
      float tempRatio1 = (forecast[idx1].Temperature - tempMin) / (tempMax - tempMin);
      int y1 = graphY + graphHeight - (int)(tempRatio1 * graphHeight);
      
      int x2 = graphX + ((i + 1) * graphWidth / forecastCount);
      float tempRatio2 = (forecast[idx2].Temperature - tempMin) / (tempMax - tempMin);
      int y2 = graphY + graphHeight - (int)(tempRatio2 * graphHeight);
      
      // Draw 2px thick line (two 1px lines offset by 1px)
      epd_write_line(x1, y1, x2, y2, Black, framebuffer);
      epd_write_line(x1 + 1, y1, x2 + 1, y2, Black, framebuffer);
    }
  }
  
  // Draw x-axis time labels (approximately 4-5 labels across 24 hours)
  setFont(OpenSans8B);
  int labelInterval = (forecastCount > 0) ? (forecastCount / 4) : 6;
  if (labelInterval < 1) labelInterval = 1;
  
  for (int i = 0; i < forecastCount; i += labelInterval) {
    int idx = validForecastIndices[i];
    int x = graphX + (i * graphWidth / forecastCount);
    char timeBuffer[12] = {};
    time_t ts = forecast[idx].Dt;  // UTC timestamp from API
    // Convert UTC to local time by adding timezone offset
    // globalTimezoneOffset: positive = east of UTC (ahead), negative = west of UTC (behind)
    time_t local_ts = ts + globalTimezoneOffset;
    struct tm *t = gmtime(&local_ts);  // Use gmtime since we've already applied offset
    
    if (String(settings.Units) == "M") {
      strftime(timeBuffer, sizeof(timeBuffer), "%H", t); // 24-hour format
    } else {
      // 12-hour format without leading zero (e.g., "7AM" not "07AM")
      int hour12 = t->tm_hour % 12;
      if (hour12 == 0) hour12 = 12;
      char ampm = (t->tm_hour < 12) ? 'A' : 'P';
      snprintf(timeBuffer, sizeof(timeBuffer), "%d%cM", hour12, ampm);
    }
    drawString(x, graphY + graphHeight + 15, String(timeBuffer), CENTER, Black);
  }
}

/**
 * Draw WiFi signal strength bars based on RSSI value.
 * Displays 1-5 bars depending on signal strength, with numeric RSSI value in dB.
 * 
 * @param x Left position of WiFi icon
 * @param y Bottom position of WiFi bars (bars extend upward)
 * @param rssi Signal strength in dBm (-100 to 0, typically -90 to -30)
 */
void drawWiFiSignal(int x, int y, int rssi) {
  int WIFIsignal = 0;
  int xpos = 1;
  
  // Draw bars from weakest to strongest (left to right)
  // Each bar represents a 20dB range
  for (int _rssi = -100; _rssi <= rssi; _rssi = _rssi + 20) {
    // Determine bar height based on RSSI threshold
    if (_rssi <= -20)  WIFIsignal = 20; // 5 bars: -20dBm or better
    if (_rssi <= -40)  WIFIsignal = 16; // 4 bars: -40 to -21dBm
    if (_rssi <= -60)  WIFIsignal = 12; // 3 bars: -60 to -41dBm
    if (_rssi <= -80)  WIFIsignal = 8;  // 2 bars: -80 to -61dBm
    if (_rssi <= -100) WIFIsignal = 4;  // 1 bar:  -100 to -81dBm
    
    // Draw filled bars if signal present, outline only if no signal
    if (rssi != 0) {
      fillRect(x + xpos * 8, y - WIFIsignal, 6, WIFIsignal, Black);
    } else {
      drawRect(x + xpos * 8, y - WIFIsignal, 6, WIFIsignal, Black);
    }
    xpos++;
  }
  if (rssi == 0) {
    drawString(x + 28, y - 18, "x", LEFT, Black);
  } else {
    // Draw RSSI value in dB to the right of the bars
    // Bars end around x + 50 (5 bars * 8px spacing + 6px width), add spacing
    setFont(OpenSans8B);
    String rssiStr = String(rssi) + " dB";
    drawString(x + 50, y-14, rssiStr, LEFT, Black);
  }
}

/**
 * Draw battery icon with fill level, percentage, and voltage.
 * Icon consists of a rectangle with a small terminal on the right side.
 * Fill level is proportional to battery percentage.
 * 
 * @param x Left position (icon drawn at x+25)
 * @param y Bottom position (icon drawn at y-14)
 * @param percentage Battery charge percentage (0-100)
 * @param voltage Battery voltage in volts
 */
void drawBatteryIcon(int x, int y, uint8_t percentage, float voltage) {
  int batWidth = 40;
  int batHeight = 15;
  int terminalWidth = 4;
  int terminalHeight = 7;
  
  // Draw battery body outline
  drawRect(x + 25, y - 14, batWidth, batHeight, Black);
  
  // Draw positive terminal (small rectangle on right side, centered vertically)
  int terminalX = x + 25 + batWidth;
  int terminalY = y - 14 + (batHeight - terminalHeight) / 2;
  fillRect(terminalX, terminalY, terminalWidth, terminalHeight, Black);
  
  // Draw battery fill level (proportional to percentage)
  // If voltage > 4.2V, show as fully charged (100% fill)
  uint8_t displayPercentage = (voltage > 4.2) ? 100 : percentage;
  if (displayPercentage > 0) {
    int fillWidth = (batWidth - 2) * displayPercentage / 100; // Account for 1px border on each side
    if (fillWidth > 0) {
      fillRect(x + 27, y - 12, fillWidth, batHeight - 2, Black);
    }
  }
  
  // Draw percentage/charging status and voltage text to the right of icon
  setFont(OpenSans8B);
  String batStr;
  if (voltage > 4.35) {
    // Battery voltage above 4.2V indicates charging
    batStr = "Charging  " + String(voltage, 1) + "v";
    drawString(x + 85, y - 17, batStr, LEFT, Black);
  } else {
    batStr = String(percentage) + "%  " + String(voltage, 1) + "v";
    drawString(x + 85, y - 13, batStr, LEFT, Black);
  }
}

/**
 * Draw status bar at bottom of display.
 * Layout: WiFi signal (left), Refresh time (center), Battery (right).
 * 
 * @param statusStr Status message (currently unused)
 * @param refreshTimeStr Time string to display in center
 * @param rssi WiFi signal strength in dBm
 * @param batVoltage Battery voltage in millivolts
 */
void drawStatusBar(const String &statusStr, const String &refreshTimeStr, int rssi, uint32_t batVoltage) {
  int barY = DISP_HEIGHT - 25; // Vertical position of status bar
  
  // Draw light grey background for status bar (color 0xEE)
  fillRect(0, barY, DISP_WIDTH, DISP_HEIGHT - barY, 0xEE);
  
  // Draw separator line at top of status bar
  drawLine(0, barY, DISP_WIDTH, barY, Black);
  
  // WiFi signal strength icon on left
  drawWiFiSignal(2, barY + 20, rssi);
  
  // Calculate battery percentage from voltage
  uint8_t percentage = 100;
  float voltage = 0.0;
  if (batVoltage > 0) {
    voltage = batVoltage / 1000.0; // Convert millivolts to volts
    
    if (voltage >= 4.20) {
      percentage = 100; // Fully charged
    } else if (voltage <= 3.20) {
      percentage = 0; // Empty
    } else {
      // Polynomial curve fit for Li-ion battery (more accurate than linear)
      // Formula derived from typical Li-ion discharge curve
      percentage = 2836.9625 * pow(voltage, 4) - 43987.4889 * pow(voltage, 3) + 
                    255233.8134 * pow(voltage, 2) - 656689.7123 * voltage + 632041.7303;
      percentage = (percentage > 100) ? 100 : ((percentage < 0) ? 0 : percentage);
    }
  }
  
  // Refresh time centered at bottom
  setFont(OpenSans8B);
  drawString(DISP_WIDTH / 2, barY + 5, refreshTimeStr, CENTER, Black);
  
  // Battery icon and text on right
  // Calculate total width: icon starts at x+25, text ends at x+85+textWidth
  setFont(OpenSans8B);
  String batStr;
  if (voltage > 4.35) {
    batStr = "Charging  " + String(voltage, 1) + "v";
  } else {
    batStr = String(percentage) + "%  " + String(voltage, 1) + "v";
  }
  uint16_t textWidth = getStringWidth(batStr);
  int totalWidth = 85 + textWidth; // Distance from icon start (x+25) to text end
  int batteryX = DISP_WIDTH - 2 - totalWidth; // Position from right edge
  int batteryY = barY + 17; // Align with time text (battery draws at y-14, time at barY)
  
  drawBatteryIcon(batteryX, batteryY, percentage, voltage);
}

/**
 * Draw low battery warning screen.
 * Displays "Low Battery" message centered on white background.
 * Called when battery voltage is at or below 3.2V.
 */
void drawLowBatteryScreen() {
  // Ensure framebuffer is white
  memset(framebuffer, 0xFF, EPD_WIDTH * EPD_HEIGHT / 2);
  
  // Set font to OpenSans24B
  setFont(OpenSans24B);
  
  // Calculate center position for "Low Battery" text
  String message = "Low Battery";
  uint16_t textWidth = getStringWidth(message);
  uint16_t textHeight = getStringHeight(message);
  int x = DISP_WIDTH / 2;
  int y = DISP_HEIGHT / 2;
  
  // Draw centered text
  drawString(x, y, message, CENTER, Black);
}

/**
 * Draw setup mode screen.
 * Displays "Setup Mode" centered on a white background.
 */
void drawSetupModeScreen() {
  // Ensure framebuffer is white
  memset(framebuffer, 0xFF, EPD_WIDTH * EPD_HEIGHT / 2);
  
  int centerX = DISP_WIDTH / 2;
  int centerY = DISP_HEIGHT / 2;
  
  // Draw "Setup Mode" in OpenSans24B
  setFont(OpenSans24B);
  String title = "Setup Mode";
  uint16_t titleHeight = getStringHeight(title);
  
  // Draw remaining lines in OpenSans18B
  setFont(OpenSans18B);
  String line1 = "Connect to Wifi \"ESP Weather Station\"";
  String line2 = "and go to the site \"192.168.4.1\" to configure.";
  String line3 = "Or press the reset button to cancel.";
  
  uint16_t lineHeight = getStringHeight(line1);
  int lineSpacing = lineHeight + 10; // 10px spacing between lines
  
  // Calculate total height of all text to center vertically
  int totalHeight = titleHeight + 20 + (lineHeight * 3) + (lineSpacing * 2); // 20px gap between title and lines
  int startY = centerY - (totalHeight / 2);
  
  // Draw title
  setFont(OpenSans24B);
  drawString(centerX, startY, title, CENTER, Black);
  
  // Draw three lines below title
  setFont(OpenSans18B);
  int lineStartY = startY + titleHeight + 20; // 20px gap after title
  drawString(centerX, lineStartY, line1, CENTER, Black);
  drawString(centerX, lineStartY + lineSpacing, line2, CENTER, Black);
  drawString(centerX, lineStartY + (lineSpacing * 2) + 4, line3, CENTER, Black);
}

/**
 * Draw WiFi connection error screen.
 * Displays "Wifi Connection Failed" message centered on white background.
 * Called when WiFi connection fails.
 */
void drawWiFiErrorScreen() {
  // Ensure framebuffer is white
  memset(framebuffer, 0xFF, EPD_WIDTH * EPD_HEIGHT / 2);
  
  // Set font to OpenSans24B
  setFont(OpenSans24B);
  
  // Calculate center position for error message text
  String message = "Wifi Connection Failed";
  uint16_t textWidth = getStringWidth(message);
  uint16_t textHeight = getStringHeight(message);
  int x = DISP_WIDTH / 2;
  int y = DISP_HEIGHT / 2;
  
  // Draw centered text
  drawString(x, y, message, CENTER, Black);
}

/**
 * Draw invalid location error screen.
 * Displays error message with instructions for entering setup mode.
 */
void drawInvalidLocationScreen() {
  // Ensure framebuffer is white
  memset(framebuffer, 0xFF, EPD_WIDTH * EPD_HEIGHT / 2);
  
  int centerX = DISP_WIDTH / 2;
  int startY = DISP_HEIGHT / 4; // Start at 1/4 from top
  
  // Draw title "Invalid Location String" in OpenSans24B
  setFont(OpenSans24B);
  String title = "Invalid Location String";
  uint16_t titleHeight = getStringHeight(title);
  drawString(centerX, startY, title, CENTER, Black);
  
  // Draw body text in OpenSans18B
  setFont(OpenSans18B);
  int currentY = startY + titleHeight + 30; // 30px spacing after title
  
  String line1 = "Go into setup mode to correct.";
  
  drawString(centerX, currentY, line1, CENTER, Black);
}

/**
 * Draw invalid API key error screen.
 * Displays error message with instructions for entering setup mode.
 */
void drawInvalidAPIKeyScreen() {
  // Ensure framebuffer is white
  memset(framebuffer, 0xFF, EPD_WIDTH * EPD_HEIGHT / 2);
  
  int centerX = DISP_WIDTH / 2;
  int startY = DISP_HEIGHT / 4; // Start at 1/4 from top
  
  // Draw title "OpenWeatherMap API Key Invalid" in OpenSans24B
  setFont(OpenSans24B);
  String title = "OpenWeatherMap API Key Invalid";
  uint16_t titleHeight = getStringHeight(title);
  drawString(centerX, startY, title, CENTER, Black);
  
  // Draw body text in OpenSans18B
  setFont(OpenSans18B);
  int currentY = startY + titleHeight + 30; // 30px spacing after title
  
  String line1 = "Enter setup mode to enter a correct API key.";
  
  drawString(centerX, currentY, line1, CENTER, Black);
}
