/*
inital code for ee_prop library 2nd version
*/

#include "ee_prop.h"

//#define DEBUG_ON

#ifdef DEBUG_ON

#define DEBUGprint(x) Serial.print(x)
#define DEBUGprintln(x) Serial.println(x)

#else

#define DEBUGprint(x)
#define DEBUGprintln(x)

#endif

// setup WebServer
#ifdef ESP8266
ESP8266WebServer server(80);
#else
WebServer server(80);
#endif

// constructor. Set name and flag to use chipId in topics
ee_prop::ee_prop(const char *room, const char *name, bool useId, const char *version)
{
    // set name
    _name = String(name);
    _version = String(version);
    _room = String(room);

    // set chipId

#ifdef ESP8266
    chipId = String(ESP.getChipId(), HEX);
#else
    uint64_t macAddress = ESP.getEfuseMac();
    uint64_t macAddressTrunc = macAddress << 40;
    uint32_t shortCode = macAddressTrunc >> 40;
    chipId = String(shortCode, HEX);
#endif
    updateInterval = 30000;

    chipId.toUpperCase();

    // set subscription topics

    sprintf(topics[topicCount++], "eeprop");
    sprintf(topics[topicCount++], "%s", _room.c_str());
    sprintf(topics[topicCount++], "/cmnd/%s", _name.c_str());
    sprintf(topics[topicCount++], "/cmnd/%s/%s", _name.c_str(), chipId.c_str());

    if (useId)
    {
        sprintf(lwtTopic, "/lwt/%s/%s", _name.c_str(), chipId.c_str());
        sprintf(pubTopic, "/stat/%s/%s", _name.c_str(), chipId.c_str());
        sprintf(telTopic, "/tele/%s/%s", _name.c_str(), chipId.c_str());
    }
    else
    {
        sprintf(lwtTopic, "/lwt/%s", _name.c_str());
        sprintf(pubTopic, "/stat/%s", _name.c_str());
        sprintf(telTopic, "/tele/%s", _name.c_str());
    }
}

ee_prop::~ee_prop()
{
    DEBUGprintln("ee_prop::~ee_prop");
}

void ee_prop::begin(const char *ssid, const char *password, const char *mqtt_server)
{
    // set WiFi Tx Power Level
    wifi_power_t txPower = WIFI_POWER_2dBm;

    DEBUGprint("NAME: ");
    DEBUGprintln(_name);
    DEBUGprint("VERSION: ");
    DEBUGprintln(_version);
    DEBUGprint("FILE: ");
    DEBUGprintln(__FILE__);
    DEBUGprint("SSID: ");
    DEBUGprintln(ssid);
    DEBUGprint("Password: ");
    DEBUGprintln(password);
    DEBUGprint("Chip ID: ");
    DEBUGprintln(chipId);

    // connect to WiFi. Reboot after 10 fail attempts
    WiFi.hostname(_name);
    WiFi.disconnect();
    WiFi.mode(WIFI_STA);

    DEBUGprint("WiFi Tx Power Level set to: ");

#ifdef ESP32
    DEBUGprintln(WiFi.getTxPower());
    WiFi.setTxPower(txPower);
    DEBUGprint("WiFi Tx Power Level changed to: ");
    DEBUGprintln(WiFi.getTxPower());
#else
    WiFi.setOutputPower(txPower);
    DEBUGprint("WiFi Tx Power Level changed to: ");
    DEBUGprintln(txPower);
#endif

    DEBUGprintln("Connecting to WiFi");

    WiFi.begin(ssid, password);
    int i = 0;

    while (WiFi.status() != WL_CONNECTED)
    {

        delay(100);
        DEBUGprint(".");
        i++;
        if (i > 30)
        {
            ESP.restart();
        }
    }

    // print connected

    DEBUGprintln("WiFi connected");
    DEBUGprint("IP address: ");
    DEBUGprintln(WiFi.localIP());

    // set up MQTT client
    _mqtt_client.setClient(_wifi_client);

    // connect to MQTT broker
    _mqtt_client.setServer(mqtt_server, 1883);

    //_mqtt_client.setCallback(callback);
    _mqtt_client.setCallback([this](char *topic, byte *payload, unsigned int length)
                             { this->callback(topic, payload, length); });

    // set keepalive
    _mqtt_client.setKeepAlive(30);

    reconnect();

    server.on("/", []()
              {
    String message = String(" <a href=\"") + String("/update\"> UPDATE </a>");
    server.send(200, "text/html", message.c_str()); });

    ElegantOTA.begin(&server); // Start ElegantOTA
    server.begin();
    DEBUGprintln("HTTP server started");

    // send boot info to MQTT broker
    sendBoot();

#ifdef ESP32

    // call loop as task
    xTaskCreatePinnedToCore(
        [](void *param)
        {
            for (;;)
            {
                ((ee_prop *)param)->loop();
                vTaskDelay(10);
            }
        },
        "ee_prop", 4096, this, 1, NULL, CORE);

#endif
}

