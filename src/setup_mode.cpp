/**
 * Setup Mode Implementation
 * 
 * Provides WiFi Access Point and web server for device configuration.
 * Based on: https://randomnerdtutorials.com/esp32-access-point-ap-web-server/
 */

#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "setup_mode.h"
#include "settings.h"

/**
 * Validate and geocode location if latitude/longitude are invalid.
 * Checks if lat/lon are -181 (invalid). If so, uses OpenWeatherMap Geocoding API
 * to get valid coordinates from the location string and saves them to EEPROM.
 * 
 * @return true if location is valid or successfully geocoded, false if geocoding fails
 */
int validateAndGeocodeLocation() {
  // Check if latitude and longitude are invalid (-181)
  float lat = String(settings.Latitude).toFloat();
  float lon = String(settings.Longitude).toFloat();
  
  // If coordinates are valid (not -181), no geocoding needed
  if (lat > -180.0 && lat < 180.0 && lon > -180.0 && lon < 180.0) {
#if DEBUG_LEVEL
    if (Serial) {
      Serial.println("Location coordinates are valid. No geocoding needed.");
    }
#endif
    return 0; // Success
  }
  
#if DEBUG_LEVEL
  if (Serial) {
    Serial.println("Location coordinates are invalid. Attempting geocoding...");
  }
#endif
  
  // Check if location string is empty
  if (strlen(settings.City) == 0) {
#if DEBUG_LEVEL
    if (Serial) {
      Serial.println("Error: Location string is empty. Cannot geocode.");
    }
#endif
    return 2; // Other error (invalid location)
  }
  
  // Check if API key is set
  if (strlen(settings.apikey) == 0) {
#if DEBUG_LEVEL
    if (Serial) {
      Serial.println("Error: API key is not set. Cannot geocode.");
    }
#endif
    return 2; // Other error (missing API key, but not invalid API key)
  }
  
  // Connect to WiFi if not already connected
  if (WiFi.status() != WL_CONNECTED) {
    // Check if WiFi SSID is set
    if (strlen(settings.ssid) == 0) {
#if DEBUG_LEVEL
      if (Serial) {
        Serial.println("Error: WiFi SSID is not set. Cannot connect for geocoding.");
      }
#endif
      return false;
    }
    
#if DEBUG_LEVEL
    if (Serial) {
      Serial.print("Connecting to WiFi: ");
      Serial.println(settings.ssid);
    }
#endif
    
    WiFi.mode(WIFI_STA);
    WiFi.begin(settings.ssid, settings.password);
    
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
      delay(500);
#if DEBUG_LEVEL
      if (Serial) {
        Serial.print(".");
      }
#endif
      attempts++;
    }
#if DEBUG_LEVEL
    if (Serial) {
      Serial.println();
    }
#endif
    
    if (WiFi.status() != WL_CONNECTED) {
#if DEBUG_LEVEL
      if (Serial) {
        Serial.println("Error: Failed to connect to WiFi for geocoding.");
      }
#endif
      return 2; // Other error
    }
    
#if DEBUG_LEVEL
    if (Serial) {
      Serial.print("WiFi connected. IP address: ");
      Serial.println(WiFi.localIP());
    }
#endif
  }
  
  // Build geocoding API request
  // API endpoint: http://api.openweathermap.org/geo/1.0/direct?q={city name}&limit=1&appid={API key}
  WiFiClient client;
  HTTPClient http;
  
  String locationQuery = String(settings.City);
  locationQuery.replace(" ", "+"); // URL encode spaces
  locationQuery.replace(",", "%2C"); // URL encode commas
  
  String uri = "/geo/1.0/direct?q=" + locationQuery + "&limit=1&appid=" + String(settings.apikey);
  
#if DEBUG_LEVEL
  if (Serial) {
    Serial.print("Geocoding API request: ");
    Serial.print("api.openweathermap.org");
    Serial.println(uri);
  }
