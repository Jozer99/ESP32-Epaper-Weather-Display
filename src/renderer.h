#ifndef __RENDERER_H__
#define __RENDERER_H__

#include <Arduino.h>
#include <time.h>
#include "forecast_record.h"
#include "epd_driver.h"

// Display dimensions for LilyGo T5 4.7
#define DISP_WIDTH  960
#define DISP_HEIGHT 540

typedef enum alignment {
  LEFT,
  RIGHT,
  CENTER
} alignment_t;

// Main rendering functions
void initDisplay();
void powerOffDisplay();
void drawCurrentConditions(Forecast_record_type *current, int wifi_signal);
void drawForecast(Forecast_record_type *forecast, int num_forecasts, tm *timeInfo);
void drawLocationDate(const String &city, const String &date);
void drawOutlookGraph(Forecast_record_type *forecast, int num_forecasts, tm *timeInfo);
void drawStatusBar(const String &statusStr, const String &refreshTimeStr, int rssi, uint32_t batVoltage);
void drawLowBatteryScreen();
void drawWiFiErrorScreen();
void drawSetupModeScreen();
void drawInvalidLocationScreen();
void drawInvalidAPIKeyScreen();

// Helper drawing functions
void drawString(int16_t x, int16_t y, const String &text, alignment_t alignment, uint8_t color = 0x00);
void drawMultiLnString(int16_t x, int16_t y, const String &text, alignment_t alignment, 
                       uint16_t max_width, uint16_t max_lines, int16_t line_spacing, uint8_t color = 0x00);
uint16_t getStringWidth(const String &text);
uint16_t getStringHeight(const String &text);
void setFont(GFXfont const &font);

// Icon drawing functions
void DisplayConditionsSection(int x, int y, String IconName, bool IconSize);

// External framebuffer (defined in main.ino)
extern uint8_t *framebuffer;
extern GFXfont currentFont;

#endif

