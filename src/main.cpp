#include <Arduino.h>
#include <WiFi.h>
#include <time.h>
#include <ArduinoOTA.h>

#include "pins.h"
#include "tasks.h"
#include "sensor.h"
#include "ctsensor.h"
#include "ota.h"

#define true 1
#define false 0

// Timers and periods
unsigned int blinkPeriod = 1000;

// NTP Time
const char *ntpServer         = "north-america.pool.ntp.org";
const long gmtOffset_sec      = -28800; // GMT -8:00 (Pacific)
const int  daylightOffset_sec = 3600;   // Yes to DST

void setPins() {
    pinMode(PIN_OUT_PUMP_RELAY, OUTPUT);    // Pump Relay 
    pinMode(PIN_OUT_LED_BACKOFF, OUTPUT);   // LED Backoff Indicator

    pinMode(PIN_ADC_CT_1, ANALOG);     // Mains L1 Current Sensor 0-1V
    pinMode(PIN_ADC_CT_2, ANALOG);     // Mains L2 Current Sensor 0-1V

    pinMode(PIN_IN_REQ_1, INPUT_PULLUP);    // Resident 1 Water Request (Low == water requested)
    pinMode(PIN_IN_REQ_2, INPUT_PULLUP);    // Resident 2 Water Request (Low == water requested)

    digitalWrite(PIN_OUT_PUMP_RELAY, LOW);  // Set outputs low by default
    digitalWrite(PIN_OUT_LED_BACKOFF, LOW); // Set outputs low by default
}

void scanWifi() {
    Serial.println("Scanning for Wifi networks");
    WiFi.mode(WIFI_STA);
    int n = WiFi.scanNetworks(); // returns the number of networks found
    if (n == 0) {
        Serial.println("no networks found");
    } else {
        Serial.println(String(n) + " networks found\n");
        for (int i = 0; i < n; ++i) {
            // Print SSID and RSSI for each network found
            Serial.println(String(i + 1) + ": " + WiFi.SSID(i) + " (" + String(WiFi.RSSI(i)) + " dBm) " + String((WiFi.encryptionType(i) == WIFI_AUTH_OPEN) ? " " : "*"));
        }
    }
    Serial.println("");
}

void printLocalTime() {
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    Serial.println("Failed to obtain time");
    return;
  }
  Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
}

void timeString(char* timeStr) {
    struct tm timeinfo;
    if(!getLocalTime(&timeinfo)){
        Serial.println("ERROR - Failed to obtain time");
        return;
    }
    strftime(timeStr, sizeof(timeStr), "%H:%M:%S", &timeinfo);
}

void setup() {
    Serial.begin(115200);
    delay(10);

    // Set default values and modes for GPIO pins
    setPins();

    // scanWifi();

    // Start blinking the built-in LED to denote we don't have wifi connected yet
    xTaskCreate(
        taskBlinkLED,
        "task Blink LED",
        900,
        &blinkPeriod,
        1,
        &hBlinker
    );
    vTaskSuspend(hBlinker); // Suspend and toggle from wifi task

    // Task - WiFi connection
    xTaskCreatePinnedToCore(
        taskWifi,    // Function for task
        "task Wifi", // Task name
        5000,        // Stack size (bytes)
        hBlinker,    // Parameter
        1,           // Task priority, larger number is higher priority
        NULL,        // Task handle
        0            // Core to pin to, 0 or 1
    );

    //init and get the time
    while(WiFi.status() != WL_CONNECTED){
        Serial.println("Wifi not connected, waiting 5 seconds...");
        delay(5000);
    }

    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    printLocalTime();

    // Task - Poll Sensors
    xTaskCreate(
        taskPollSensor,
        "task PollSensor",
        5000,
        NULL,
        3,
        &hPollSensors
    );
    //  vTaskSuspend(hPollSensors);

    /* Setup OTA Updates */
    setupOTA();

}

void loop() {
    ArduinoOTA.handle();

    int req_1_val = digitalRead(PIN_IN_REQ_1);
    int req_2_val = digitalRead(PIN_IN_REQ_2);

    double apparentPower = readCTApparentPower(PIN_ADC_CT_1);
    Serial.println("Apparent Power: " + String(apparentPower));

    Serial.println("Request 1: " + String(req_1_val));
    Serial.println("Request 2: " + String(req_2_val));

    // LOW is a request for water
    // HIGH on both request pins should turn off pump
    if(req_1_val && req_2_val) {
        Serial.println("Switch pump off");
        digitalWrite(PIN_OUT_PUMP_RELAY, LOW);
    } else {
        Serial.println("Switch pump on");
        digitalWrite(PIN_OUT_PUMP_RELAY, HIGH);
    }
    
    vTaskDelay(5000 / portTICK_PERIOD_MS);
}
