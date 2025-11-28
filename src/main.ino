/*
 * Weather Station for LilyGo T5 4.7 E-Paper Display
 * 
 * Features:
 * - Current weather conditions with large icon display
 * - 5-day forecast grouped by day with daily high/low temperatures
 * - 24-hour temperature and precipitation graph
 * - Battery and WiFi signal strength indicators
 * - Low battery protection with visual warning
 * - Deep sleep power management with wake hour scheduling
 * 
 * Uses OpenWeatherMap One Call API 3.0 for weather data.
 * Time synchronization via API response (no NTP required).
 */

#include <Arduino.h>
#include <esp_task_wdt.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "epd_driver.h"
#include "esp_adc_cal.h"
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <SPI.h>
#include <time.h>
#include <sys/time.h>
#include "lang.h"
#include "forecast_record.h"
#include "renderer.h"
#include "setup_mode.h"
#include "settings.h"

#define SCREEN_WIDTH   EPD_WIDTH
#define SCREEN_HEIGHT  EPD_HEIGHT

// Color definitions for e-paper display (grayscale values)
#define White         0xFF
#define LightGrey     0xBB
#define Grey          0x88
#define DarkGrey      0x44
#define Black         0x00

// Program variables
#define max_hourly_readings 48  // One Call API 3.0 provides 48 hours of hourly forecasts
#define max_daily_readings 8    // One Call API 3.0 provides 8 days of daily forecasts

Forecast_record_type  WxConditions[1];
Forecast_record_type  WxHourlyForecast[max_hourly_readings];  // Hourly forecasts for 24-hour graph
Forecast_record_type  WxDailyForecast[max_daily_readings];    // Daily forecasts for 5-day display

// Runtime variables (not configuration - these change during execution)
long StartTime       = 0;  // Timestamp when device woke up
long SleepTimer      = 0;  // Calculated sleep duration in seconds
long Delta           = 30; // RTC speed compensation offset (seconds)

// Fonts
#include "OpenSans8B.h"
#include "OpenSans10B.h"
#include "OpenSans12B.h"
#include "OpenSans18B.h"
#include "OpenSans24B.h"
#include "moon.h"
#include "sunrise.h"
#include "sunset.h"

// Global variables shared with renderer
GFXfont  currentFont;
uint8_t *framebuffer;

String  Time_str = "--:--:--";
String  Date_str = "-- --- ----";
int     wifi_signal, CurrentHour = 0, CurrentMin = 0, CurrentSec = 0, EventCnt = 0, vref = 1100;

// RTC memory variable to track if low battery screen has been shown
// This persists across deep sleep
RTC_DATA_ATTR bool lowBatteryScreenShown = false;

// Function prototypes
void BeginSleep();
uint8_t StartWiFi();
void StopWiFi();
void InitialiseSystem();
bool DecodeWeather(WiFiClient& json);
int obtainWeatherData(WiFiClient & client); // Returns: 0 = success, 1 = API key invalid (401), 2 = other error
boolean UpdateLocalTime();
void SetRTCTimeFromAPI(time_t apiTime, int timezoneOffset);
void DisplayWeather();  // Main display function - adapted to use GUI layout
String ConvertUnixTime(int unix_time);
uint32_t readBatteryVoltage();

/**
 * Prepare device for deep sleep and enter sleep mode.
 * Calculates sleep duration to align with minute boundaries based on SleepDuration.
 * The Delta offset compensates for ESP32 RTC drift.
 */
void BeginSleep() {
  epd_poweroff_all();
  UpdateLocalTime();
  
  // Calculate sleep duration: align to next SleepDuration minute boundary
  // Example: if SleepDuration=60 and current time is 14:23:45, sleep until 15:00:00
  int minutesUntilNext = settings.SleepDuration - (CurrentMin % settings.SleepDuration);
  int secondsUntilNext = (minutesUntilNext * 60) - CurrentSec;
  SleepTimer = secondsUntilNext + Delta; // Add compensation offset
  
  esp_sleep_enable_timer_wakeup(SleepTimer * 1000000LL); // Convert to microseconds
  Serial.println("Awake for : " + String((millis() - StartTime) / 1000.0, 3) + "-secs");
  Serial.println("Entering " + String(SleepTimer) + " (secs) of sleep time");
  Serial.println("Starting deep-sleep period...");
  esp_deep_sleep_start();
}

