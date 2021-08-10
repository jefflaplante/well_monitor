/* tasks.cpp */
#include <Arduino.h>
#include <WiFi.h>

#include "tasks.h"
#include "secrets.h"
#include "config.h"
#include "pins.h"
#include "ctsensor.h"
#include "mqtt.h"
#include "state.h"

// Timers
unsigned int const WIFI_WATCHDOG_MS     = 10000; // 10 second WiFi connection watchdog timer
unsigned int const WIFI_TIMEOUT_MS      = 20000; // 20 second WiFi connection timeout
unsigned int const WIFI_RECOVER_TIME_MS = 30000; // 30 second WiFi retry after a failed connection attempt
unsigned int const POLL_SENSORS_MS      = 10000; // 10 seconds
unsigned int const ONE_HOUR_PERIOD_MS   = 3.6e+6;
unsigned int const TWO_HOUR_PERIOD_MS   = (2 * ONE_HOUR_PERIOD_MS);

// GPIO and ADC Pins
extern const uint8_t PIN_ADC_CT_1        = A7;
extern const uint8_t PIN_IN_REQ_1        = GPIO_NUM_21;
extern const uint8_t PIN_IN_REQ_2        = GPIO_NUM_22;
extern const uint8_t PIN_OUT_PUMP_RELAY  = GPIO_NUM_12;
extern const uint8_t PIN_OUT_LED_BACKOFF = GPIO_NUM_13;
extern const uint8_t PIN_LED_ERROR       = LED_BUILTIN;

// Task handles
TaskHandle_t hPollSensors = NULL;
TaskHandle_t hBlinker = NULL;

// State
// int state_req_1   = 0;
// int state_req_2   = 0;
// int state_pump    = 0;
// int state_backoff = 0;
// int state_pump_ok = 0;

// QueueHandle_t sensorQueue;

void taskWifi(void * parameter) {
    while(1) {
        // If wifi is connected then sleep for n seconds and check again (watchdog)
        if(WiFi.status() == WL_CONNECTED){
            // Serial.println("Check: wifi connected");
            vTaskDelay(WIFI_WATCHDOG_MS / portTICK_PERIOD_MS);
            vTaskSuspend(hBlinker);
            digitalWrite(LED_BUILTIN, LOW);
            continue;
        }

        // Resume blinker while waiting for wifi
        vTaskResume(hBlinker);

        Serial.println("\n[WIFI] Connecting");
        WiFi.mode(WIFI_STA);
        WiFi.begin(WIFI_NETWORK, WIFI_PASSWORD);

        unsigned int startAttemptTime = millis();

        // Keep looping while we're not connected and haven't reached the timeout
        while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < WIFI_TIMEOUT_MS){
            vTaskDelay(500 / portTICK_PERIOD_MS); // Delay 500ms
        }

        // When we couldn't make a WiFi connection (or the timeout expired)
		// Yield and retry
        if(WiFi.status() != WL_CONNECTED){
            Serial.println("[WIFI] FAILED");
            vTaskDelay(WIFI_RECOVER_TIME_MS / portTICK_PERIOD_MS);
            continue;
        }

        Serial.println("[WIFI] Connected");
        Serial.print("IP address: " + WiFi.localIP().toString() + "\n");
        vTaskSuspend(hBlinker);
        digitalWrite(LED_BUILTIN, LOW);
    }
}

void taskBlinkLED(void * parameter) {
    unsigned int period = *(int *)parameter;
    pinMode(PIN_LED_ERROR, OUTPUT);
    
    while(1) {
        digitalWrite(PIN_LED_ERROR, HIGH);
        vTaskDelay(period / portTICK_PERIOD_MS);
        digitalWrite(PIN_LED_ERROR, LOW);
        vTaskDelay(period / portTICK_PERIOD_MS);
    }
}

void checkRequestForWater(State *state) {
    int startTime = millis();

    char req_1_val = digitalRead(PIN_IN_REQ_1);
    char req_2_val = digitalRead(PIN_IN_REQ_2);

    state->req1 = req_1_val;
    state->req2 = req_2_val;

    Serial.println("Request 1: " + String(req_1_val));
    Serial.println("Request 2: " + String(req_2_val));

    mqttPublish("well/monitor/metrics/checkRequestForWater_time_ms", 0, false, String(millis() - startTime).c_str());
}

