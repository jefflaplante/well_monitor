/* tasks.h */
#ifndef TASKS_H
#define TASKS_H

#include <Arduino.h>

// Timers
extern unsigned int const WIFI_WATCHDOG_MS;
extern unsigned int const WIFI_TIMEOUT_MS;   
extern unsigned int const WIFI_RECOVER_TIME_MS;
extern unsigned int const period;  
extern unsigned int const ONE_HOUR_PERIOD_MS;   
extern unsigned int const TWO_HOUR_PERIOD_MS;

// Task handles
extern TaskHandle_t hPollSensors;
extern TaskHandle_t hBlinker;

void taskBlinkLED(void * parameter);

void taskWifi(void * parameter);

void taskPollSensor(void * vParameter);

#endif /* !TASKS_H */
