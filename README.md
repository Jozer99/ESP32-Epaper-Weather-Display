<h1>Introduction</h1>
This project began with the goal to create a user friendly weather display using the Lilygo T5 EPD ESP32 development platform.  Several existing projects go partway to addressing this goal, and I borrowed liberally from them, in particular:

https://github.com/CybDis/Lilygo-T5-4.7-WeatherStation-with-HomeAssistant
Served as the basis of the code upon which I built.  

https://github.com/G6EJD/LilyGo-EPD-47-OWM-Weather-Display
Which seems no longer to exist, was the first developer of a weather display for this platform.

https://github.com/lmarzen/esp32-weather-epd
Served as aesethic inspiration, although I do not believe I borrowed a single line of code from this project, it is worthy of mention.  

My code is user friendly in two ways:
1.  The code can be flashed unmodified to the ESP32 platform.  User settings can be changed once the code is uploaded, rather than requiring modification of the code in the IDE.
2.  I personally find the simplified display of my project provides just the right amount of information about the current and upcoming weather, rather than too much or two little.

<h1>Installation</h1>
This code can be flashed to the the Lilygo module without modification.  It should be flashed using PlatformIO or compatible platform (such as Visual Studio Code with the PlatformIO plugin).  The included configuration will means that all that is required is to load up the code in your development environment, plug in the Lilygo board by USB, and press the "Upload" button.  

<h1>Use</h1>
Once installed, the weather display must be configured.  This is done by putting the module into "setup mode" by pressing a special combination of buttons.  I recommend doing this while the module is fully charged (if it has a battery installed) or while it is plugged into USB power, as setup mode has considerable power draw due to wifi.  

To enter setup mode, hold buttons 3 and 5 while tapping button 1 briefly.  After a few seconds, the unit will boot into setup mode and display directions on screen.  All buttons can be released at this point.  
<img width="1187" height="1000" alt="Buttons" src="https://github.com/user-attachments/assets/f9a20d47-d0b0-4860-8013-18b4a0e16261" />

During setup mode, the unit acts as a Wifi Access Point with the SSID "ESP Weather Station".  Connect to this access point with a computer, tablet, or smartphone, and go to the URL <b>192.168.4.1</b> in a web browser.  This will display the configuration page where you can fill in the correct settings:

<b>Wifi Network</b> - The weather station must connect to the internet in order to collect weather forecast data.  Provide SSID of your wifi network for it to connect to.  
<b>Wifi Password</b> - Provide the password for your wifi network.  
<b>OpenWeatherMap API Key</b> - Weather data is provided by OpenWeatherMap, which provides free forecast data.  You will need to establish an account at https://openweathermap.org/ and request an API key through their website.  The API is free for up to 1000 calls a day.  With the default configuration, the unit makes only 24 API calls per day, so this is plenty.  
<b>Location String</b> - This is the name of the location that weather will be provided for.  The recommended format is "City, State, Country" or similar.  You can use the search bar on the homepage of https://openweathermap.org/ to determine the appropriate location string if you are unsure.  
<b>Units</b> - This will switch between Imperial (US) and Metric (everywhere else) units for displaying weather information.  
<b>Update Interval</b> - This sets the frequency at which the weather display is updated.  The default is every 60 minutes.  Increasing the frequency will increase battery usage and API calls.  
<b>Start Time</b> - This determines what time of day the unit starts displaying updates.  For instance, setting this to 6AM means the unit will not fetch and display updates between midnight and 6AM.  This increases battery life.  The default value is midnight.  
<b>Stop Time</b> - This determines what time of day the unit stops displaying updates.  For instance, setting this to 8PM means the unit will not fetch and display updates between 8PM and midnight.  This increases battery life.  The default value is midnight.  

Once you have entered your settings, click the "Save and Reboot" button on the webpage.  The unit will take a few seconds to reboot, then should start displaying weather data.  Any problems will be indicated on screen.  

In my experience, with the default settings, battery life should be about one month using a single 18650 cell or equivalent lithium battery.  Battery life can be increased by reducing the update frequency and/or adjusting the start and stop times to do fewere refreshes per day.  Power use in standby is extremely low, while power draw is relatively high while updating.  

<h1>License</h1>
This code is released under GPL v3.0, as were the projects upon which it is based.  Modification and commercial use is allowed, but source code of derivative projects must be released for free, and proper attribution must be made.  

<h1>Things that could be better</h1>
This code is by no means finished.  Here are a list of things I would like to address in the future:
<ul>
<li>Getting an API key is a bit of a steep ask for users, it would be great to find a weather source that does not require registration and limited API keys</li>
<li>The battery percentage tracking code is OK, not great.  The voltage detection seems to be pretty good on my unit, but the percent reading leaves something to be desired.  Fully fixing this will require a deep dive into lithium battery technology.  </li>
<li>This code was written for the earlier "V3" revision of the T5 4.7 inch development kit.  Judging from online pictures, Lilygo has released a newer revision of the kit, mainly distinguishable by having only three hardware buttons instead of five.  I have not tested the code on the newer version (as I do not have one) but I suspect some minor modifications will be required in order to make it work properly.</li>
<li>Optimization and clean up; I cut down on the amount of internet requests the unit makes compared to previous projects, but I suspect further optimization is possible.  Almost all power consumption occurs while the unit is awake for an update, so optimizing that code more would result in increased battery life.</li>
</ul>
