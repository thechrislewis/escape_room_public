#define ROOM "room"
#define NAME "rfid"
#define USEID true
#define VERSION "0.1"
#define SS_PIN D8 // change as required
#define RST_PIN D3 // change as required

const char *ssid = "***";
const char *password = "****";
const char *mqtt_server = "192.168.1.1";


#ifdef DEBUG_ON

#define DEBUGprint(x) Serial.print(x)
#define DEBUGprintln(x) Serial.println(x)

#else

#define DEBUGprint(x)
#define DEBUGprintln(x)

#endif

#include <ArduinoJson.h>
#include <SPI.h>
#include <MFRC522.h>

#include <ee_prop.h>

ee_prop prop(ROOM, NAME, USEID, VERSION);
MFRC522 mfrc522(SS_PIN, RST_PIN); // Instance of the class

enum msg_type
{
  BOOT,
  READING,
  STATE
};

typedef struct status_message
{
  msg_type type;
  bool present;
  char fw[6];
  char tag[12];
} status_message;

String lasttag;
String version = "No Sensor"; // default sensor state before detected

// Create a struct_message called sensor_msg
status_message sensor_msg;
unsigned long last_tag_read;

bool tagRead = false;
unsigned long updateInterval = 30000; // send status every 30 seconds

String dump_byte_array(byte *buffer, byte bufferSize)
{
  String cardId;
  byte start = 0;
  if (bufferSize > 4)
  {
    start = 1;
    bufferSize = 5;
  }
  for (byte i = start; i < bufferSize; i++)
  {

    cardId += String((buffer[i] < 0x10 ? " 0" : " "));
    cardId += String(buffer[i], HEX);
  }

  cardId.trim();
  cardId.toUpperCase();

  return cardId;
}

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

    DEBUGprint("processMSG: ");
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

    if (doc.containsKey("disable"))
    {
      disable();
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

void sendStatus()
{
  // send status every updateInterval ms
  static unsigned long lastStatus = 0;
  unsigned long now = millis();

  if (now - lastStatus > updateInterval)
  {
    lastStatus = now;
    DynamicJsonDocument doc(256);
    doc["info"]["fw"] = sensor_msg.fw;
    doc["info"]["tag"] = tagRead;
    prop.sendMQTT(prop.getPubTopic(), doc, false);
  }
}

// activate function
void activate()
{
  enable();
  unsolve();
  initReader();

  prop.setActive(true);
}

void deactivate()
{
  prop.setActive(false);
}

void enable()
{
  prop.setEnabled(true);
}

void disable()
{
  deactivate();
  prop.setEnabled(false);
}

void solve()
{
  prop.setSolved(true);
  prop.setActive(false); // deactivate sensor
}

void unsolve()
{
  prop.setSolved(false);
}

void initReader()
{
  // Init MFRC522

  memset(&sensor_msg, 0, sizeof(sensor_msg));
  
  sensor_msg.present = false; //just to be sure :-)

  mfrc522.PCD_Init(); // Init MFRC522

  // set antenna power to max
  mfrc522.PCD_ClearRegisterBitMask(mfrc522.RFCfgReg, (0x07 << 4));
  mfrc522.PCD_SetRegisterBitMask(mfrc522.RFCfgReg, (0x00 << 4));

  delay(4);
  mfrc522.PCD_ClearRegisterBitMask(mfrc522.RFCfgReg, (0x07 << 4));
  mfrc522.PCD_SetRegisterBitMask(mfrc522.RFCfgReg, (0x07 << 4));

  byte v = mfrc522.PCD_ReadRegister(MFRC522::VersionReg);

  // Lookup which version

  switch (v)
  {
  case 0x88:
    version = "clone";
    break;
  case 0x90:
    version = "0.0";
    break;
  case 0x91:
    version = "1.0";
    sensor_msg.present = true;
    break;
  case 0x92:
    version = "2.0";
    sensor_msg.present = true;
    break;
  case 0x12:
    version = "fake";
    break;
  case 0x00:
    version = "fail";
    break;
  case 0xFF:
    version = "fail";
    break;
  default:
    version = "reset";
  }

  strcpy(sensor_msg.fw, version.c_str());
  sendStatus();

  DEBUGprint("MFRC522 version: ");
  DEBUGprintln(version);


}

void setup()
{
  
  Serial.begin(115200);
  delay(1000);

  SPI.begin(); // Init SPI bus

  prop.begin(ssid, password, mqtt_server);
  prop.addCallback(process_message);
  prop.setUpdateInterval(30000); // internal in ms to send 'up' message

  // initialise prop specific items ( tones, leds, displays etc)

  initReader(); // initialise the reader

}

unsigned long timer1;
void loop()
{

  DynamicJsonDocument doc(256);
  String cardId;

  prop.loop();

  if (prop.isEnabled())
  {
    sendStatus();
  }

  if (prop.isActive())
  {

    // Verify if the UID has been read

    if (mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial())
    {
      last_tag_read = millis();
      if (tagRead == false)
      {
        tagRead = true;
        cardId = dump_byte_array(mfrc522.uid.uidByte, mfrc522.uid.size); // extract cardId

        DEBUGprint("tag detected : ");
        DEBUGprintln(cardId.c_str());

        doc["tag"] = cardId.c_str();
        prop.sendMQTT(prop.getPubTopic(), doc, false);
      }
    }
    else
    {
      if (tagRead && (millis() > last_tag_read + 300))
      {
        tagRead = false;
        DEBUGprintln("no tag");
        doc["tag"] = "00 00 00 00";
        prop.sendMQTT(prop.getPubTopic(), doc, false);
      }
    }
  }
}