/**
 * Set ESP32 RTC time from OpenWeatherMap API response.
 * 
 * The API provides time as a Unix timestamp (UTC). We store UTC in the RTC
 * Timezone is configured from the API's timezone_offset to convert UTC to local time.
 * 
 * @param apiTime Unix timestamp from API (UTC)
 * @param timezoneOffset Timezone offset in seconds from UTC (positive = east of UTC, negative = west of UTC)
 */
void SetRTCTimeFromAPI(time_t apiTime, int timezoneOffset) {
  // Store UTC time in RTC - localtime() will handle timezone conversion
  struct timeval tv;
  tv.tv_sec = apiTime;
  tv.tv_usec = 0;
  settimeofday(&tv, NULL);
  
  // Construct timezone string from API offset (format: GMT+5 or GMT-5)
  // Convert seconds to hours (offset is in seconds, POSIX format uses hours)
  int offsetHours = timezoneOffset / 3600;
  char tzString[16];
  if (offsetHours >= 0) {
    snprintf(tzString, sizeof(tzString), "GMT+%d", offsetHours);
  } else {
    snprintf(tzString, sizeof(tzString), "GMT%d", offsetHours); // Negative sign included
  }
  
  // Configure timezone for automatic UTC->local conversion
  setenv("TZ", tzString, 1);
  tzset();
  
  Serial.println("RTC set from API time (UTC)");
  time_t now = time(NULL);
  struct tm *timeinfo = localtime(&now);
  Serial.print("Current local time: ");
  Serial.println(timeinfo, "%a %b %d %Y   %H:%M:%S");
  Serial.print("Timezone from API: ");
  Serial.print(tzString);
  Serial.print(" (offset: ");
  Serial.print(timezoneOffset / 3600.0);
  Serial.println(" hours)");
}

/**
 * Connect to WiFi network and retrieve signal strength.
 * Attempts connection twice if first attempt fails.
 * 
 * @return WiFi status (WL_CONNECTED on success)
 */
uint8_t StartWiFi() {
  Serial.println("\r\nWiFi Connecting to: " + String(settings.ssid));
  IPAddress dns(8, 8, 8, 8); // Google DNS
  WiFi.disconnect();
  WiFi.mode(WIFI_STA); // Station mode (client)
  WiFi.begin(settings.ssid, settings.password);
  
  if (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.printf("WiFi connection failed, retrying...!\n");
    WiFi.disconnect(true); // Clear stored credentials
    delay(500);
    WiFi.begin(settings.ssid, settings.password);
  }
  
  if (WiFi.waitForConnectResult() == WL_CONNECTED) {
    wifi_signal = WiFi.RSSI(); // Store signal strength before WiFi is turned off
    Serial.println("WiFi connected at: " + WiFi.localIP().toString());
  }
  else {
    wifi_signal = 0;
    Serial.println("WiFi connection *** FAILED ***");
  }
  return WiFi.status();
}

/**
 * Disconnect from WiFi and power down WiFi module to save power.
 */
void StopWiFi() {
  WiFi.disconnect();
  WiFi.mode(WIFI_OFF);
  Serial.println("WiFi switched Off");
}

/**
 * Initialize system components: serial, display, and framebuffer.
 * Framebuffer uses 4-bit grayscale (2 pixels per byte), hence size is width*height/2.
 */
