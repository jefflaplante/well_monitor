#include <ArduinoJson.h>

#include "mqtt.h"

/* Setup MQTT topics for HomeAssistant */
void HASetupSensors() {
    StaticJsonDocument<500> doc;
    char output[500];
    const char * topic = "";

    // Sample state topic for well monitor
    // mqttPublish("homeassistant/sensor/well_monitor/state", 0, true, "{\"state\": \"OFF\", \"current\": 0, \"power\": 0}");

    /* Well Pump Binary Sensor */

    topic = "homeassistant/binary_sensor/well_monitor_pump/config";
    doc["name"] = "Well Monitor: Pump State";
    doc["unique_id"] = "well_monitor_pump_state";
    doc["device_class"] = "power";
    doc["state_topic"] = "homeassistant/sensor/well_monitor/state";
    doc["value_template"] = "{{ value_json.state }}";
    serializeJson(doc, output);
    mqttPublish(topic, 0, false, output);
    doc.clear();


    /* Well Pump Sensor - Pump Current and Power */

    topic = "homeassistant/sensor/well_monitor_pump_current/config";
    doc["name"] = "Well Monitor: Pump Current";
    doc["unique_id"] = "well_monitor_pump_current";
    doc["device_class"] = "current";
    doc["state_topic"] = "homeassistant/sensor/well_monitor/state";
    doc["value_template"] = "{{ value_json.current }}";
    serializeJson(doc, output);
    mqttPublish(topic, 0, false, output);
    doc.clear();

    topic = "homeassistant/sensor/well_monitor_pump_power/config";
    doc["name"] = "Well Monitor: Pump Power";
    doc["unique_id"] = "well_monitor_pump_power";
    doc["device_class"] = "power";
    doc["state_topic"] = "homeassistant/sensor/well_monitor/state";
    doc["value_template"] = "{{ value_json.power }}";
    serializeJson(doc, output);
    mqttPublish(topic, 0, false, output);
    doc.clear();

    // /* Well Pump Switch */

    topic = "homeassistant/switch/well_monitor_switch/config";
    doc["name"] = "Well Monitor: Pump Switch";
    doc["unique_id"] = "well_pump_power_switch";
    doc["device_class"] = "switch";
    doc["state_topic"] = "homeassistant/sensor/well_monitor/state";
    doc["value_template"] = "{{ value_json.state }}";
    doc["cmd_t"] = "homeassistant/switch/well_monitor/set";
    serializeJson(doc, output);
    mqttPublish(topic, 0, false, output); // Publish a homeassistant config message for switch
    doc.clear();
}