#endif
  
  http.begin(client, "api.openweathermap.org", 80, uri);
  int httpCode = http.GET();
  
  if (httpCode == HTTP_CODE_UNAUTHORIZED || httpCode == 401) {
    // API key is invalid
#if DEBUG_LEVEL
    if (Serial) {
      Serial.print("Geocoding API error: Invalid API key. HTTP code: ");
      Serial.println(httpCode);
    }
#endif
    http.end();
    return 1; // API key invalid
  }
  
  if (httpCode != HTTP_CODE_OK) {
#if DEBUG_LEVEL
    if (Serial) {
      Serial.print("Geocoding API error. HTTP code: ");
      Serial.println(httpCode);
    }
#endif
    http.end();
    return 2; // Other error
  }
  
  // Parse JSON response
  // Expected format: [{"name":"...","lat":51.5085,"lon":-0.1257,"country":"GB",...}]
  auto doc = DynamicJsonDocument(8192);
  DeserializationError error = deserializeJson(doc, http.getStream());
  http.end();
  
  if (error) {
#if DEBUG_LEVEL
    if (Serial) {
      Serial.print("Failed to parse geocoding JSON: ");
      Serial.println(error.c_str());
    }
#endif
    return 2; // Other error
  }
  
  // Check if response is an array with at least one result
  if (!doc.is<JsonArray>() || doc.size() == 0) {
#if DEBUG_LEVEL
    if (Serial) {
      Serial.println("Geocoding API returned no results.");
    }
#endif
    return 2; // Other error (invalid location)
  }
  
  // Get first result
  JsonObject firstResult = doc[0];
  if (!firstResult.containsKey("lat") || !firstResult.containsKey("lon")) {
#if DEBUG_LEVEL
    if (Serial) {
      Serial.println("Geocoding API response missing lat/lon fields.");
    }
#endif
    return 2; // Other error
  }
  
  float geocodedLat = firstResult["lat"].as<float>();
  float geocodedLon = firstResult["lon"].as<float>();
  
  // Validate geocoded coordinates
  if (geocodedLat < -90.0 || geocodedLat > 90.0 || 
      geocodedLon < -180.0 || geocodedLon > 180.0) {
#if DEBUG_LEVEL
    if (Serial) {
      Serial.println("Geocoding API returned invalid coordinates.");
    }
#endif
    return 2; // Other error
  }
  
  // Convert to strings and save to settings
  String latStr = String(geocodedLat, 6); // 6 decimal places for precision
  String lonStr = String(geocodedLon, 6);
  
  strncpy(settings.Latitude, latStr.c_str(), sizeof(settings.Latitude) - 1);
  settings.Latitude[sizeof(settings.Latitude) - 1] = '\0';
  strncpy(settings.Longitude, lonStr.c_str(), sizeof(settings.Longitude) - 1);
  settings.Longitude[sizeof(settings.Longitude) - 1] = '\0';
  
  // Save to EEPROM
  saveSettings();
  
#if DEBUG_LEVEL
  if (Serial) {
    Serial.print("Geocoding successful. Coordinates: ");
    Serial.print(settings.Latitude);
    Serial.print(", ");
    Serial.println(settings.Longitude);
  }
#endif
  
  return 0; // Success
}

/**
 * Run setup mode with WiFi Access Point and web server.
 * Creates an AP with SSID "ESP Weather" (no password) and serves a configuration page.
 * Based on: https://randomnerdtutorials.com/esp32-access-point-ap-web-server/
 */
