/* state.h */
#ifndef STATE_H
#define STATE_H

struct State {
    bool backoff;
    unsigned long backoffTimeoutSeconds;
    unsigned int pumpNotOkCount;
    bool pumpOn;
    bool pumpOk;

    bool req1;
    bool req2;

    float voltage;
    float current;
    float power;
};

void setPumpRelay(short int state);

void setBackoff(short int state);

#endif /* !STATE_H */
