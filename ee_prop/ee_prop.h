#ifndef EE_PROP_H
#define EE_PROP_H

#define MAX_TOPICS 10
#define CORE 1 // define which core to run on if ESP32

// #include <Arduino.h>
#include <ArduinoJson.h>

#include "PubSubClient.h"

#ifdef ESP8266
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>

#else
#include "WiFi.h"
#include <WebServer.h>

#endif

#include "WiFiClient.h"
#include <ElegantOTA.h>

class ee_prop
{

private:
    String _name;
    String _room;
    String chipId;
    String _version;

    char lwtTopic[32];
    char pubTopic[32];
    char subTopic[32];
    char telTopic[32];

    char topics[MAX_TOPICS][32];
    int topicCount;

    WiFiClient _wifi_client;
    PubSubClient _mqtt_client;

    unsigned long updateInterval;

    bool _Active;
    bool _Solved;
    bool _Enabled;

    // user callback pointer
    void (*_myCallback)(char *, byte *, unsigned int);

    // function pointer for adding user status callback using json object
    void (*_myStatusCallback)(JsonObject &);

public:
    ee_prop(const char *room, const char *name, bool useId, const char *version);
    // ee_prop(const char *name, bool useId, const char *version);

    ~ee_prop();

    void begin(const char *ssid, const char *password, const char *mqtt_server);
    void reconnect();
    void loop();

    // callback function for MQTT
    void callback(char *topic, byte *payload, unsigned int length);

    // add user callback function
    void addCallback(void (*myCallback)(char *, byte *, unsigned int));
    // send boot
    void sendBoot();

    // send status
    void sendStatus();

    // send MQTT
    void sendMQTT(char *topic, DynamicJsonDocument &doc, bool retain);
    // update interval
    void setUpdateInterval(unsigned long interval);
    // getPubTopic
    char *getPubTopic();

    // add subscribe
    void subscribe(char *topic);

    // set puzzle active
    void setActive(bool active);
    // set puzzle solved
    void setSolved(bool solved);
    // set puzzle enabled
    void setEnabled(bool enabled);

    // get puzzle active
    bool isActive();
    // get puzzle solved
    bool isSolved();
    // get puzzle enabled
    bool isEnabled();
};

#endif