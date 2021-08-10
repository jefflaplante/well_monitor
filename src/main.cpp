#include <Arduino.h>
#include <WiFi.h>
#include <time.h>
#include <ArduinoOTA.h>

#include "pins.h"
#include "state.h"
#include "tasks.h"
#include "sensor.h"
#include "ctsensor.h"
#include "ota.h"
#include "mqtt.h"
#include "homeassistant.hpp"

#define true 1
#define false 0

/* struct to hold the state variables for the pump monitor */
struct State state;

// NTP Time
const char *ntpServer         = "north-america.pool.ntp.org";
const long gmtOffset_sec      = -28800; // GMT -8:00 (Pacific)
const int  daylightOffset_sec = 3600;   // Yes to DST

void printLocalTime() {
    struct tm timeinfo;
    if(!getLocalTime(&timeinfo)){
        Serial.println("Failed to obtain time");
        mqttLog("Failed to obtain time from NTP server");
        return;
    }
    Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
}

void timeString(char* timeStr) {
    struct tm timeinfo;
    if(!getLocalTime(&timeinfo)){
        Serial.println("ERROR - Failed to obtain time");
        mqttLog("ERROR: Failed to obtain time");
        return;
    }
    strftime(timeStr, sizeof(timeStr), "%H:%M:%S", &timeinfo);
}

/* Set default state */
void setDefaultState() {
    state.backoff = false;
    state.pumpOn = false;
    state.pumpOk = true;
    state.pumpNotOkCount = 0;
    state.backoffTimeoutSeconds = 0;
    state.req1 = false;
    state.req2 = false;
    state.voltage = 0;
    state.current = 0;
    state.power = 0;
}

void setup() {
    Serial.begin(115200);
    delay(10);

    /* Set default values and modes for GPIO pins */
    setPins();

    /* Set default state */
    setDefaultState();

    /* Create semaphore to guard reads of ADC */
    xSemaphoreADC = xSemaphoreCreateMutex();

    if(xSemaphoreADC == NULL) {
        mqttLog("ERROR creating xSemaphoreADC to guard ADC reads");
    }

    /* Task - blink onboard LED */
    unsigned int blinkPeriod = 700;
    xTaskCreate(
        taskBlinkLED,
        "task Blink LED",
        900,
        &blinkPeriod,
        1,
        &hBlinker
    );
    vTaskSuspend(hBlinker);

    /* Task - Establish and maintain a WiFi connection */
    xTaskCreatePinnedToCore(
        taskWifi,    // Function for task
        "task Wifi", // Task name
        5000,        // Stack size (bytes)
        NULL,        // Parameter
        1,           // Task priority, larger number is higher priority
        NULL,        // Task handle
        0            // Core to pin to, 0 or 1
    );

    /* Check WiFi Connection */
    Serial.println("Waiting for Wifi connection");
    while(WiFi.status() != WL_CONNECTED){
        Serial.print(".");
        delay(500);
    }
    Serial.println("");

    /* Configure NTP and RTC */
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    printLocalTime();

    /* Setup OTA Updates */
    setupOTA();

    /* Setup MQTT Client */
    setupMQTT();

    /* Task - Poll Sensors */
    xTaskCreate(
        taskPollSensors,
        "task Poll Sensors",
        5000,
        &state,
        3,
        &hPollSensors
    );

    mqttLog("Well monitor setup complete");
}

void loop() {
    ArduinoOTA.handle();
    HASetupSensors();
    vTaskDelay(5000 / portTICK_PERIOD_MS);
}
