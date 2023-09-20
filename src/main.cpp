#include <Arduino.h>
#include "config.h"
#include <WiFi.h>
#include <ArduinoOTA.h>
#include <MQTT.h>
#include <Preferences.h>
#include "driver/mcpwm.h"

// Flash Memory API for ESP32
// https://randomnerdtutorials.com/esp32-save-data-permanently-preferences/
Preferences preferences;
const char preferencesNamespace[] = "IOT_LIGHTS";
unsigned int minPressure;
unsigned int maxPressure;
void initPreferences();

// Wifi Variables
const char ssid[] = WIFI_NAME;
const char pass[] = WIFI_PASS;
const char mqttBroker[] = MQTT_BROKER;
const char hostNamePrefix[] = "IOT_LIGHTS_";
const char lightsID[] = LIGHTS_ID;
const String hostName = String(hostNamePrefix) + String(lightsID);
const char otaPass[] = OTA_PASS;
const int otaPort = OTA_PORT;
WiFiClient net;
MQTTClient client;
unsigned long lastMillis = 0;
void mqttConnect();
void initWifi();
void initOta();
void initMqtt();
void otaLoop();
void mqttLoop();
void messageReceived(String &topic, String &payload);

// LED Vars
const int ledBankAPin = 25;
const int ledBankBPin = ;
bool ledEnabled = false;
int ledDuty = 50; // 0 - 50
int minSinDuty = 0;
int maxSinDuty = 50;
int curSinDuty = 0;
int curAnimMillis = 0;
int prevAnimMillis = 0;
int animationTime = 4000;
bool ledAnimEnabled = false;
int curAnimTime = 0;
void handleLeds();
void setDutyCycle(int duty);
double getCurRads();
int sinDimMap(double rads);
void initLeds();

// Status Update
const uint statusInterval = 3000;
unsigned long statusPrevMillis = 0;
void sendStatus();

/**
 * @brief
 *
 */
void setup()
{
  Serial.begin(115200);
  while (!Serial)
    delay(100); // wait for native usb
  Serial.println("IOT Lights Controller");

  delay(50);

  initPreferences();

  initWifi();
  initOta();
  initMqtt();
  initLeds();
  sendStatus();
}

/**
 * @brief
 *
 */
void loop()
{
  if ((WiFi.status() != WL_CONNECTED))
  {
    Serial.println("Disconnected from Wifi. Restarting...");
    ESP.restart();
  }
  otaLoop();
  mqttLoop();
  handleLeds();

  if (millis() - statusPrevMillis > statusInterval)
  {
    sendStatus();
    statusPrevMillis = millis();
  }
}

/**
 * @brief
 *
 */
void initPreferences()
{
  preferences.begin(preferencesNamespace, false);
  ledEnabled = preferences.getBool("ledEnabled", false);
  ledDuty = preferences.getUInt("ledDuty", 50); // Default LED Duty
  ledAnimEnabled = preferences.getBool("ledAnimEnabled", false);
  minSinDuty = preferences.getUInt("minSinDuty", 0);
  maxSinDuty = preferences.getUInt("maxSinDuty", 50);
  animationTime = preferences.getUInt("animationTime", 4000);

  Serial.print("LED State From Preferences: ");
  Serial.println(ledEnabled);
  Serial.print("LED Duty From Preferences: ");
  Serial.println(ledDuty);
  Serial.print("LED Anim Enabled From Preferences: ");
  Serial.println(ledAnimEnabled);
  Serial.print("Min Sin Duty From Preferences: ");
  Serial.println(minSinDuty);
  Serial.print("Max Sin Duty From Preferences: ");
  Serial.println(maxSinDuty);
  Serial.print("LED Anim Time From Preferences: ");
  Serial.println(animationTime);
}

/**
 * @brief
 *
 */
void initWifi()
{

  WiFi.mode(WIFI_STA);
  WiFi.setHostname(hostName.c_str());
  WiFi.begin(ssid, pass);
  Serial.print("checking wifi...");
  unsigned long wifiWaitTime = 120000; // Wait two minutes before restarting
  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print(".");
    delay(1000);
    if (millis() >= wifiWaitTime)
    {
      Serial.print("Unable to connect to wifi. Restarting...");
      ESP.restart();
    }
  }
  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

/**
 * @brief
 *
 */