void InitialiseSystem() {
  StartTime = millis();
  Serial.begin(115200);
  while (!Serial); // Wait for serial monitor
  Serial.println(String(__FILE__) + "\nStarting...");
  
  // Initialize settings system (load from EEPROM or create defaults)
  initSettings();
  
  epd_init();
  
  // Allocate framebuffer: 4-bit grayscale = 2 pixels per byte
  framebuffer = (uint8_t *)ps_calloc(sizeof(uint8_t), EPD_WIDTH * EPD_HEIGHT / 2);
  if (!framebuffer) Serial.println("Memory alloc failed!");
  memset(framebuffer, 0xFF, EPD_WIDTH * EPD_HEIGHT / 2); // Fill with white
}

void loop() {
  // Empty - all work is done in setup() before entering deep sleep
}

/**
 * Main setup function - runs once on wake from deep sleep.
 * 
 * Flow:
 * 1. Initialize system (serial, display, framebuffer)
 * 2. Check battery voltage - if low, show warning and sleep
 * 3. Connect to WiFi
 * 4. Check RTC and wake hours - fetch weather if within wake hours or RTC not set
 * 5. Parse weather data and set RTC time from API
 * 6. Draw weather display to framebuffer and update e-paper screen
 * 7. Enter deep sleep until next wake time
 */
