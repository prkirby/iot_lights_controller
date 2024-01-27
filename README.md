# ESP32 String-lights controller

## MQTT Enabled ESP32/arduino controller for generic, switching phase LED string lights


### For OTA upload of firmware, use the following:

- Ensure you have platformio cli installed
- Copy and setup the otaconfig.ini file
- **UPDATE THE LIGHTS_ID** in config.h before building and uploading
- Would like to build script that auto uploads with prompts for light IDs

`pio run -t upload --upload-port {HOSTNAME}.local`
