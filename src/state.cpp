#include <Arduino.h>

#include "state.h"
#include "pins.h"

void setPumpRelay(short int state) {
    pinMode(PIN_OUT_PUMP_RELAY, OUTPUT);
    digitalWrite(PIN_OUT_PUMP_RELAY, HIGH);
}

void setBackoff(short int state) {
    pinMode(PIN_OUT_LED_BACKOFF, OUTPUT);
    digitalWrite(PIN_OUT_LED_BACKOFF, state);
}