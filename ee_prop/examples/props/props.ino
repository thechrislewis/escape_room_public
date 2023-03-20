/*
Changelog.
v 0.1 initial release
v 0.2 Moved more status messages to library. user status now only sent if prop.enabled is true.

*/

#define ROOM "room"
#define NAME "template"
#define USEID true
#define VERSION "0.2"
#define DEBUG_ON

// change as required for your network
const char *ssid = "***";
const char *password = "****";
const char *mqtt_server = "192.168.1.1";

// #define USE_BUZZER

#ifdef DEBUG_ON

#define DEBUGprint(x) Serial.print(x)
#define DEBUGprintln(x) Serial.println(x)

#else

#define DEBUGprint(x)
#define DEBUGprintln(x)

#endif

#include <ArduinoJson.h>
#include "config.h"
#include <ee_prop.h>

#ifdef USE_BUZZER
#define BUZZERPIN 25

#ifdef ESP32
#include <ESP32Servo.h> //for tones
#endif                  // ESP32

#endif // USE_BUZZER

#ifdef USE_BUTTONS
#include <Button2.h>
#endif

ee_prop prop(ROOM, NAME, USEID, VERSION);
unsigned long updateInterval = 30000; // interval in ms to send status message

// callback function for MQTT messages
// this is called when a message is received on the subscribed topic

void process_message(char *topic, byte *payload, unsigned int length)
{

  DynamicJsonDocument doc(256);

#ifdef DEBUG_ON

  Serial.print("MQTT processing message [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++)
  {
    Serial.print((char)payload[i]);
  }
  Serial.println();
#endif

  DeserializationError err = deserializeJson(doc, payload, length);
  if (!err)
  {

    DEBUGprint("JSON Message received: ");
    DEBUGprintln(doc.as<String>());

    if (doc.containsKey("activate"))
    {
      int option = doc["activate"];
      if (option == 1)
      {
        activate();
      }
      else
      {
        deactivate();
      }
      return; // no further processing reqd
    }

    if (doc.containsKey("enable"))
    {
      int option = doc["enable"];
      if (option == 1)
      {
        enable();
      }
      else
      {
        disable();
      }
      return; // no further processing reqd
    }

    if (doc.containsKey("solve"))
    {
      int option = doc["solve"];
      if (option == 1)
      {
        solve();
      }
      else
      {
        unsolve();
      }
      return; // no further processing reqd
    }

    if (doc.containsKey("status"))
    {
      sendStatus();
      return; // no further processing reqd
    }

    if (doc.containsKey("update"))
    {
      DEBUGprintln("update received");
      unsigned long interval = doc["update"];

      prop.setUpdateInterval(interval);
      return; // no further processing reqd
    }
  }
}

// send status message - this is independent of status from prop library
void sendStatus()
{

  DynamicJsonDocument doc(256);
  // user information specific to this prop eg peripheral information such as rfid fw version
  doc["device"]["info"] = "not found";

#ifdef USE_BUZZER
  doc["info"]["buzzer"] = true;
#endif

  prop.sendMQTT(prop.getPubTopic(), doc, false);
}

// activate function
void activate()
{
  // do stuff to activate prop
  // set/reset any variables
  // re-initialise any peripherals if required
  prop.setActive(true);
}

void deactivate()
{
  // opposite of activate
  prop.setActive(false);
}

void enable()
{
  // do stuff to enable prop
  // eg turn on leds, displays etc
  prop.setEnabled(true);
}

void disable()
{
  // do stuff to disable prop
  // eg turn off leds, displays etc
  prop.setEnabled(false);
}

void solve()
{
  // do stuff on solve
  // eg. turn leds green, update displays etc
  prop.setSolved(true);
  deactivate();
}

void unsolve()
{
  prop.setSolved(false);
}

// setup function
void setup()
{
  Serial.begin(115200);
  delay(1000);

  prop.begin(ssid, password, mqtt_server);
  prop.addCallback(process_message);
  prop.setUpdateInterval(updateInterval);

  // initialise prop specific items ( tones, leds, displays etc)
  // eg. pinMode(ledPin, OUTPUT);
  
}

void loop()
{

  static unsigned long timer1 = 0;

#ifdef ESP8266 // only required for ESP8266 - ESP32 is handled by the library
  prop.loop();
#endif

  if (prop.isActive())
  {
    // do stuff when puzzle is active
  }

  if (prop.isEnabled() && millis() > timer1)
  {
    timer1 = millis() + updateInterval;
    sendStatus();
  }
}
