/**
 * @file global.h
 * @author Martin Verges <martin@verges.cc>
 * @version 0.1
 * @date 2022-05-29
**/

#if !(defined(ESP32))
  #error This code is intended to run on the ESP32 platform! Please check your Tools->Board setting.
#endif

#define uS_TO_S_FACTOR   1000000           // Conversion factor for micro seconds to seconds
#define TIME_TO_SLEEP    10                // WakeUp interval
#define LOOP_DELAY 5000                    // Interval for loop()

#if !(defined(ARDUINO_ARCH_ESP32))
  #define ARDUINO_ARCH_ESP32 true
#endif
#undef USE_LittleFS
#define USE_LittleFS true

#include <Arduino.h>
#include <WebServer.h>
#include <Preferences.h>
#include <ESPmDNS.h>

// Power Management
#include <driver/rtc_io.h>
#include <esp_sleep.h>
#include <soc/rtc.h>
extern "C" {
  #if ESP_ARDUINO_VERSION_MAJOR >= 2
    #include <esp32/clk.h>
  #else
    #include <esp_clk.h>
  #endif
}

#include "global.h"
#include "api-routes.h"

// LIN + IBS200x Sensor
#include <TJA1020.hpp>
#include <IBS_Sensor.hpp>
#define LIN_SERIAL_SPEED LIN_BAUDRATE_IBS_SENSOR
#define PIN_NSLP 17
Lin_TJA1020 LinBus(1, LIN_SERIAL_SPEED, PIN_NSLP);
IBS_Sensor BatSensor(2);

void deepsleepForSeconds(int seconds) {
    esp_sleep_enable_timer_wakeup(seconds * uS_TO_S_FACTOR);
    esp_deep_sleep_start();
}

// Check if a feature is enabled, that prevents the
// deep sleep mode of our ESP32 chip.
void sleepOrDelay() {
  if (enableWifi || enableMqtt) {
    yield();
    delay(LOOP_DELAY);
  } else {
    // We can save a lot of power by going into deepsleep
    // Thid disables WIFI and everything.
    esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
    sleepTime = rtc_time_slowclk_to_us(rtc_time_get(), esp_clk_slowclk_cal_get());

    preferences.end();
    Serial.println(F("[POWER] Sleeping..."));
    esp_deep_sleep_start();
  }
}

void print_wakeup_reason() {
  esp_sleep_wakeup_cause_t wakeup_reason;
  wakeup_reason = esp_sleep_get_wakeup_cause();

  switch(wakeup_reason) {
    case ESP_SLEEP_WAKEUP_EXT0 : 
      Serial.println(F("[POWER] Wakeup caused by external signal using RTC_IO"));
    break;
    case ESP_SLEEP_WAKEUP_EXT1 : Serial.println(F("[POWER] Wakeup caused by external signal using RTC_CNTL")); break;
    case ESP_SLEEP_WAKEUP_TIMER : 
      Serial.println(F("[POWER] Wakeup caused by timer"));
      uint64_t timeNow, timeDiff;
      timeNow = rtc_time_slowclk_to_us(rtc_time_get(), esp_clk_slowclk_cal_get());
      timeDiff = timeNow - sleepTime;
      printf("Now: %" PRIu64 "ms, Duration: %" PRIu64 "ms\n", timeNow / 1000, timeDiff / 1000);
      delay(2000);
    break;
    case ESP_SLEEP_WAKEUP_TOUCHPAD : Serial.println(F("[POWER] Wakeup caused by touchpad")); break;
    case ESP_SLEEP_WAKEUP_ULP : Serial.println(F("[POWER] Wakeup caused by ULP program")); break;
    default : Serial.printf("[POWER] Wakeup was not caused by deep sleep: %d\n",wakeup_reason); break;
  }
}

void MDNSBegin(String hostname) {
  if (!enableWifi) return;
  Serial.println("[MDNS] Starting mDNS Service!");
  MDNS.begin(hostname.c_str());
  MDNS.addService("http", "tcp", 80);
}