void ee_prop::reconnect()
{
    int attempts = 0;
    // Loop until we're reconnected
    while (!_mqtt_client.connected() && attempts < 10)
    {
        DEBUGprint("Attempting MQTT connection...");
        // Attempt to connect
        if (_mqtt_client.connect(chipId.c_str(), "", "", lwtTopic, 0, true, "Offline"))
        {
            DEBUGprintln("connected");
            // Once connected, publish an announcement...
            _mqtt_client.publish(lwtTopic, "Online", true);
            DEBUGprintln("LWT sent");
            // ... and resubscribe
            // loop through topics and subscribe
            for (int i = 0; i < topicCount; i++)
            {
                _mqtt_client.subscribe(topics[i]);
                DEBUGprint("Subscribed to: ");
                DEBUGprintln(topics[i]);
            }
        }
        else
        {
            // wait 1 second before retrying or reboot after 10 fail attempts
            attempts++;
            DEBUGprint("failed, rc=");
            DEBUGprint(_mqtt_client.state());
            DEBUGprintln(" try again in 1 second");
            delay(1000);
        }

        // reboot after 10 fail attempts
        if (attempts >= 10)
        {
            ESP.restart();
        }
    }
}

void ee_prop::loop()
{
    static unsigned long sendStatusMillis = 0;
    static unsigned long lastMillis = 0;
    // check wifi connection and reconnect if needed
    if (WiFi.status() != WL_CONNECTED && millis() - lastMillis > 5000)
    {
        WiFi.disconnect();
        WiFi.reconnect();
        lastMillis = millis();
    }
    else
    {
        if (!_mqtt_client.connected())
        {
            reconnect();
        }
        _mqtt_client.loop();
    }
    // handle OTA
    server.handleClient();

    // send update every 30 seconds
    if (millis() > sendStatusMillis)
    {
        // create dynamic json object to send to broker
        DynamicJsonDocument doc(256);

        doc["sys"]["uptime"] = millis() / 1000; // uptime

// get core we're running on
#ifdef ESP32
        doc["sys"]["core"] = xPortGetCoreID();
#endif

        // call sendJson function to send json object to broker
        sendMQTT(pubTopic, doc, true);
        sendStatusMillis = millis() + updateInterval;
    }
}

// send esp boot info to MQTT broker
void ee_prop::sendBoot()
{
    // send name and wifi status using sendJson
    DEBUGprintln("Sending boot info");

    DynamicJsonDocument doc(256);
    doc["name"] = _name;
    doc["version"] = _version;
    doc["rssi"] = WiFi.RSSI();

#ifdef ESP32
    doc["TxPower"] = WiFi.getTxPower();
#endif

    doc["ssid"] = WiFi.SSID();
    doc["ipaddr"] = WiFi.localIP().toString();
    doc["id"] = chipId;

    // retain boot message
    sendMQTT(telTopic, doc, true);

    DEBUGprint("Status sent: ");
    DEBUGprintln(doc.as<String>());
}

// callback function for MQTT proceeses reboot command then calls user callback function
//  reboot command is in payload
void ee_prop::callback(char *topic, byte *payload, unsigned int length)
{

#ifdef DEBUG_ON
    DEBUGprint("Message arrived [");
    DEBUGprint(topic);
    DEBUGprint("] ");
    for (int i = 0; i < length; i++)
    {
        DEBUGprint((char)payload[i]);
    }
    DEBUGprintln();
#endif

    // reboot command
    if (strncmp((char *)payload, "reboot", 6) == 0)
    {
        DEBUGprintln("Rebooting...");
        // mqtt disconnect
        _mqtt_client.disconnect();
        // shutdown wifi
        WiFi.disconnect();
        WiFi.mode(WIFI_OFF);

        delay(1000);
        ESP.restart();
    }

    // call user callback function if exists
    if (_myCallback != NULL)
    {
        _myCallback(topic, payload, length);
    }
}

// add user callback function
void ee_prop::addCallback(void (*myCallback)(char *, byte *, unsigned int))
{
    _myCallback = myCallback;
}

// send json object to MQTT broker
// function needs to serialize json object to string
// using arduinojson v6 and above
void ee_prop::sendMQTT(char *topic, DynamicJsonDocument &doc, bool retain)
{
    String json;
    serializeJson(doc, json);
    _mqtt_client.publish(topic, json.c_str(), retain);
    DEBUGprint("JSON sent: ");
    DEBUGprintln(json);
}

// set update interval
void ee_prop::setUpdateInterval(unsigned long interval)
{
    updateInterval = interval;
    DEBUGprint("Update interval set to: ");
    DEBUGprintln(updateInterval);
}

// getPubTopic
char *ee_prop::getPubTopic()
{
    return pubTopic;
}

// subscribe() function
void ee_prop::subscribe(char *topic)
{
    _mqtt_client.subscribe(topic);
}

// setPuzzleActive
void ee_prop::setActive(bool active)
{
    _Active = active;
}

// getPuzzleActive
bool ee_prop::isActive()
{
    return _Active;
}

// setPuzzleSolved
void ee_prop::setSolved(bool solved)
{
    _Solved = solved;
}

// getPuzzleSolved
bool ee_prop::isSolved()
{
    return _Solved;
}

// setPuzzleEnabled
void ee_prop::setEnabled(bool enabled)
{
    _Enabled = enabled;
}

// getPuzzleEnabled
bool ee_prop::isEnabled()
{
    return _Enabled;
}