void setup() {
  InitialiseSystem();
  
  // Read and print status of the three buttons (GPIO 34, 35, 39)
  // Configure pins with internal pull-up resistors enabled
  pinMode(34, INPUT_PULLUP);    // Button 3
  pinMode(35, INPUT_PULLUP);    // Button 2
  pinMode(39, INPUT_PULLUP);    // Button 4
  
  Serial.println("\n=== Button Status ===");
  Serial.printf("Button 2 (GPIO 35): %s\n", digitalRead(35) == HIGH ? "HIGH" : "LOW");
  Serial.printf("Button 3 (GPIO 34): %s\n", digitalRead(34) == HIGH ? "HIGH" : "LOW");
  Serial.printf("Button 4 (GPIO 39): %s\n", digitalRead(39) == HIGH ? "HIGH" : "LOW");
  Serial.println("=== End Button Status ===\n");
  
  // Check if buttons 2 and 4 are both pressed to enter setup mode
  // With pull-up resistors, pressed buttons read LOW
  bool button2Pressed = (digitalRead(35) == LOW);
  bool button4Pressed = (digitalRead(39) == LOW);
  
  if (button2Pressed && button4Pressed) {
    // Enter setup mode
    Serial.println("Setup mode activated (Buttons 2 and 4 pressed)");
    
    epd_poweron();
    epd_clear();
    drawSetupModeScreen();
    epd_draw_grayscale_image(epd_full_screen(), framebuffer);
    epd_poweroff_all();
    
    Serial.println("Setup mode screen displayed. Starting WiFi AP and web server...");
    // Run setup mode with WiFi AP and web server
    runSetupMode();
    return; // Exit setup() early
  }
  
  // Check battery voltage first - if low, show message once and sleep
  uint32_t batVoltage = readBatteryVoltage();
  float voltage = (batVoltage > 0) ? (batVoltage / 1000.0) : 0.0;
  
  if (voltage > 0 && voltage <= 3.2) {
    // Battery is low (<= 3.2V)
    if (!lowBatteryScreenShown) {
      // Show low battery screen once
      Serial.println("Low battery detected: " + String(voltage, 2) + "V");
      Serial.println("Displaying low battery screen...");
      
      epd_poweron();
      epd_clear();
      drawLowBatteryScreen();
      epd_draw_grayscale_image(epd_full_screen(), framebuffer);
      epd_poweroff_all();
      
      // Mark that we've shown the screen
      lowBatteryScreenShown = true;
      Serial.println("Low battery screen displayed. Going to sleep...");
    } else {
      // Already shown, just sleep without redrawing
      Serial.println("Low battery screen already shown. Going to sleep...");
    }
    
    // Go to sleep - will wake up again later to check battery
    BeginSleep();
    return; // Exit setup() early
  }
  
  // Battery is OK (> 3.2V), reset the flag and proceed normally
  if (voltage > 3.2) {
    lowBatteryScreenShown = false; // Reset flag when battery is good
  }
  
  if (StartWiFi() == WL_CONNECTED) {
    // Validate and geocode location if needed (after setup mode has finished)
    // Returns: 0 = success, 1 = API key invalid (401), 2 = other error (invalid location, etc.)
    int locationResult = validateAndGeocodeLocation();
    
    if (locationResult == 1) {
      // API key is invalid - show error screen
      Serial.println("OpenWeatherMap API key is invalid");
      StopWiFi();
      epd_poweron();
      epd_clear();
      drawInvalidAPIKeyScreen();
      epd_draw_grayscale_image(epd_full_screen(), framebuffer);
      epd_poweroff_all();
      
      // Go to sleep - user needs to enter setup mode to fix API key
      Serial.println("Entering deep sleep due to invalid API key...");
      BeginSleep();
      return; // Exit setup() early
    } else if (locationResult == 2) {
      // Location is invalid and geocoding failed - show error screen
      Serial.println("Location validation failed. Displaying error screen...");
      StopWiFi();
      epd_poweron();
      epd_clear();
      drawInvalidLocationScreen();
      epd_draw_grayscale_image(epd_full_screen(), framebuffer);
      epd_poweroff_all();
      
      // Go to sleep - user needs to enter setup mode to fix location
      Serial.println("Entering deep sleep due to invalid location...");
      BeginSleep();
      return; // Exit setup() early
    }
    // locationResult == 0 means success, continue normally
    
    // Check if RTC is initialized (timestamp >= year 2000)
    time_t now = time(NULL);
    bool rtcSet = (now >= 946684800); // Unix timestamp for 2000-01-01
    
    bool WakeUp = false;
    if (rtcSet) {
      // RTC is set - check if current time is within wake hours
      UpdateLocalTime();
      
      // SleepHour == 24 means "never sleep" (always wake)
      if (settings.SleepHour == 24) {
        WakeUp = true; // Always wake regardless of hour
      } else {
        // Handle wake hours that span midnight (e.g., 22:00 to 06:00)
        if (settings.WakeupHour > settings.SleepHour) {
          // Spanning midnight: wake if hour >= WakeupHour OR hour <= SleepHour
          WakeUp = (CurrentHour >= settings.WakeupHour || CurrentHour <= settings.SleepHour);
        } else {
          // Normal range: wake if hour is between WakeupHour and SleepHour
          WakeUp = (CurrentHour >= settings.WakeupHour && CurrentHour <= settings.SleepHour);
        }
      }
      
      Serial.print("Current hour: ");
      Serial.println(CurrentHour);
      Serial.print("Wake hours: ");
      Serial.print(settings.WakeupHour);
      Serial.print(" - ");
      Serial.println(settings.SleepHour);
      Serial.print("WakeUp status: ");
      Serial.println(WakeUp ? "true" : "false");
    } else {
      // RTC not initialized - always fetch weather to set time from API
      Serial.println("RTC not set, fetching weather to set time from API...");
      WakeUp = true;
    }
    
    if (WakeUp) {
      Serial.println("Within wake hours, fetching weather...");
      byte Attempts = 1;
      int weatherResult = 2; // 0 = success, 1 = API key invalid, 2 = other error
      WiFiClient client;
      while (weatherResult != 0 && Attempts <= 2) {
        if (weatherResult != 0) {
          weatherResult = obtainWeatherData(client);
        }
        Attempts++;
      }
      Serial.println("Received weather data...");
      if (weatherResult == 0) {
        // Update time strings after RTC was set from API
        UpdateLocalTime();
        
        StopWiFi();
        epd_poweron();
        epd_clear();
        
        Serial.println("Drawing weather display...");
        DisplayWeather();
        
        // Update display - render framebuffer to screen
        Serial.println("Updating display...");
        epd_draw_grayscale_image(epd_full_screen(), framebuffer);
        epd_poweroff_all();
        Serial.println("Display updated successfully");
      } else if (weatherResult == 1) {
        // API key is invalid - show error screen
        Serial.println("OpenWeatherMap API key is invalid");
        StopWiFi();
        epd_poweron();
        epd_clear();
        drawInvalidAPIKeyScreen();
        epd_draw_grayscale_image(epd_full_screen(), framebuffer);
        epd_poweroff_all();
      } else {
        // Other error - show generic error message
        Serial.println("Failed to receive weather data");
        epd_poweron();
        epd_clear();
        drawWiFiErrorScreen(); // Reuse WiFi error screen for generic errors
        epd_draw_grayscale_image(epd_full_screen(), framebuffer);
        epd_poweroff_all();
      }
    } else {
      // Outside wake hours - skip weather fetch and display update to save power
      // Leave current display image in place and go back to sleep
      Serial.println("Outside wake hours, skipping weather fetch and display update");
      StopWiFi();
      // No EPD operations - current image remains on screen
    }
  }
  else {
    Serial.println("WiFi or time setup failed");
    Serial.println("Displaying WiFi connection error...");
    
    epd_poweron();
    epd_clear();
    drawWiFiErrorScreen();
    epd_draw_grayscale_image(epd_full_screen(), framebuffer);
    epd_poweroff_all();
  }
  BeginSleep();
}