void initOta()
{
  /* create a connection at port 3232 */
  ArduinoOTA.setPort(otaPort);
  /* we use mDNS instead of IP of ESP32 directly */
  ArduinoOTA.setHostname(hostName.c_str());

  /* we set password for updating */
  ArduinoOTA.setPassword(otaPass);

  /* this callback function will be invoked when updating start */
  ArduinoOTA.onStart([]()
                     { Serial.println("Start updating"); });
  /* this callback function will be invoked when updating end */
  ArduinoOTA.onEnd([]()
                   { Serial.println("\nEnd updating"); });
  /* this callback function will be invoked when a number of chunks of software was flashed
  so we can use it to calculate the progress of flashing */
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total)
                        { Serial.printf("Progress: %u%%\r", (progress / (total / 100))); });

  /* this callback function will be invoked when updating error */
  ArduinoOTA.onError([](ota_error_t error)
                     {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed"); });
  /* start updating */
  ArduinoOTA.begin();
}

/**
 * @brief
 *
 */
void initMqtt()
{
  // Note: Local domain names (e.g. "Computer.local" on OSX) are not supported
  // by Arduino. You need to set the IP address directly.
  client.begin(mqttBroker, net);
  client.onMessage(messageReceived);

  mqttConnect();
}

/**
 * @brief
 *
 */
void mqttConnect()
{

  Serial.print("\nconnecting...");
  while (!client.connect(hostName.c_str(), "public", "public"))
  {
    Serial.print(".");
    delay(1000);
  }

  Serial.println("\nconnected!");

  client.subscribe("/hello");
  client.subscribe("/ledEnable");
  client.subscribe("/ledDisable");
  client.subscribe("/setLedDuty");
  client.subscribe("/ledAnimEnable");
  client.subscribe("/ledAnimDisable");
  client.subscribe("/setMinSinDuty");
  client.subscribe("/setMaxSinDuty");
  client.subscribe("/setAnimTime");
}

/**
 * @brief
 *
 * @param topic
 * @param payload
 */
void messageReceived(String &topic, String &payload)
{
  // Serial.println("incoming: " + topic + " - " + payload);

  // Note: Do not use the client in the callback to publish, subscribe or
  // unsubscribe as it may cause deadlocks when other things arrive while
  // sending and receiving acknowledgments. Instead, change a global variable,
  // or push to a queue and handle it in the loop after calling `client.loop()`.
  if (topic == "/ledEnable")
  {
    ledEnabled = true;
    // Serial.println("LED Enabled");
    preferences.putBool("ledEnabled", ledEnabled);
    // client.publish("/ledState", String(ledEnabled));
  }
  else if (topic == "/ledDisable")
  {
    ledEnabled = false;
    // Serial.println("LED Disabled");
    preferences.putBool("ledEnabled", ledEnabled);
    // client.publish("/ledState", String(ledEnabled));
  }
  else if (topic == "/setLedDuty")
  {
    // Serial.print("New LED Duty: ");
    // Serial.println(payload);
    ledDuty = payload.toInt();
    preferences.putUInt("ledDuty", ledDuty);
    // client.publish("/ledDutyState", String(ledDuty));
  }
  else if (topic == "/ledAnimEnable")
  {
    // Serial.println("LED Animation Enabled");
    ledAnimEnabled = true;
    preferences.putBool("ledAnimEnabled", ledAnimEnabled);
    // client.publish("/ledAnimState", String(ledAnimEnabled));
  }
  else if (topic == "/ledAnimDisable")
  {
    // Serial.println("LED Animation Disabled");
    ledAnimEnabled = false;
    preferences.putBool("ledAnimEnabled", ledAnimEnabled);
    // client.publish("/ledAnimState", String(ledAnimEnabled));
  }
  else if (topic == "/setMinSinDuty")
  {
    // Serial.print("New min sin Duty: ");
    // Serial.println(payload);
    minSinDuty = payload.toInt();
    preferences.putUInt("minSinDuty", minSinDuty);
    // client.publish("/minSinDutyState", String(minSinDuty));
  }
  else if (topic == "/setMaxSinDuty")
  {
    // Serial.print("New max sin Duty: ");
    // Serial.println(payload);
    maxSinDuty = payload.toInt();
    preferences.putUInt("maxSinDuty", maxSinDuty);
    // client.publish("/maxSinDutyState", String(maxSinDuty));
  }
  else if (topic == "/setAnimTime")
  {
    // Serial.print("New animation time: ");
    // Serial.println(payload);
    animationTime = payload.toInt();
    preferences.putUInt("animationTime", animationTime);
    // client.publish("/animTimeState", String(animationTime));
  }

  // Debounce send status after new input received
  statusPrevMillis = millis();
}

/**
 * @brief
 *
 */
void otaLoop()
{
  /* this function will handle incomming chunk of SW, flash and respond sender */
  ArduinoOTA.handle();
}

/**
 * @brief
 *
 */
void mqttLoop()
{
  client.loop();
  delay(5); // <- fixes some issues with WiFi stability

  if (!client.connected())
  {
    mqttConnect();
  }
}

/**
 * @brief
 *
 */
void sendStatus()
{
  client.publish("/ledState", String(ledEnabled));
  client.publish("/ledDutyState", String(ledDuty));
  client.publish("/ledAnimState", String(ledAnimEnabled));
  client.publish("/minSinDutyState", String(minSinDuty));
  client.publish("/maxSinDutyState", String(maxSinDuty));
  client.publish("/animTimeState", String(animationTime));
}

/**
 * LED Initialization
 * https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/ledc.html#led-control-ledc
 */
void initLeds()
{

  printf("Configuring Initial Parameters of mcpwm...\n");

  mcpwm_gpio_init(MCPWM_UNIT_0, MCPWM0A, ledBankAPin);
  mcpwm_gpio_init(MCPWM_UNIT_0, MCPWM1A, ledBankBPin);

  mcpwm_config_t pwm_config;
  pwm_config.frequency = 2000; // frequency = 20000Hz
  pwm_config.cmpr_a = 50;      // duty cycle of PWMxA = 60.0%
  pwm_config.cmpr_b = 50;      // duty cycle of PWMxb = 50.0%
  pwm_config.counter_mode = MCPWM_UP_COUNTER;
  pwm_config.duty_mode = MCPWM_DUTY_MODE_0;
  mcpwm_init(MCPWM_UNIT_0, MCPWM_TIMER_0, &pwm_config);
  mcpwm_init(MCPWM_UNIT_0, MCPWM_TIMER_1, &pwm_config);
  // delay(20);

  // Enable Software Sync output from Timer 0
  mcpwm_set_timer_sync_output(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_SWSYNC_SOURCE_TEZ);

  // Configure Timer 1 to listen on Timer 0 sync signal, 180 out of phase
  mcpwm_sync_config_t sync_config;
  sync_config.sync_sig = MCPWM_SELECT_TIMER0_SYNC;
  sync_config.timer_val = 500;
  sync_config.count_direction = MCPWM_TIMER_DIRECTION_UP;

  mcpwm_sync_configure(MCPWM_UNIT_0, MCPWM_TIMER_1, &sync_config);

  printf("mcpwm config finished...\n");
}

/**
 * @brief Set the Duty Cycle object
 *
 */
void setDutyCycle(int duty)
{
  mcpwm_set_duty(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_GEN_A, duty);
  mcpwm_set_duty(MCPWM_UNIT_0, MCPWM_TIMER_1, MCPWM_GEN_A, duty);
}

/**
 *
 */
void handleLeds()
{
  if (!ledEnabled)
  {
    setDutyCycle(0);
    return;
  }

  if (ledAnimEnabled)
  {
    curAnimMillis = millis();
    if (curAnimMillis - prevAnimMillis > animationTime)
    {
      prevAnimMillis = curAnimMillis;
    }
    curAnimTime = curAnimMillis - prevAnimMillis;
    curSinDuty = sinDimMap(getCurRads());
    // Serial.print("Cur sin Duty: ");
    // Serial.println(curSinDuty);
    setDutyCycle(curSinDuty);
    return;
  }

  setDutyCycle(ledDuty);
  return;
}

/**
 * @brief Get the Cur Rads based on animation percent
 *
 * @return double
 */
double getCurRads()
{
  double animPercent = double(curAnimTime) / double(animationTime);
  return animPercent * PI;
}

/**
 * @brief Map the diming value based on a sin wav function
 * Should make this float for better fading, but need to get rid of map
 * @return int
 */
int sinDimMap(double rads)
{
  double sinVal = sin(rads);
  if (sinVal < 0)
    sinVal *= -1;
  return map(double(50.0 * sinVal), 0.0, 50.0, minSinDuty, maxSinDuty);
}