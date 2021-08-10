/* ctsensor.h */
#ifndef CTSENSOR_H
#define CTSENSOR_H

#include "state.h"

extern SemaphoreHandle_t xSemaphoreADC;
extern TimerHandle_t backoffTimerHandle;
extern const double offset;
extern const float badLoadWattsLow;
extern const float badLoadWattsHigh;

double readCTApparentPower(int pin, State *state);

#endif /* !CTSENSOR_H */