/**
 * @file global.h
 * @author Martin Verges <martin@verges.cc>
 * @version 0.1
 * @date 2022-05-29
**/
#include <Arduino.h>
#include <DNSServer.h>
#include <WebServer.h>
#include <FS.h>
#include <LittleFS.h>
#include <Preferences.h>
#include "MQTTclient.h"
#include "wifimanager.h"

#define webserverPort 80                    // Start the Webserver on this port
#define NVS_NAMESPACE "ibs2mqtt"            // Preferences.h namespace to store settings

RTC_DATA_ATTR struct timing_t {
  // Check Services like MQTT, ...
  uint64_t lastServiceCheck = 0;               // last millis() from ServiceCheck
  const unsigned int serviceInterval = 30000;  // Interval in ms to execute code

  // Sensor data in loop()
  uint64_t lastSensorRead = 0;                 // last millis() from Sensor read
  const unsigned int sensorInterval = 500;     // Interval in ms to execute code

  // Setup executing in loop()
  uint64_t lastSetupRead = 0;                  // last millis() from Setup run
  const unsigned int setupInterval = 15 * 60 * 1000 / 255;   // Interval in ms to execute code
} Timing;

RTC_DATA_ATTR uint64_t sleepTime = 0;       // Time that the esp32 slept

WIFIMANAGER WifiManager;
bool enableWifi = true;                     // Enable Wifi, disable to reduce power consumtion, stored in NVS

String hostName;
DNSServer dnsServer;
WebServer webServer(webserverPort);
Preferences preferences;

MQTTclient Mqtt;

uint64_t runtime() {
  return rtc_time_slowclk_to_us(rtc_time_get(), esp_clk_slowclk_cal_get()) / 1000;
}