/**
 * Main display function - renders complete weather information to framebuffer.
 * Layout includes: location/date, current conditions, 5-day forecast, 24-hour graph, and status bar.
 */
void DisplayWeather() {
  // Clear framebuffer to white
  memset(framebuffer, 0xFF, EPD_WIDTH * EPD_HEIGHT / 2);
  
  // Get current time from RTC for forecast day labels
  time_t now = time(NULL);
  struct tm *timeInfo = localtime(&now);
  if (timeInfo == NULL) {
    // Fallback if RTC not set
    struct tm defaultTime = {0};
    timeInfo = &defaultTime;
  }
  
  // Render display sections in order (back to front)
  drawLocationDate(settings.City, Date_str);                                    // Top right: city and date
  drawCurrentConditions(WxConditions, wifi_signal);                    // Center: current weather
  drawForecast(WxDailyForecast, max_daily_readings, timeInfo);         // Top row: 5-day forecast
  drawOutlookGraph(WxHourlyForecast, max_hourly_readings, timeInfo);   // Bottom: 24-hour graph
  drawStatusBar("", Time_str, wifi_signal, readBatteryVoltage());       // Bottom: status indicators
}

/**
 * Convert Unix timestamp to formatted time string.
 * Format depends on unit system (Metric = 24-hour, Imperial = 12-hour with AM/PM).
 * 
 * @param unix_time Unix timestamp
 * @return Formatted time string (e.g., "14:30" or "2:30PM")
 */
String ConvertUnixTime(int unix_time) {
  time_t tm = unix_time;
  struct tm *now_tm = localtime(&tm);
  char output[40];
  if (String(settings.Units) == "M") {
    strftime(output, sizeof(output), "%H:%M %d/%m/%y", now_tm); // 24-hour format
  }
  else {
    strftime(output, sizeof(output), "%I:%M%P %m/%d/%y", now_tm); // 12-hour with AM/PM
  }
  return output;
}

/**
 * Parse OpenWeatherMap One Call API 3.0 JSON response.
 * Extracts current weather, hourly forecasts (48h), and daily forecasts (8 days).
 * Sets RTC time from API response timestamp.
 * 
 * @param json WiFiClient stream containing JSON response
 * @return true if parsing successful, false on error
 */
