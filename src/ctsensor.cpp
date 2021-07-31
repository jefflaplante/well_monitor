#include <math.h>
#include <Arduino.h>

#include "ctsensor.h"

// RMS voltage
const double vRMS = 120.0;      // Assumed or measured

// Parameters for measuring RMS current
const double offset = 1.65;     // Half the ADC max voltage
const int numTurns = 2000;      // 1:2000 transformer turns
const int rBurden = 100;        // Burden resistor value
const int numSamples = 1000;    // Number of samples before calculating RMS

extern double readCTApparentPower(int pin) {
    int sample;
    double voltage;
    double iPrimary;
    double acc = 0;
    double iRMS;
    double apparentPower;
    
    // Take a number of samples and calculate RMS current
    for ( int i = 0; i < numSamples; i++ ) {
        
        // Read ADC, convert to voltage, remove offset
        // (offset is applied by a voltage divider in the circuit between shield and ground)
        sample = analogRead(pin);
        voltage = (sample * 3.3) / 4096; // 12-bit ADC is 4096 values from 0 to 3.3v
        voltage = voltage - offset;
        
        // Calculate the sensed current
        iPrimary = (voltage / rBurden) * numTurns;
        
        // Square current and add to accumulator
        acc += pow(iPrimary, 2);
    }
    
    // Calculate RMS from accumulated values
    iRMS = sqrt(acc / numSamples);
    
    // Calculate apparent power and publish it
    apparentPower = vRMS * iRMS;

    return apparentPower; // in VoltAmps
}
