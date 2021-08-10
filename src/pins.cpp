#include <Arduino.h>

#include "pins.h"

void setPins() {
    pinMode(PIN_OUT_PUMP_RELAY, OUTPUT);    // Pump Relay 
    pinMode(PIN_OUT_LED_BACKOFF, OUTPUT);   // LED Backoff Indicator

    pinMode(PIN_ADC_CT_1, ANALOG);          // Mains L1 Current Sensor 0-1V

    pinMode(PIN_IN_REQ_1, INPUT_PULLUP);    // Resident 1 Water Request (Low == water requested)
    pinMode(PIN_IN_REQ_2, INPUT_PULLUP);    // Resident 2 Water Request (Low == water requested)

    digitalWrite(PIN_OUT_PUMP_RELAY, LOW);  // Set outputs low by default
    digitalWrite(PIN_OUT_LED_BACKOFF, LOW); // Set outputs low by default
}