bool DecodeWeather(WiFiClient& json) {
  Serial.print(F("\nDeserializing One Call API 3.0 json... "));
  auto doc = DynamicJsonDocument(64 * 1024);
  DeserializationError error = deserializeJson(doc, json);
  if (error) {
    Serial.print(F("deserializeJson() failed: "));
    Serial.println(error.c_str());
    return false;
  }
  JsonObject root = doc.as<JsonObject>();
  Serial.println(" Decoding One Call API 3.0 data");
  
  // Parse current weather conditions
  JsonObject current = root["current"];
  if (!current.isNull()) {
    int timezoneOffset = doc["timezone_offset"] | 0;
    time_t apiTime = current["dt"].as<int>(); // Unix timestamp (UTC)
    
    // Store current weather data
    WxConditions[0].FTimezone   = timezoneOffset;
    WxConditions[0].Dt         = apiTime;
    WxConditions[0].Sunrise     = current["sunrise"].as<int>();
    WxConditions[0].Sunset      = current["sunset"].as<int>();
    WxConditions[0].Temperature = current["temp"].as<float>();
    WxConditions[0].FeelsLike   = current["feels_like"].as<float>();
    WxConditions[0].Pressure    = current["pressure"].as<int>();
    WxConditions[0].Humidity     = current["humidity"].as<int>();
    WxConditions[0].Cloudcover   = current["clouds"].as<int>();
    WxConditions[0].Visibility   = current["visibility"].as<int>();
    WxConditions[0].Windspeed    = current["wind_speed"].as<float>();
    WxConditions[0].Winddir      = current["wind_deg"].as<float>();
    WxConditions[0].Forecast0    = current["weather"][0]["description"].as<const char*>();
    WxConditions[0].Icon         = current["weather"][0]["icon"].as<const char*>();
    
    // Synchronize RTC with API time
    SetRTCTimeFromAPI(apiTime, timezoneOffset);
    
    // Get today's high/low from first daily forecast entry
    JsonArray daily = root["daily"];
    if (daily.size() > 0) {
      JsonObject firstDay = daily[0];
      JsonObject temp = firstDay["temp"];
      WxConditions[0].High = temp["max"].as<float>();
      WxConditions[0].Low  = temp["min"].as<float>();
    }
  }
  
  // Parse hourly forecasts (48 hours) - used for 24-hour graph
  JsonArray hourly = root["hourly"];
  Serial.print(F("\nReceiving Hourly Forecast - "));
  for (byte r = 0; r < max_hourly_readings && r < hourly.size(); r++) {
    WxHourlyForecast[r].Dt          = hourly[r]["dt"].as<int>();
    WxHourlyForecast[r].Temperature = hourly[r]["temp"].as<float>();
    WxHourlyForecast[r].Low         = hourly[r]["temp"].as<float>(); // Hourly has single temperature value
    WxHourlyForecast[r].High        = hourly[r]["temp"].as<float>();
    WxHourlyForecast[r].Pressure    = hourly[r]["pressure"].as<float>();
    WxHourlyForecast[r].Humidity    = hourly[r]["humidity"].as<float>();
    WxHourlyForecast[r].Icon        = hourly[r]["weather"][0]["icon"].as<const char*>();
    WxHourlyForecast[r].Cloudcover  = hourly[r]["clouds"].as<int>();
    WxHourlyForecast[r].Pop         = hourly[r]["pop"].as<float>(); // Probability of precipitation (0.0-1.0)
    WxHourlyForecast[r].Windspeed   = hourly[r]["wind_speed"].as<float>();
    WxHourlyForecast[r].Winddir     = hourly[r]["wind_deg"].as<float>();
    
    // Parse optional precipitation fields (hourly API uses "1h" key)
    if (hourly[r]["rain"].isNull() || hourly[r]["rain"]["1h"].isNull()) {
      WxHourlyForecast[r].Rainfall = 0.0;
    } else {
      WxHourlyForecast[r].Rainfall = hourly[r]["rain"]["1h"].as<float>();
    }
    if (hourly[r]["snow"].isNull() || hourly[r]["snow"]["1h"].isNull()) {
      WxHourlyForecast[r].Snowfall = 0.0;
    } else {
      WxHourlyForecast[r].Snowfall = hourly[r]["snow"]["1h"].as<float>();
    }
  }
  Serial.println(String(hourly.size()) + " periods received");
  
  // Parse daily forecasts (8 days) - used for 5-day forecast display
  JsonArray daily = root["daily"];
  Serial.print(F("\nReceiving Daily Forecast - "));
  for (byte r = 0; r < max_daily_readings && r < daily.size(); r++) {
    JsonObject day = daily[r];
    JsonObject temp = day["temp"]; // Daily forecast has temp object with min/max/day/night
    
    WxDailyForecast[r].Dt          = day["dt"].as<int>();
    WxDailyForecast[r].Temperature = temp["day"].as<float>(); // Daytime average temperature
    WxDailyForecast[r].Low         = temp["min"].as<float>();
    WxDailyForecast[r].High        = temp["max"].as<float>();
    WxDailyForecast[r].Pressure    = day["pressure"].as<float>();
    WxDailyForecast[r].Humidity    = day["humidity"].as<float>();
    WxDailyForecast[r].Icon        = day["weather"][0]["icon"].as<const char*>();
    WxDailyForecast[r].Cloudcover  = day["clouds"].as<int>();
    WxDailyForecast[r].Pop         = day["pop"].as<float>(); // Probability of precipitation
    WxDailyForecast[r].Windspeed   = day["wind_speed"].as<float>();
    WxDailyForecast[r].Winddir     = day["wind_deg"].as<float>();
    WxDailyForecast[r].Sunrise     = day["sunrise"].as<int>();
    WxDailyForecast[r].Sunset      = day["sunset"].as<int>();
    
    // Parse optional precipitation (daily API provides total for the day, not per-hour)
    if (day["rain"].isNull()) {
      WxDailyForecast[r].Rainfall = 0.0;
    } else {
      WxDailyForecast[r].Rainfall = day["rain"].as<float>();
    }
    if (day["snow"].isNull()) {
      WxDailyForecast[r].Snowfall = 0.0;
    } else {
      WxDailyForecast[r].Snowfall = day["snow"].as<float>();
    }
  }
  Serial.println(String(daily.size()) + " days received");
  
  // Calculate pressure trend: compare today's pressure with tomorrow's
  // "+" = rising, "-" = falling, "=" = stable
  if (daily.size() >= 2) {
    float pressure_trend = WxDailyForecast[0].Pressure - WxDailyForecast[1].Pressure;
    pressure_trend = ((int)(pressure_trend * 10)) / 10.0; // Round to 0.1 hPa
    WxConditions[0].Trend = "=";
    if (pressure_trend > 0)  WxConditions[0].Trend = "+";
    if (pressure_trend < 0)  WxConditions[0].Trend = "-";
  }
  
  return true;
}