void initWifiAndServices() {
  Serial.println(F("[DEBUG] calling initWifiAndServices()"));

  // Load well known Wifi AP credentials from NVS
  WifiManager.startBackgroundTask();
  WifiManager.attachWebServer(&webServer);
  WifiManager.fallbackToSoftAp(preferences.getBool("enableSoftAp", true));

  APIRegisterRoutes();
  webServer.begin();
  Serial.println(F("[WEB] HTTP server started"));

  MDNSBegin(hostName);

  if (enableMqtt) {
    Mqtt.prepare(
      preferences.getString("mqttHost", "localhost"),
      preferences.getUInt("mqttPort", 1883),
      preferences.getString("mqttTopic", "verges/tanklevel"),
      preferences.getString("mqttUser", ""),
      preferences.getString("mqttPass", "")
    );
  }
  else Serial.println(F("[MQTT] Publish to MQTT is disabled."));
}

void setup() {
  Serial.begin(115200);
  Serial.setDebugOutput(true);
  Serial.println(F("\n\n==== starting ESP32 setup() ===="));

  print_wakeup_reason();
  Serial.printf("[SETUP] Configure ESP32 to sleep for every %d Seconds\n", TIME_TO_SLEEP);

  LinBus.setSlope(LinBus.LowSlope);
  BatSensor.LinBus = &LinBus;

  if (!LittleFS.begin(true)) {
    Serial.println(F("[FS] An Error has occurred while mounting LittleFS"));
    // Reduce power consumption while having issues with NVS
    // This won't fix the problem, a check of the sensor log is required
    deepsleepForSeconds(5);
  }
  if (!preferences.begin(NVS_NAMESPACE)) preferences.clear();

  // Load Settings from NVS
  hostName = preferences.getString("hostName");
  if (hostName.isEmpty()) {
    hostName = "ibs2mqtt";
    preferences.putString("hostName", hostName);
  }
  enableWifi = preferences.getBool("enableWifi", true);
  enableMqtt = preferences.getBool("enableMqtt", false);
  preferences.end();
  
  if (enableWifi) initWifiAndServices();
  else Serial.println(F("[WIFI] Not starting WiFi!"));
}

// Soft reset the ESP to start with setup() again, but without loosing RTC_DATA as it would be with ESP.reset()
void softReset() {
  if (enableWifi) {
    webServer.close();
    MDNS.end();
    Mqtt.disconnect();
    WifiManager.stopWifi();
  }
  esp_sleep_enable_timer_wakeup(1);
  esp_deep_sleep_start();
}

void loop() {
  webServer.handleClient();
  
  if (runtime() - Timing.lastServiceCheck > Timing.serviceInterval) {
    Timing.lastServiceCheck = runtime();
    // Check if all the services work
    if (enableWifi && WiFi.status() == WL_CONNECTED && WiFi.getMode() & WIFI_MODE_STA) {
      if (enableMqtt && !Mqtt.isConnected()) Mqtt.connect();
    }
  }
  /*
  BatSensor.readFrames();
  LinBus.setMode(LinBus.Sleep);

  if (enableMqtt && Mqtt.isReady()) {
    Mqtt.client.publish((Mqtt.mqttTopic + String("/calibrationDone")).c_str(), 0, true, String(BatSensor.CalibrationDone).c_str());
    Mqtt.client.publish((Mqtt.mqttTopic + String("/UBat")).c_str(), 0, true, String(BatSensor.Ubat).c_str());
    Mqtt.client.publish((Mqtt.mqttTopic + String("/Ibat")).c_str(), 0, true, String(BatSensor.Ibat).c_str());
    Mqtt.client.publish((Mqtt.mqttTopic + String("/SOC")).c_str(), 0, true, String(BatSensor.SOC).c_str());
    Mqtt.client.publish((Mqtt.mqttTopic + String("/SOH")).c_str(), 0, true, String(BatSensor.SOH).c_str());
    Mqtt.client.publish((Mqtt.mqttTopic + String("/Cap_Available")).c_str(), 0, true, String(BatSensor.Cap_Available).c_str());
  }
  Serial.printf("Voltage: %.3f Volt\tState of Charge: %.1f %%\n", BatSensor.Ubat, BatSensor.SOC);
/*
  Serial.printf("-----------------------------\n");
  Serial.printf("Calibration done: %d\n", BatSensor.CalibrationDone);
  Serial.printf("Voltage: %.3f Volt\n", BatSensor.Ubat);
  Serial.printf("Current: %.3f Ampere\n", BatSensor.Ibat);
  Serial.printf("State of Charge: %.1f %%\n", BatSensor.SOC);
  Serial.printf("State of Health: %.1f %%\n", BatSensor.SOH);
  Serial.printf("Available Capacity: %.1f\n", BatSensor.Cap_Available);
*/
  //sleepOrDelay();
}