void resolveState(State *state) {
    
    // LOW is a request for water
    // HIGH on both request pins should turn off pump
    // HIGH on PIN_OUT_PUMP_RELAY turns relay ON
    if(state->req1 && state->req2) {
        digitalWrite(PIN_OUT_PUMP_RELAY, LOW);
        state->pumpOn = LOW;
    } else {
        // If backoff is true then turn off pump
        if (state->backoff) { 
            digitalWrite(PIN_OUT_PUMP_RELAY, LOW);
            state->pumpOn = LOW;
        } else {
            digitalWrite(PIN_OUT_PUMP_RELAY, HIGH);
            state->pumpOn = HIGH;
         }
    }
}

void mqttPublishState(State *state){
    // Send inverse of pin read to mqtt as these are PULL DOWN pins where LOW == TRUE
    mqttPublish("well/monitor/water_request/1", 0, false, String(!state->req1).c_str());
    mqttPublish("well/monitor/water_request/2", 0, false, String(!state->req2).c_str());

    const char *pumpState = "OFF"; 
    if(state->pumpOn) {
        pumpState = "ON";
    } else {
        pumpState = "OFF";
    }
    mqttPublish("well/monitor/pump", 0, false, pumpState);

    const char *backoffState = "OFF"; 
    if(state->backoff) {
        backoffState = "ON";
    } else {
        backoffState = "OFF";
    }
    mqttPublish("well/monitor/pump/backoff", 0, true, backoffState);
    mqttPublish("well/monitor/pump/pump_not_ok_count", 0, true, String(state->pumpNotOkCount).c_str());

    mqttPublish("well/monitor/pump/mains_volts", 0, false, String(state->voltage).c_str());
    mqttPublish("well/monitor/pump/current_amps", 0, false, String(state->current).c_str());
    mqttPublish("well/monitor/pump/power_watts", 0, false, String(state->power).c_str());

    mqttPublish("well/monitor/pump/backoff_timeout_minutes", 0, false, String(state->backoffTimeoutSeconds).c_str());

    // Update HomeAssistant state endpoint
    char l[100];
    sprintf(l, "{\"state\": \"%s\", \"current\": %2.2f, \"power\": %.0f}", pumpState, state->current, state->power );
    mqttPublish("homeassistant/sensor/well_monitor/state", 0, true, l);
}

void taskPollSensors(void * state) {
    while(1){
        struct tm timeinfo;
        getLocalTime(&timeinfo);
        Serial.print(&timeinfo, "%x %X");

        State *s = (struct State*) state; // Cast void pointer to a State struct pointer

        // Check request for water and update state
        checkRequestForWater(s);

        // Check well pump power and update state
        readCTApparentPower(PIN_ADC_CT_1, s);
        
        // Look at State struct and resolve desired state
        resolveState(s);

        // Publish state to MQTT
        mqttPublishState(s);

        if(xSemaphoreADC != NULL) {
            if(xSemaphoreTake(xSemaphoreADC, ( TickType_t ) 100) == pdTRUE) {
                unsigned int adc_Value = analogRead(PIN_ADC_CT_1);
                int adc_mV = analogReadMilliVolts(PIN_ADC_CT_1);
                xSemaphoreGive(xSemaphoreADC);

                mqttPublish("well/monitor/pump/raw/adc_mV", 0, false, String(adc_mV).c_str());
                mqttPublish("well/monitor/pump/raw/adc_adjusted_mV", 0, false, String(adc_mV - (offset * 1000)).c_str());
                mqttPublish("well/monitor/pump/raw/adc_Value", 0, false, String(adc_Value).c_str());
            } else {
                mqttLog("Unable to get semaphore to read from ADC during taskPollSensors()");
            }
        } else {
            mqttLog("xSemaphoreADC is NULL");
        }

        vTaskDelay(POLL_SENSORS_MS / portTICK_PERIOD_MS);
    }
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