/**
 * Fetch weather data from OpenWeatherMap One Call API 3.0.
 * Requests current weather, hourly forecast (48h), and daily forecast (8 days).
 * Excludes minutely forecast and alerts to reduce response size.
 * 
 * @param client WiFiClient for HTTP connection
 * @return 0 if data received and parsed successfully, 1 if API key invalid (401), 2 for other errors
 */
int obtainWeatherData(WiFiClient & client) {
  const String units = (String(settings.Units) == "M" ? "metric" : "imperial");
  client.stop(); // Close any existing connection
  
  HTTPClient http;
  // Build API request URI
  String uri = "/data/3.0/onecall?lat=" + String(settings.Latitude) + "&lon=" + String(settings.Longitude) + 
               "&exclude=minutely,alerts&appid=" + String(settings.apikey) + 
               "&units=" + units + "&lang=" + String(settings.Language);
  
  const char* server = "api.openweathermap.org";
  Serial.print("Connecting: ");
  Serial.print(String(server) + uri);
  Serial.println();
  
  http.begin(client, server, 80, uri); // HTTP port 80 (use 443 + WiFiClientSecure for HTTPS)
  int httpCode = http.GET();
  
  if (httpCode == HTTP_CODE_OK) {
    if (!DecodeWeather(http.getStream())) {
      http.end();
      return 2; // Parsing error
    }
    client.stop();
    http.end();
    return 0; // Success
  }
  else {
    Serial.printf("connection failed, http error code %i %s\n", httpCode, http.errorToString(httpCode));
    client.stop();
    http.end();
    
    // Check if error is due to invalid API key (HTTP 401 Unauthorized)
    if (httpCode == 401) {
      return 1; // API key invalid
    }
    return 2; // Other error
  }
}

