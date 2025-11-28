#ifndef __SETUP_MODE_H__
#define __SETUP_MODE_H__

#include <Arduino.h>
#include <WiFi.h>

/**
 * Run setup mode with WiFi Access Point and web server.
 * Creates an AP with SSID "ESP Weather" (no password) and serves a configuration page.
 * Based on: https://randomnerdtutorials.com/esp32-access-point-ap-web-server/
 * 
 * This function runs in an infinite loop, handling web server requests until the device is reset.
 */
void runSetupMode();

/**
 * Validate and geocode location if latitude/longitude are invalid.
 * Checks if lat/lon are -181 (invalid). If so, uses OpenWeatherMap Geocoding API
 * to get valid coordinates from the location string and saves them to EEPROM.
 * 
 * @return 0 if location is valid or successfully geocoded, 1 if API key is invalid (401), 2 if other error (invalid location, etc.)
 */
int validateAndGeocodeLocation();

#endif // __SETUP_MODE_H__


