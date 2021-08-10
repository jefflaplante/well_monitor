#include <math.h>
#include <Arduino.h>

#include "ctsensor.h"
#include "mqtt.h"
#include "state.h"
#include "pins.h"

SemaphoreHandle_t xSemaphoreADC;
TimerHandle_t backoffTimerHandle = NULL;

unsigned int const ONE_HOUR_PERIOD_MS  = 3.6e+6;
unsigned int const TWO_HOUR_PERIOD_MS  = (2 * ONE_HOUR_PERIOD_MS);
unsigned int const ONE_MIN_MS          = 60000;
unsigned int const BACKOFF_TIMER_MS    = TWO_HOUR_PERIOD_MS;
unsigned int const PUMP_NOT_OK_LIMIT   = 3;

// Parameters for measuring RMS current
const double vRMS       = 120.0;   // Assumed or measured
const double offset     = 1.644;   // Half the ADC max voltage in Volts (measured voltage across R2 of voltage divider)
const double numTurns   = 2000.0;  // 1:2000 transformer turns
const double rBurden    = 200.0;   // Burden resistor value in Ohms
const double numSamples = 1000.0;  // Number of samples before calculating RMS

const float badLoadWattsLow  = 1000.0;
const float badLoadWattsHigh = 1500.0;

void backoffTimerCallback(TimerHandle_t xTimer){
    digitalWrite(PIN_OUT_LED_BACKOFF, LOW);
}

bool isPumpOk(State *state){
    const float p = state->power;

    // Update state.backoff == PIN_OUT_LED_BACKOFF since timer callback doesn't have access to state struct
    state->backoff = digitalRead(PIN_OUT_LED_BACKOFF);

    // Update backoff timer timeout seconds if a timer exists and is active
    if(backoffTimerHandle != NULL) { 
        if (xTimerIsTimerActive(backoffTimerHandle)) {
            state->backoffTimeoutSeconds = (((xTimerGetExpiryTime(backoffTimerHandle) - xTaskGetTickCount()) / portTICK_PERIOD_MS) / 1000);
        }
    }

    // If pump power consumption is in the band where we know it's sucking air then:
    //  Count occurrences and trigger remidiation after PUMP_NOT_OK_LIMIT occurrences:
    //      Turn off the pump
    //      Set backoff state to true
    //      Start the backoff timer
    //
    if(p >= badLoadWattsLow && p <= badLoadWattsHigh) {
        state->pumpNotOkCount++;
        state->pumpOk = false;

        if(state->pumpNotOkCount >= PUMP_NOT_OK_LIMIT) {
            digitalWrite(PIN_OUT_LED_BACKOFF, HIGH);  // Turn on backoff LED indicator
            digitalWrite(PIN_OUT_PUMP_RELAY, LOW);    // Turn off pump
            state->backoff = true;
            state->pumpOn  = false;

            // Create a backoff timer if it doesn't already exist
            // Start backoff timer if not already active
            if(backoffTimerHandle != NULL) {
                if(!xTimerIsTimerActive(backoffTimerHandle)) {
                    xTimerStart(backoffTimerHandle, 100);
                    mqttLog("starting backoff timer");
                } else {
                    mqttLog("backoff timer already active");
                    TickType_t xRemainingTime = (xTimerGetExpiryTime(backoffTimerHandle) - xTaskGetTickCount()) / portTICK_PERIOD_MS;
                    
                    char l[100];
                    sprintf(l, "backoff timer will expire in %d seconds", (xRemainingTime / 1000) );
                    mqttLog(l);
                }
            } else {
                mqttLog("creating backoff timer");

                backoffTimerHandle = xTimerCreate(
                    "Backoff",
                    BACKOFF_TIMER_MS / portTICK_PERIOD_MS,
                    pdFALSE,
                    NULL,
                    backoffTimerCallback
                );
                
                if(backoffTimerHandle != NULL) {
                    xTimerStart(backoffTimerHandle, 100);
                } else {
                    mqttLog("ERROR: unable to create backoff timer");
                }
            }
        }

        return false;
    }

    state->pumpOk = true;
    state->pumpNotOkCount = 0;
    return true;
}

extern double readCTApparentPower(int pin, State *state) {
    int startTime = millis();

    double voltage;
    double adjVoltage;
    double iPrimary;
    double iSecondary;
    double acc = 0.0;
    double iRMS;
    double apparentPower;
    
    // Take a number of samples and calculate RMS current
    if(xSemaphoreADC != NULL) {
        if(xSemaphoreTake(xSemaphoreADC, ( TickType_t ) 100) == pdTRUE) {
            for ( int i = 0; i < numSamples; i++ ) {
                
                // Read ADC, convert to voltage, remove offset
                // (offset is applied by a voltage divider in the circuit between shield and ground)

                voltage = analogReadMilliVolts(pin) / 1000.0; // divide by 1000 to convert mV to V
                adjVoltage = voltage - offset;
                
                iSecondary = adjVoltage / rBurden;
                iPrimary = iSecondary * numTurns;

                // Square current and add to accumulator
                acc += pow(iPrimary, 2);
            }
            xSemaphoreGive(xSemaphoreADC);
        } else {
            mqttLog("Unable to get semaphore to read from ADC during readCTApparentPower()");
        }
    } else {
        mqttLog("xSemaphoreADC is NULL");
    }
    
    // Calculate RMS current from accumulated values
    iRMS = sqrt(acc / numSamples);

    // Ignore noise below 500mA
    if(iRMS < 0.5 ) {
        iRMS = 0.0;
    }
    
    // Calculate apparent power
    apparentPower = vRMS * iRMS;

    char l[100];
    sprintf(l, "%3.2fV * %2.1fA = %4.1fW", vRMS, iRMS, apparentPower);
    mqttLog(l);

    state->voltage = vRMS;
    state->current = iRMS;
    state->power = apparentPower;
    
    // realPowerWatts = apparentPowerVoltAmps * powerFactor
    // (probably a wash in regards to the power factor of the well pump)

    isPumpOk(state);

    mqttPublish("well/monitor/metrics/readCTApparentPower_time_ms", 0, false, String(millis() - startTime).c_str());
    return apparentPower; 
}

