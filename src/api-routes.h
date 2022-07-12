/**
 * @file api-routes.h
 * @author Martin Verges <martin@verges.cc>
 * @version 0.1
 * @date 2022-05-29
**/

#include <Arduino.h>
#include <AsyncJson.h>
#include <ArduinoJson.h>
#include <ESPAsyncWebServer.h>
#include <FS.h>
#include <LittleFS.h>
#include <IBS_Sensor.hpp>

extern bool enableWifi;
extern bool enableMqtt;
extern IBS_Sensor BatSensor;

void APIRegisterRoutes() {
/*
  webServer.on("/api/status", HTTP_GET, [&](AsyncWebServerRequest *request) {
    String output;
    StaticJsonDocument<1024> doc;

    doc["CalibrationDone"] = BatSensor.CalibrationDone;
    doc["Ubat"] = BatSensor.Ubat;
    doc["Ibat"] = BatSensor.Ibat;
    doc["SOC"] = BatSensor.SOC;
    doc["SOH"] = BatSensor.SOH;
    doc["Cap_Available"] = BatSensor.Cap_Available;

    serializeJson(doc, output);
    request->send(200, "application/json", output);
  });
*/
  webServer.on("/api/level/current", HTTP_GET, [&](AsyncWebServerRequest *request) {
    if (request->contentType() == "application/json") {
      String output;
      StaticJsonDocument<16> doc;
      doc["levelPercent"] = BatSensor.SOC;
      serializeJson(doc, output);
      request->send(200, "application/json", output);
    } else request->send(200, "text/plain", (String)BatSensor.SOC);
  });

  webServer.on("/api/reset", HTTP_POST, [&](AsyncWebServerRequest *request) {
    AsyncResponseStream *response = request->beginResponseStream("application/json");
    request->send(200, "application/json", "{\"message\":\"Resetting the sensor!\"}");
    request->send(response);
    yield();
    delay(250);
    ESP.restart();
  });

  webServer.on("/api/config", HTTP_POST, [&](AsyncWebServerRequest * request){}, NULL,
    [&](AsyncWebServerRequest * request, uint8_t *data, size_t len, size_t index, size_t total) {

    DynamicJsonDocument jsonBuffer(1024);
    deserializeJson(jsonBuffer, (const char*)data);

    if (preferences.begin(NVS_NAMESPACE)) {
      String hostname = jsonBuffer["hostname"].as<String>();
      if (!hostname || hostname.length() < 3 || hostname.length() > 32) {
        // TODO: Add better checks according to RFC hostnames
        request->send(422, "application/json", "{\"message\":\"Invalid hostname!\"}");
        return;
      } else {
        preferences.putString("hostName", hostname);
      }

      if (preferences.putBool("enableWifi", jsonBuffer["enablewifi"].as<boolean>())) {
        enableWifi = jsonBuffer["enablewifi"].as<boolean>();
      }
      if (preferences.putBool("enableSoftAp", jsonBuffer["enablesoftap"].as<boolean>())) {
        WifiManager.fallbackToSoftAp(jsonBuffer["enablesoftap"].as<boolean>());
      }

      // MQTT Settings
      preferences.putUInt("mqttPort", jsonBuffer["mqttport"].as<uint16_t>());
      preferences.putString("mqttHost", jsonBuffer["mqtthost"].as<String>());
      preferences.putString("mqttTopic", jsonBuffer["mqtttopic"].as<String>());
      preferences.putString("mqttUser", jsonBuffer["mqttuser"].as<String>());
      preferences.putString("mqttPass", jsonBuffer["mqttpass"].as<String>());
      if (preferences.putBool("enableMqtt", jsonBuffer["enablemqtt"].as<boolean>())) {
        if (enableMqtt) Mqtt.disconnect();
        enableMqtt = jsonBuffer["enablemqtt"].as<boolean>();
        if (enableMqtt) {
          Mqtt.prepare(
            jsonBuffer["mqtthost"].as<String>(),
            jsonBuffer["mqttport"].as<uint16_t>(),
            jsonBuffer["mqtttopic"].as<String>(),
            jsonBuffer["mqttuser"].as<String>(),
            jsonBuffer["mqttpass"].as<String>()
          );
          Mqtt.connect();
        }
      }
    }
    preferences.end();
    
    request->send(200, "application/json", "{\"message\":\"New hostname stored in NVS, reboot required!\"}");
  });

  webServer.on("/api/config", HTTP_GET, [&](AsyncWebServerRequest *request) {
    if (request->contentType() == "application/json") {
      String output;
      StaticJsonDocument<1024> doc;

      if (preferences.begin(NVS_NAMESPACE, true)) {
        doc["hostname"] = hostName;
        doc["enablewifi"] = enableWifi;
        doc["enablesoftap"] = WifiManager.getFallbackState();

        // MQTT
        doc["enablemqtt"] = enableMqtt;
        doc["mqttport"] = preferences.getUInt("mqttPort", 1883);
        doc["mqtthost"] = preferences.getString("mqttHost", "");
        doc["mqtttopic"] = preferences.getString("mqttTopic", "");
        doc["mqttuser"] = preferences.getString("mqttUser", "");
        doc["mqttpass"] = preferences.getString("mqttPass", "");
      }
      preferences.end();

      serializeJson(doc, output);
      request->send(200, "application/json", output);
    } else request->send(415, "text/plain", "Unsupported Media Type");
  });

  webServer.on("/api/esp/heap", HTTP_GET, [&](AsyncWebServerRequest * request) {
    request->send(200, "text/plain", String(ESP.getFreeHeap()));
  });

  webServer.on("/api/esp/cores", HTTP_GET, [&](AsyncWebServerRequest * request) {
    request->send(200, "text/plain", String(ESP.getChipCores()));
  });

  webServer.on("/api/esp/freq", HTTP_GET, [&](AsyncWebServerRequest * request) {
    request->send(200, "text/plain", String(ESP.getCpuFreqMHz()));
  });

  webServer.serveStatic("/", LittleFS, "/")
    .setCacheControl("max-age=86400")
    .setDefaultFile("index.html");

  webServer.onNotFound([&](AsyncWebServerRequest *request) {
    if (request->method() == HTTP_OPTIONS) {
      request->send(200);
    } else {
      if (request->contentType() == "application/json") {
        request->send(404, "application/json", "{\"message\":\"Not found\"}");
      } else request->send(404, "text/plain", "Not found");
    }
  });
}