void runSetupMode() {
  // Set up WiFi Access Point
  const char* ap_ssid = "ESP Weather Station";
  const char* ap_password = "";  // No password
  
#if DEBUG_LEVEL
  if (Serial) {
    Serial.print("Setting AP (Access Point)…");
  }
#endif
  WiFi.softAP(ap_ssid, ap_password);
  
  IPAddress IP = WiFi.softAPIP();
#if DEBUG_LEVEL
  if (Serial) {
    Serial.print("AP IP address: ");
    Serial.println(IP);
    Serial.println("Web server started. Connect to WiFi 'ESP Weather Station' and navigate to http://192.168.4.1");
  }
#endif
  
  // Start web server on port 80
  WiFiServer server(80);
  server.begin();
  
  // Main loop to handle web server requests
  while (true) {
    WiFiClient client = server.available();   // Listen for incoming clients
    
    if (client) {                             // If a new client connects,
#if DEBUG_LEVEL
      if (Serial) {
        Serial.println("New Client.");          // print a message out in the serial port
      }
#endif
      String currentLine = "";                // make a String to hold incoming data from the client
      String header = "";                     // Variable to store the HTTP request
      
      while (client.connected()) {            // loop while the client's connected
        if (client.available()) {             // if there's bytes to read from the client,
          char c = client.read();             // read a byte, then
#if DEBUG_LEVEL
          if (Serial && Serial.availableForWrite() > 0) {
            Serial.write(c);                    // print it out the serial monitor
          }
#endif
          header += c;
          if (c == '\n') {                    // if the byte is a newline character
            // if the current line is blank, you got two newline characters in a row.
            // that's the end of the client HTTP request, so send a response:
            if (currentLine.length() == 0) {
              // HTTP headers always start with a response code (e.g. HTTP/1.1 200 OK)
              // and a content-type so the client knows what's coming, then a blank line:
              client.println("HTTP/1.1 200 OK");
              client.println("Content-type:text/html");
              client.println("Connection: close");
              client.println();
              
              // Check if the "Reboot without Saving" button was pressed
              bool isRebootRequest = (header.indexOf("GET /reboot") >= 0);
              
              if (isRebootRequest) {
                // Reboot without saving
#if DEBUG_LEVEL
                if (Serial) {
                  Serial.println("\n=== Reboot Requested (No Save) ===");
                  Serial.println("Rebooting without saving settings...");
                }
#endif
                
                // Send response before rebooting
                client.println("<!DOCTYPE html><html>");
                client.println("<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
                client.println("<title>ESP Weather Setup</title>");
                client.println("<body><h1>Rebooting...</h1>");
                client.println("<p>ESP32 will reboot now without saving changes.</p>");
                client.println("</body></html>");
                client.println();
                client.stop();
                
                // Reset the ESP32
                delay(500);
#if DEBUG_LEVEL
                if (Serial) {
                  Serial.println("Rebooting ESP32...");
                }
#endif
                ESP.restart();
              }
              
              // Check if the "Save and Reboot" button was pressed
              bool isSaveRequest = (header.indexOf("GET /save") >= 0 || header.indexOf("/save?") >= 0);
              
              if (isSaveRequest) {
                // Helper function to extract URL parameter value from header
                auto extractParam = [](const String& header, const String& paramName) -> String {
                  String searchStr = paramName + "=";
                  int start = header.indexOf(searchStr);
                  if (start < 0) return "";
                  start += searchStr.length();
                  int end = start;
                  while (end < header.length() && 
                         header.charAt(end) != ' ' && 
                         header.charAt(end) != '&' && 
                         header.charAt(end) != '\n' &&
                         header.charAt(end) != '\r') {
                    end++;
                  }
                  String value = header.substring(start, end);
                  // Basic URL decoding
                  value.replace("+", " ");
                  value.replace("%20", " ");
                  value.replace("%2B", "+");
                  value.replace("%2C", ",");
                  value.replace("%2F", "/");
                  return value;
                };
                
                // Extract all form parameters
                String apiKey = extractParam(header, "apikey");
                String ssid = extractParam(header, "ssid");
                String password = extractParam(header, "password");
                String location = extractParam(header, "location");
                String units = extractParam(header, "units");
                String frequency = extractParam(header, "frequency");
                String startHour = extractParam(header, "startHour");
                String stopHour = extractParam(header, "stopHour");
                
                // Print all received values to serial
#if DEBUG_LEVEL
                if (Serial) {
                  Serial.println("\n=== Configuration Received ===");
                  Serial.print("OpenWeatherMap API Key: ");
                  Serial.println(apiKey.length() > 0 ? apiKey : "(empty)");
                  Serial.print("WiFi SSID: ");
                  Serial.println(ssid.length() > 0 ? ssid : "(empty)");
                  Serial.print("WiFi Password: ");
                  Serial.println(password.length() > 0 ? "***" : "(empty)");
                  Serial.print("Location: ");
                  Serial.println(location.length() > 0 ? location : "(empty)");
                  Serial.print("Units: ");
                  Serial.println(units.length() > 0 ? units : "(empty)");
                  Serial.print("Update Frequency (minutes): ");
                  Serial.println(frequency.length() > 0 ? frequency : "(empty)");
                  Serial.print("Start Updating Hour: ");
                  Serial.println(startHour.length() > 0 ? startHour : "(empty)");
                  Serial.print("Stop Updating Hour: ");
                  Serial.println(stopHour.length() > 0 ? stopHour : "(empty)");
                  Serial.println("=== End Configuration ===\n");
                  
                  // Validate and save settings
                  Serial.println("Validating and saving settings...");
                }
#endif
                bool validationError = false;
                String errorMessage = "";
                
                // Validate API Key (max 63 chars, allow empty but warn)
                if (apiKey.length() > 63) {
                  validationError = true;
                  errorMessage += "API Key too long (max 63 characters). ";
                }
                
                // Validate WiFi SSID (max 63 chars, allow empty but warn)
                if (ssid.length() > 63) {
                  validationError = true;
                  errorMessage += "WiFi SSID too long (max 63 characters). ";
                }
                
                // Validate WiFi Password (max 63 chars)
                if (password.length() > 63) {
                  validationError = true;
                  errorMessage += "WiFi Password too long (max 63 characters). ";
                }
                
                // Validate Location/City (max 127 chars)
                if (location.length() > 127) {
                  validationError = true;
                  errorMessage += "Location too long (max 127 characters). ";
                }
                
                // Validate Units (must be "I" or "M")
                if (units.length() > 0 && units != "I" && units != "M") {
                  validationError = true;
                  errorMessage += "Invalid Units value (must be 'I' or 'M'). ";
                }
                
                // Validate Update Frequency (must be a positive integer between 1 and 1439)
                long sleepDuration = settings.SleepDuration; // Default to current value
                if (frequency.length() > 0) {
                  sleepDuration = frequency.toInt();
                  if (sleepDuration <= 0 || sleepDuration >= 1440) {
                    // Invalid value - set to default of 60 minutes
                    sleepDuration = 60;
#if DEBUG_LEVEL
                    if (Serial) {
                      Serial.println("Warning: Update Frequency out of range. Setting to default value of 60 minutes.");
                    }
#endif
                  }
                } else {
                  // If no value provided, validate current value
                  if (sleepDuration <= 0 || sleepDuration >= 1440) {
                    sleepDuration = 60;
#if DEBUG_LEVEL
                    if (Serial) {
                      Serial.println("Warning: Current Update Frequency is invalid. Setting to default value of 60 minutes.");
                    }
#endif
                  }
                }
                
                // Validate Start Hour (must be "none" or 0-23)
                int wakeupHour = settings.WakeupHour; // Default to current value
                if (startHour.length() > 0) {
                  if (startHour == "none") {
                    wakeupHour = 0;
                  } else {
                    int hour = startHour.toInt();
                    if (hour < 0 || hour > 23) {
                      validationError = true;
                      errorMessage += "Invalid Start Hour (must be 0-23 or 'none'). ";
                    } else {
                      wakeupHour = hour;
                    }
                  }
                }
                
                // Validate Stop Hour (must be "none" or 1-23)
                // "none" maps to 24, otherwise 1 AM = 1, 2 AM = 2, ..., 11 PM = 23
                int sleepHour = settings.SleepHour; // Default to current value
                if (stopHour.length() > 0) {
                  if (stopHour == "none") {
                    sleepHour = 24; // "12 AM/None" maps to 24
                  } else {
                    int hour = stopHour.toInt();
                    if (hour < 1 || hour > 23) {
                      validationError = true;
                      errorMessage += "Invalid Stop Hour (must be 1-23 or 'none'). ";
                    } else {
                      sleepHour = hour;
                    }
                  }
                }
                
                // Validate that startHour is less than stopHour
                // If not, set stopHour equal to startHour
                // Note: If stopHour is 24 (never sleep), skip this check as 24 is a special value
                if (!validationError && sleepHour != 24) {
                  if (wakeupHour >= sleepHour) {
                    // Start hour is not less than stop hour - set stopHour equal to startHour
                    sleepHour = wakeupHour;
#if DEBUG_LEVEL
                    if (Serial) {
                      Serial.print("Warning: Start Hour (");
                      Serial.print(wakeupHour);
                      Serial.print(") is not less than Stop Hour. Setting Stop Hour to ");
                      Serial.println(sleepHour);
                    }
#endif
                  }
                }
                
                // If validation failed, send error response
                if (validationError) {
#if DEBUG_LEVEL
                  if (Serial) {
                    Serial.print("Validation error: ");
                    Serial.println(errorMessage);
                  }
#endif
                  client.println("<!DOCTYPE html><html>");
                  client.println("<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
                  client.println("<title>ESP Weather Setup - Error</title>");
                  client.println("<body><h1>Validation Error</h1>");
                  client.print("<p style=\"color: red;\">");
                  client.print(errorMessage);
                  client.println("</p>");
                  client.println("<p><a href=\"/\">Go back to form</a></p>");
                  client.println("</body></html>");
                  client.println();
                  client.stop();
                  return; // Don't reboot on validation error
                }
                
                // All validation passed - save to settings structure
#if DEBUG_LEVEL
                if (Serial) {
                  Serial.println("Validation passed. Saving to EEPROM...");
                }
#endif
                
                // Copy validated strings to settings (with length limits)
                strncpy(settings.apikey, apiKey.c_str(), sizeof(settings.apikey) - 1);
                settings.apikey[sizeof(settings.apikey) - 1] = '\0';
                
                strncpy(settings.ssid, ssid.c_str(), sizeof(settings.ssid) - 1);
                settings.ssid[sizeof(settings.ssid) - 1] = '\0';
                
                strncpy(settings.password, password.c_str(), sizeof(settings.password) - 1);
                settings.password[sizeof(settings.password) - 1] = '\0';
                
                strncpy(settings.City, location.c_str(), sizeof(settings.City) - 1);
                settings.City[sizeof(settings.City) - 1] = '\0';
                
                // Set Units (use validated value or keep current)
                if (units.length() > 0 && (units == "I" || units == "M")) {
                  settings.Units[0] = units.charAt(0);
                }
                
                // Set numeric values
                settings.SleepDuration = sleepDuration;
                settings.WakeupHour = wakeupHour;
                settings.SleepHour = sleepHour;
                
                // Set Latitude and Longitude to invalid value (-181)
                strncpy(settings.Latitude, "-181", sizeof(settings.Latitude) - 1);
                settings.Latitude[sizeof(settings.Latitude) - 1] = '\0';
                strncpy(settings.Longitude, "-181", sizeof(settings.Longitude) - 1);
                settings.Longitude[sizeof(settings.Longitude) - 1] = '\0';
                
                // Save settings to EEPROM
                saveSettings();
#if DEBUG_LEVEL
                if (Serial) {
                  Serial.println("Settings saved to EEPROM successfully.");
                }
#endif
                
                // Send success response before resetting
                client.println("<!DOCTYPE html><html>");
                client.println("<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
                client.println("<title>ESP Weather Setup</title>");
                client.println("<body><h1>Configuration Saved!</h1>");
                client.println("<p>Settings have been saved to EEPROM.</p>");
                client.println("<p>ESP32 will reboot now...</p>");
                client.println("</body></html>");
                client.println();
                client.stop();
                
                // Reset the ESP32
                delay(500);
#if DEBUG_LEVEL
                if (Serial) {
                  Serial.println("Rebooting ESP32...");
                }
#endif
                ESP.restart();
              } else {
                // Display the HTML web page (normal form)
                client.println("<!DOCTYPE html><html>");
                client.println("<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
                client.println("<title>ESP Weather Setup</title>");
                client.println("<style>");
                client.println("html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center; padding: 20px;}");
                client.println("h1 { color: #0F3376; }");
                client.println(".input-group { margin: 20px 0; }");
                client.println("label { display: block; margin-bottom: 5px; font-weight: bold; }");
                client.println("input[type=\"text\"], select { width: 300px; padding: 10px; font-size: 16px; border: 1px solid #ccc; border-radius: 4px; }");
                client.println(".help-text { font-style: italic; font-size: 12px; color: #666; margin-top: 5px; }");
                client.println(".button { background-color: #4CAF50; border: none; color: white; padding: 16px 40px;");
                client.println("text-decoration: none; font-size: 20px; margin: 20px 10px; cursor: pointer; border-radius: 4px; }");
                client.println(".button:hover { background-color: #45a049; }");
                client.println("</style></head>");
                
                // Web Page Heading
                client.println("<body><h1>ESP Weather Setup</h1>");
                
                // Form with all configuration fields (pre-populated with current settings)
                client.println("<form action=\"/save\" method=\"GET\">");
                
                // Helper function to escape HTML special characters
                auto escapeHtml = [](const String& str) -> String {
                  String escaped = str;
                  escaped.replace("&", "&amp;");
                  escaped.replace("<", "&lt;");
                  escaped.replace(">", "&gt;");
                  escaped.replace("\"", "&quot;");
                  escaped.replace("'", "&#39;");
                  return escaped;
                };
                
                // OpenWeatherMap API Key
                client.println("<div class=\"input-group\">");
                client.println("<label for=\"apikey\">OpenWeatherMap API Key:</label>");
                client.print("<input type=\"text\" id=\"apikey\" name=\"apikey\" value=\"");
                client.print(escapeHtml(String(settings.apikey)));
                client.println("\" placeholder=\"Enter your API key\">");
                client.println("</div>");
                
                // WiFi SSID
                client.println("<div class=\"input-group\">");
                client.println("<label for=\"ssid\">Wifi SSID:</label>");
                client.print("<input type=\"text\" id=\"ssid\" name=\"ssid\" value=\"");
                client.print(escapeHtml(String(settings.ssid)));
                client.println("\" placeholder=\"Enter WiFi network name\">");
                client.println("</div>");
                
                // WiFi Password
                client.println("<div class=\"input-group\">");
                client.println("<label for=\"password\">Wifi Password:</label>");
                client.print("<input type=\"text\" id=\"password\" name=\"password\" value=\"");
                client.print(escapeHtml(String(settings.password)));
                client.println("\" placeholder=\"Enter WiFi password\">");
                client.println("</div>");
                
                // Location
                client.println("<div class=\"input-group\">");
                client.println("<label for=\"location\">Location:</label>");
                client.print("<input type=\"text\" id=\"location\" name=\"location\" value=\"");
                client.print(escapeHtml(String(settings.City)));
                client.println("\" placeholder=\"Chicago, IL, US\">");
                client.println("<div class=\"help-text\">In the format Town/City, State/Province, Country; example 'Chicago, IL, US'</div>");
                client.println("</div>");
                
                // Units dropdown
                client.println("<div class=\"input-group\">");
                client.println("<label for=\"units\">Units:</label>");
                client.println("<select id=\"units\" name=\"units\">");
                if (String(settings.Units) == "I") {
                  client.println("<option value=\"I\" selected>Imperial</option>");
                  client.println("<option value=\"M\">Metric</option>");
                } else {
                  client.println("<option value=\"I\">Imperial</option>");
                  client.println("<option value=\"M\" selected>Metric</option>");
                }
                client.println("</select>");
                client.println("</div>");
                
                // Update Frequency
                client.println("<div class=\"input-group\">");
                client.println("<label for=\"frequency\">Update Frequency (minutes):</label>");
                client.print("<input type=\"text\" id=\"frequency\" name=\"frequency\" value=\"");
                client.print(settings.SleepDuration);
                client.println("\" placeholder=\"60\">");
                client.println("</div>");
                
                // Start Updating Hour dropdown
                client.println("<div class=\"input-group\">");
                client.println("<label for=\"startHour\">Start Updating Hour:</label>");
                client.println("<select id=\"startHour\" name=\"startHour\">");
                int startHour = settings.WakeupHour;
                for (int hour = 0; hour <= 23; hour++) {
                  client.print("<option value=\"");
                  if (hour == 0) {
                    client.print("none");
                  } else {
                    client.print(hour);
                  }
                  client.print("\"");
                  if ((hour == 0 && startHour == 0) || (hour == startHour && startHour != 0)) {
                    client.print(" selected");
                  }
                  client.print(">");
                  if (hour == 0) {
                    client.print("12 AM/None");
                  } else if (hour < 12) {
                    client.print(hour);
                    client.print(" AM");
                  } else if (hour == 12) {
                    client.print("12 PM");
                  } else {
                    client.print(hour - 12);
                    client.print(" PM");
                  }
                  client.println("</option>");
                }
                client.println("</select>");
                client.println("</div>");
                
                // Stop Updating Hour dropdown
                client.println("<div class=\"input-group\">");
                client.println("<label for=\"stopHour\">Stop Updating Hour:</label>");
                client.println("<select id=\"stopHour\" name=\"stopHour\">");
                int stopHour = settings.SleepHour;
                
                // First option: "12 AM/None" with value "none"
                // Maps to sleepHour value of 24
                client.print("<option value=\"none\"");
                if (stopHour == 24) {  // "12 AM/None" maps to 24
                  client.print(" selected");
                }
                client.println(">12 AM/None</option>");
                
                // Generate options from 1 AM to 11 PM in forward order
                for (int hour = 1; hour <= 23; hour++) {
                  client.print("<option value=\"");
                  client.print(hour);
                  client.print("\"");
                  if (hour == stopHour) {
                    client.print(" selected");
                  }
                  client.print(">");
                  if (hour < 12) {
                    client.print(hour);
                    client.print(" AM");
                  } else if (hour == 12) {
                    client.print("12 PM");
                  } else {
                    client.print(hour - 12);
                    client.print(" PM");
                  }
                  client.println("</option>");
                }
                client.println("</select>");
                client.println("</div>");
                
                // Save and Reboot button (at the bottom)
                client.println("<button type=\"submit\" class=\"button\">Save and Reboot</button>");
                client.println("</form>");
                
                // Reboot without Saving button (separate form/link)
                client.println("<form action=\"/reboot\" method=\"GET\" style=\"margin-top: 20px;\">");
                client.println("<button type=\"submit\" class=\"button\" style=\"background-color: #f44336;\">Reboot without Saving</button>");
                client.println("</form>");
                
                client.println("</body></html>");
                client.println();
              }
              
              // Break out of the while loop
              break;
            } else { // if you got a newline, then clear currentLine
              currentLine = "";
            }
          } else if (c != '\r') {  // if you got anything else but a carriage return character,
            currentLine += c;      // add it to the end of the currentLine
          }
        }  // End of if (client.available())
      }  // End of while (client.connected())
      // Clear the header variable
      header = "";
      // Close the connection
      client.stop();
#if DEBUG_LEVEL
      if (Serial) {
        Serial.println("Client disconnected.");
        Serial.println("");
      }
#endif
    }
    delay(10); // Small delay to prevent watchdog issues
  }
}

