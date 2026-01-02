/**
 * Settings Management
 * 
 * Handles loading and saving user settings to/from EEPROM using ESP32 Preferences.
 * On first boot, initializes with default values.
 */

#include <Arduino.h>
#include <Preferences.h>
#include "settings.h"

// Preferences namespace for settings storage
Preferences preferences;

// Global settings variable
Settings settings;

// Default settings
// Note: Using regular initialization instead of C99 designators for C++ compatibility
const Settings defaultSettings = {
  "SSID",                       // ssid
  "Password",                   // password
  "APIKEY",                     // OpenWeatherMap apikey
  "Chicago,IL,US",              // Location String
  "41.8832",                    // Latitude
  "87.6324",                    // Longitude
  "en",                         // Language
  "M",                          // Units
  60,                           // SleepDuration
  0,                            // WakeupHour
  24,                           // SleepHour
  SETTINGS_MAGIC                // magic
};

/**
 * Initialize settings system.
 * Checks if settings exist in EEPROM, if not creates with defaults.
 */
void initSettings() {
  preferences.begin("weather", false); // Open preferences namespace "weather" in read-write mode
  
  // Check if settings exist by reading magic number
  uint32_t magic = preferences.getUInt("magic", 0);
  
  if (magic != SETTINGS_MAGIC) {
    // Settings don't exist or are invalid - initialize with defaults
#if DEBUG_LEVEL
    if (Serial) {
      Serial.println("Settings not found in EEPROM. Initializing with defaults...");
    }
#endif
    resetSettingsToDefaults();
  } else {
    // Settings exist - load them
#if DEBUG_LEVEL
    if (Serial) {
      Serial.println("Loading settings from EEPROM...");
    }
#endif
    loadSettings();
  }
  
  preferences.end();
}

/**
 * Load settings from EEPROM.
 * @return true if settings were loaded successfully
 */
bool loadSettings() {
  preferences.begin("weather", true); // Open in read-only mode
  
  // Load all settings (use default if key doesn't exist)
  String ssidStr = preferences.getString("ssid", defaultSettings.ssid);
  String passwordStr = preferences.getString("password", defaultSettings.password);
  String apikeyStr = preferences.getString("apikey", defaultSettings.apikey);
  String cityStr = preferences.getString("City", defaultSettings.City);
  String latStr = preferences.getString("Latitude", defaultSettings.Latitude);
  String lonStr = preferences.getString("Longitude", defaultSettings.Longitude);
  String langStr = preferences.getString("Language", defaultSettings.Language);
  String unitsStr = preferences.getString("Units", defaultSettings.Units);
  
  // Copy strings to settings structure
  strncpy(settings.ssid, ssidStr.c_str(), sizeof(settings.ssid) - 1);
  settings.ssid[sizeof(settings.ssid) - 1] = '\0';
  strncpy(settings.password, passwordStr.c_str(), sizeof(settings.password) - 1);
  settings.password[sizeof(settings.password) - 1] = '\0';
  strncpy(settings.apikey, apikeyStr.c_str(), sizeof(settings.apikey) - 1);
  settings.apikey[sizeof(settings.apikey) - 1] = '\0';
  strncpy(settings.City, cityStr.c_str(), sizeof(settings.City) - 1);
  settings.City[sizeof(settings.City) - 1] = '\0';
  strncpy(settings.Latitude, latStr.c_str(), sizeof(settings.Latitude) - 1);
  settings.Latitude[sizeof(settings.Latitude) - 1] = '\0';
  strncpy(settings.Longitude, lonStr.c_str(), sizeof(settings.Longitude) - 1);
  settings.Longitude[sizeof(settings.Longitude) - 1] = '\0';
  strncpy(settings.Language, langStr.c_str(), sizeof(settings.Language) - 1);
  settings.Language[sizeof(settings.Language) - 1] = '\0';
  strncpy(settings.Units, unitsStr.c_str(), sizeof(settings.Units) - 1);
  settings.Units[sizeof(settings.Units) - 1] = '\0';
  
  settings.SleepDuration = preferences.getLong("SleepDuration", defaultSettings.SleepDuration);
  settings.WakeupHour = preferences.getInt("WakeupHour", defaultSettings.WakeupHour);
  settings.SleepHour = preferences.getInt("SleepHour", defaultSettings.SleepHour);
  settings.magic = preferences.getUInt("magic", SETTINGS_MAGIC);
  
  preferences.end();
  
#if DEBUG_LEVEL
  if (Serial) {
    Serial.println("Settings loaded from EEPROM");
  }
#endif
  return true;
}

/**
 * Save current settings to EEPROM.
 */
void saveSettings() {
  preferences.begin("weather", false); // Open in read-write mode
  
  // Save all settings
  preferences.putString("ssid", settings.ssid);
  preferences.putString("password", settings.password);
  preferences.putString("apikey", settings.apikey);
  preferences.putString("City", settings.City);
  preferences.putString("Latitude", settings.Latitude);
  preferences.putString("Longitude", settings.Longitude);
  preferences.putString("Language", settings.Language);
  preferences.putString("Units", settings.Units);
  preferences.putLong("SleepDuration", settings.SleepDuration);
  preferences.putInt("WakeupHour", settings.WakeupHour);
  preferences.putInt("SleepHour", settings.SleepHour);
  preferences.putUInt("magic", SETTINGS_MAGIC);
  
  preferences.end();
  
#if DEBUG_LEVEL
  if (Serial) {
    Serial.println("Settings saved to EEPROM");
  }
#endif
}

/**
 * Reset settings to defaults and save to EEPROM.
 */
void resetSettingsToDefaults() {
  // Copy defaults to current settings
  memcpy(&settings, &defaultSettings, sizeof(Settings));
  
  // Save to EEPROM
  saveSettings();
  
#if DEBUG_LEVEL
  if (Serial) {
    Serial.println("Settings reset to defaults and saved");
  }
#endif
}