/**
 * Read current time from RTC and update display strings.
 * Extracts hour/minute/second for wake hours check and formats time/date for display.
 * 
 * @return true if RTC is set and time was read successfully
 */
boolean UpdateLocalTime() {
  time_t now = time(NULL);
  if (now < 946684800) { // Unix timestamp for 2000-01-01 - RTC not initialized
    Serial.println("RTC not set yet, waiting for API time...");
    return false;
  }
  
  struct tm *timeinfo = localtime(&now);
  if (timeinfo == NULL) {
    Serial.println("Failed to get time from RTC");
    return false;
  }
  
  // Extract time components for wake hours logic
  CurrentHour = timeinfo->tm_hour;
  CurrentMin  = timeinfo->tm_min;
  CurrentSec  = timeinfo->tm_sec;
  Serial.println(timeinfo, "%a %b %d %Y   %H:%M:%S");
  
  // Format date string: "Saturday, November 22" (full names, no year)
  char weekday_full[20], month_full[20], day_output[30], time_output[30], update_time[30];
  strftime(weekday_full, sizeof(weekday_full), "%A", timeinfo);
  strftime(month_full, sizeof(month_full), "%B", timeinfo);
  sprintf(day_output, "%s, %s %d", weekday_full, month_full, timeinfo->tm_mday);
  
  // Format time string based on unit system
  if (String(settings.Units) == "M") {
    strftime(update_time, sizeof(update_time), "%H:%M:%S", timeinfo); // 24-hour format
  }
  else {
    strftime(update_time, sizeof(update_time), "%r", timeinfo); // 12-hour with AM/PM
  }
  sprintf(time_output, "%s", update_time);
  
  Date_str = day_output;
  Time_str = time_output;
  return true;
}

/**
 * Read battery voltage from ADC pin using ESP32 ADC calibration.
 * LilyGo T5 4.7 uses GPIO36 (ADC1_CH0) with a voltage divider circuit.
 * Uses eFuse Vref calibration for accurate voltage readings.
 * 
 * @return Battery voltage in millivolts, or 0 if reading unavailable
 */
uint32_t readBatteryVoltage() {
  const int batteryPin = 36;  // GPIO36 = ADC1_CH0 on ESP32
  
  // Characterize ADC with calibration values
  esp_adc_cal_characteristics_t adc_chars;
  esp_adc_cal_value_t val_type = esp_adc_cal_characterize(
    ADC_UNIT_1, 
    ADC_ATTEN_DB_12, 
    ADC_WIDTH_BIT_12, 
    1100,  // Default Vref in mV
    &adc_chars
  );
  
  // Use eFuse Vref if available (more accurate than default)
  if (val_type == ESP_ADC_CAL_VAL_EFUSE_VREF) {
    Serial.printf("eFuse Vref:%u mV\n", adc_chars.vref);
    vref = adc_chars.vref;
  }
  
  // Read ADC value and convert to voltage using calibrated reference
  // Formula: voltage = (rawValue / 4096.0) * 6.566 * (vref / 1000.0)
  float voltage = analogRead(batteryPin) / 4096.0 * 6.566 * (vref / 1000.0);
  
  if (voltage <= 1.0) {
    // Voltage too low indicates battery monitoring hardware not available
    return 0;
  }
  
  // Return in millivolts for easier comparison
  return (uint32_t)(voltage * 1000);
}

