/* mqtt.h */
#ifndef MQTT_H
#define MQTT_H

// #define MQTT_HOST IPAddress(192, 168, 1, 10)
#define MQTT_HOST "homeassistant.local"
#define MQTT_PORT 1883

#define ESPHOME_TOPIC_CONFIG "homeassistant/binary_sensor/well/config"
#define ESPHOME_CONFIG_PAYLOAD "{\"name\": \"well\", \"device_class\": \"binary_sensor\", \"state_topic\": \"homeassistant/binary_sensor/well/state\"}"

void setupMQTT();

uint16_t mqttPublish(const char* topic, uint8_t qos, bool retain, const char* payload);

uint16_t mqttLog(const char* msg);

#endif /* !MQTT_H */