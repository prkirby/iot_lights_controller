#ifndef CONFIG
#define CONFIG

#define LIGHTS_ID "LIVING_ROOM"

#define WIFI_NAME "wifi_name"
#define WIFI_PASS "wifi_pass"
#define MQTT_BROKER "192.168.xxx.xxx"
#define MQTT_PORT 1883
#define OTA_PASS "ota_pass"
#define OTA_PORT 3232

#endif

// For command line upload, use the following:
// Be sure setup the otaconfig.ini file
// And make sure to UPDATE THE LIGHTS_ID in config.h before uploading
// Future should make script that auto-updates that config
// pio run -t upload --upload-port {HOSTNAME}.local
