#ifndef __SETTINGS_H__
#define __SETTINGS_H__

#include <Arduino.h>

/**
 * Settings structure to hold all user-configurable values.
 * Stored in EEPROM (using ESP32 Preferences) for persistence.
 */
struct Settings {
  // WiFi credentials
  char ssid[64];
  char password[64];
  
  // OpenWeatherMap API
  char apikey[64];
  
  // Location
  char City[128];
  char Latitude[16];
  char Longitude[16];
  char Language[8];
  char Units[2];
  
  // Power management
  long SleepDuration;  // Sleep duration in minutes
  int WakeupHour;     // Start of wake hours (0-23)
  int SleepHour;       // End of wake hours (1-23, or 24 for "never sleep"/always wake)
  
  // Magic number to verify EEPROM data is valid
  uint32_t magic;
};

// Magic number to identify valid settings in EEPROM
#define SETTINGS_MAGIC 0x57454154  // "WEAT" in hex

// Default settings (used on first boot)
extern const Settings defaultSettings;

// Global settings variable (loaded from EEPROM on boot)
extern Settings settings;

/**
 * Initialize settings system.
 * Checks if settings exist in EEPROM, if not creates with defaults.
 * Must be called before using any settings values.
 */
void initSettings();

/**
 * Load settings from EEPROM.
 * @return true if settings were loaded successfully, false if EEPROM was empty/invalid
 */
bool loadSettings();

/**
 * Save current settings to EEPROM.
 */
void saveSettings();

/**
 * Reset settings to defaults and save to EEPROM.
 */
void resetSettingsToDefaults();

#endif // __SETTINGS_H__


