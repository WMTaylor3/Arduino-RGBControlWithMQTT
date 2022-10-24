# Arduino-RGBControlWithMQTT
Controls a set of RGB LED lights, reports status via MQTT for Home Assistant integration and accepts potentiometer input for brightness control

This project requires a 'credentials.h' file in order to compile which, for obvious reasons, isn't uploaded here. A sample is included below so create and modify per your setup.

#define WIFI_NAME "My Wifi Name"
#define WIFI_PASSWORD "My Wifi Password"

#define MQTT_SERVER "IPv4 address of MQTT server"
#define MQTT_PORT 1883
#define MQTT_USER "MQTT username of this node. Used for auth. i.e. led-lights"
#define MQTT_PASSWORD "MQTT password of this node. Used for auth."
#define MQTT_ID "MQTT ID of this node."
#define MQTT_TOPIC "topic/for-this-node"
