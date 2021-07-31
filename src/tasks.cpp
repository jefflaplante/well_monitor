/* tasks.cpp */
#include <Arduino.h>
#include <WiFi.h>

#include "tasks.h"
#include "secrets.h"
#include "pins.h"

// Timers
unsigned int const WIFI_WATCHDOG_MS     = 10000; // 10 second WiFi connection watchdog timer
unsigned int const WIFI_TIMEOUT_MS      = 20000; // 20 second WiFi connection timeout
unsigned int const WIFI_RECOVER_TIME_MS = 30000; // 30 second WiFi retry after a failed connection attempt
unsigned int const period               = 10000; // 10 seconds
unsigned int const ONE_HOUR_PERIOD_MS   = 3.6e+6;
unsigned int const TWO_HOUR_PERIOD_MS   = (2 * ONE_HOUR_PERIOD_MS);

// GPIO and ADC Pins
extern const uint8_t PIN_ADC_CT_1        = A6;
extern const uint8_t PIN_ADC_CT_2        = A7;
extern const uint8_t PIN_IN_REQ_1        = GPIO_NUM_21;
extern const uint8_t PIN_IN_REQ_2        = GPIO_NUM_22;
extern const uint8_t PIN_OUT_PUMP_RELAY  = GPIO_NUM_12;
extern const uint8_t PIN_OUT_LED_BACKOFF = GPIO_NUM_13;
extern const uint8_t PIN_LED_ERROR       = LED_BUILTIN;

// Task handles
TaskHandle_t hPollSensors = NULL;
TaskHandle_t hBlinker = NULL;

QueueHandle_t sensorQueue;

void taskWifi(void * parameter) {
    // Parameter passed is the blinker task handle so we can suspend/resume it as needed
    // TaskHandle_t hBlinker = *(TaskHandle_t *)parameter;

    while(1) {
        // If wifi is connected then sleep for n seconds and check again (watchdog)
        if(WiFi.status() == WL_CONNECTED){
            // Serial.println("Check: wifi connected");
            vTaskDelay(WIFI_WATCHDOG_MS / portTICK_PERIOD_MS);
            vTaskSuspend(hBlinker);
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
    }
}

void taskBlinkLED(void * parameter) {
    unsigned int period = *(int *)parameter;
    pinMode(LED_BUILTIN, OUTPUT);
    
    while(1) {
        digitalWrite(LED_BUILTIN, HIGH);
        vTaskDelay(period / portTICK_PERIOD_MS);
        digitalWrite(LED_BUILTIN, LOW);
        vTaskDelay(period / portTICK_PERIOD_MS);
    }
}

void taskPollSensor(void * vParameter) {

    // ADC18 is the pin we use for calibrating the ADC
    // unsigned short const ADC18 = 25;
    // analogSetVRefPin(ADC18);

    while(1){
        Serial.println("\nPoll sensors");

        int mvCurrentSensor1 = analogReadMilliVolts(PIN_ADC_CT_1);
        int mvCurrentSensor2 = analogReadMilliVolts(PIN_ADC_CT_2);

        unsigned int adcValue1 = analogRead(PIN_ADC_CT_1);
        unsigned int adcValue2 = analogRead(PIN_ADC_CT_2);

        float voltValue1 = ((adcValue1 * 3.3) / 4095);
        float voltValue2 = ((adcValue2 * 3.3) / 4095);
        
        struct tm timeinfo;
        getLocalTime(&timeinfo);

        Serial.print(&timeinfo, "%x %X");
        Serial.println("- ADC06: " + String(mvCurrentSensor1) + "mV");
        Serial.println("ADC Value: " + String(adcValue1));
        Serial.println("Volt Value: " + String(voltValue1));

        Serial.println("- ADC07: " + String(mvCurrentSensor2) + "mV");
        Serial.println("ADC Value: " + String(adcValue2));
        Serial.println("Volt Value: " + String(voltValue2));

        vTaskDelay(period / portTICK_PERIOD_MS);
    }
}