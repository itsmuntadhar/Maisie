/*
    Project Maisie, Seconday Node code.
    Muntadhar Haydar (@sparkist97).
    Created on March 16th, 2018, 1632.

    
*              3V3 or 5V--------VCC   (3.3V to 7V in) 2
*              pin D4-----------CE    (chip enable in) 3
*           SS pin D2-----------CSN   (chip select in) 4
*          SCK pin D5-----------SCK   (SPI clock in) 5
*         MOSI pin D7-----------SDI   (SPI Data in) 6
*         MISO pin D6-----------SDO   (SPI data out) 7
*                               IRQ   (Interrupt output, not connected)
*                  GND----------GND   (ground in) 1
*/

#include <SPI.h>
#include <RH_NRF24.h>
#include <EEPROM.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WebServer.h>

#define DEBUG

#ifdef DEBUG
#define BASE_URL "http://maisie.muntadhar.net/api/marshes/0/data/"
#else
#define BASE_URL "http://maisie.muntadhar.net/api/marshes/0/data/"
#endif
#define SSID "Maisie-MasterNode-Setup"
#define KEY "AryaStark"

#define ID "AUserHasNoID"
#define SECRET "Valar_Morghulis"
#define MARSH_ID 0

#define SSID_CHECK_ADDR 0x0F // This byte in the EEPROM should be 1 if there's an SSID saved by the client.
#define SSID_ADDR 0x10       // Where the saved client's ssid is saved at the EEPROM.
#define KEY_ADDR 0x50        //...
#define NODES_COUNT_CHECK_ADDR 0x9F
#define NODES_COUNT_ADDR 0xA0
#define BROADCAST_WIFI 0x0
#define CONNECT_TO_WIFI 0x1

#define REQUEST_DATA_COMMAND "SEND_YOUR_READINGS"
#define NODE_ID 0                                 //Node Zero is the Primary (master) node.
#define MILLIS_BETWEEN_DATA_COLLECTIONS 60 * 1000 //Must be something reasonable and related to the NODES_COUNT.
#define MILLIS_TO_DROP_PREV_PACKET 5000

HTTPClient httpClient;
String ssid = "", key = "", data = "", targetId = "", sourceId = "";
bool resetData = true, isWiFiBroadcasted = true;
;
unsigned long timer = 0, dropTimer = 0;
byte nodesCount = 1;
int millisBetweenDataCollections;
RH_NRF24 nrf24(2, 4);
ESP8266WebServer server(80);

void setup()
{
#ifdef DEBUG
  Serial.begin(115200);
#endif
  if (!nrf24.init())
  {
#ifdef DEBUG
    Serial.println("init failed");
#endif
    while (1)
      ;
  }
#ifdef DEBUG
  Serial.println("init succeeded");
#endif
  // Defaults after init are 2.402 GHz (channel 2), 2Mbps, 0dBm
  if (!nrf24.setChannel(1))
  {
#ifdef DEBUG
    Serial.println("setChannel failed");
#endif
    while (1)
      ;
  }
#ifdef DEBUG
  Serial.println("setChannel succeeded");
#endif
  if (!nrf24.setRF(RH_NRF24::DataRate2Mbps, RH_NRF24::TransmitPower0dBm))
  {
#ifdef DEBUG
    Serial.println("setRF failed");
#endif
    while (1)
      ;
  }
#ifdef DEBUG
  Serial.println("setRF succeeded");
#endif
  EEPROM.begin(512);
  start();
}

void loop()
{
  server.handleClient();
  if (!resetData && millis() - dropTimer >= MILLIS_TO_DROP_PREV_PACKET)
  {
    resetData = true;
  }
  if (nrf24.available())
  {
    uint8_t buf[RH_NRF24_MAX_MESSAGE_LEN];
    uint8_t len = sizeof(buf);
    if (nrf24.recv(buf, &len))
    {
      if (resetData)
      {
        data = "";
        targetId = "";
        sourceId = "";
      }
      byte i = 0;
      if (buf[0] != '+')
      {
        dropTimer = millis();
        while (buf[i] != '-') // Look for '-'
          targetId += String((char)buf[i++]);
#ifdef DEBUG
        Serial.print("Target Id: ");
        Serial.print(targetId);
#endif
        if (targetId != String(NODE_ID)) //it isn't ment for this particular one.
        {
          return;
        }
        i++;
        while (buf[i] != '-') // Look for '-'
          sourceId += String((char)buf[i++]);
#ifdef DEBUG
        Serial.print("\tSource Id: ");
        Serial.print(sourceId);
#endif
        if (sourceId != String(NODE_ID + 1))
        {
          return;
        }
        i++;
        String str = String((char *)buf);
        if (str[str.length() - 1] == '+')
        {
          while (buf[i] != '+')
          {
            data += String((char)buf[i++]);
          }
          resetData = false;
        }
        else
        {
          while (buf[i] != '!')
          {
            data += String((char)buf[i++]);
          }
          resetData = true;
        }
      }
      else if (buf[0] == '+' && !resetData)
      {
        i++;
        if (buf[len - 1] == '+')
        {
          while (buf[i] != '+')
          {
            data += String((char)buf[i++]);
          }
          resetData = false;
        }
        else
        {
          while (buf[i] != '!')
          {
            data += String((char)buf[i++]);
          }
          resetData = true;
        }
      }
      if (resetData)
      {
#ifdef DEBUG
        Serial.print("\tData: ");
        Serial.println(data);
#endif
        if (!isWiFiBroadcasted)
          PostData(data);
      }
    }
  }

  if (millis() - timer > millisBetweenDataCollections && resetData)
  {
    String msg = String(NODE_ID + 1) + String("-") + String(NODE_ID) + String("-") + REQUEST_DATA_COMMAND;
    SendData(msg);
    timer = millis();
  }
}

void start()
{
  if (readFromEEPROM(NODES_COUNT_CHECK_ADDR) != 0xAA)
  {
    nodesCount = 1;
    writeToEEPROM(NODES_COUNT_ADDR, 1);
    writeToEEPROM(NODES_COUNT_CHECK_ADDR, 0xAA);
  }
  else
  {
    nodesCount = readFromEEPROM(NODES_COUNT_ADDR);
  }
#ifdef DEBUG
  Serial.print("nodes count: ");
  Serial.println(nodesCount);
#endif
  millisBetweenDataCollections = MILLIS_BETWEEN_DATA_COLLECTIONS * nodesCount + (10 * nodesCount);
  isWiFiBroadcasted = !isThereAnSSID();
  ssid = isWiFiBroadcasted ? SSID : getSSID();
  key = isWiFiBroadcasted ? KEY : getKey();
#ifdef DEBUG
  Serial.println("System has been started.");
  Serial.println(isWiFiBroadcasted ? "Broadcasting..." : "Connecting to ...");
  Serial.print(ssid);
  Serial.print("\t");
  Serial.println(key);
#endif
  if (isWiFiBroadcasted)
    broadcastWiFi(ssid, key);
  else
    connectToWiFi(ssid, key);
  server.on("/test", HTTP_GET, []() {
    server.send(200, "text/plain", "works");
  });
  server.on("/add_node", HTTP_POST, handleAddNode);
  server.on("/new_wifi", HTTP_POST, handleNewWiFi);
  server.begin();
}

void handleNewWiFi()
{
  if (server.args() < 2)
  {
    server.send(400, "text/plain", "Invalid length of content. There should be only SSID and WPA Key.");
    return;
  }
  if (server.arg(0).length() < 1 || server.arg(0).length() > 31)
  {
    server.send(400, "text/plain", "Invalid SSID length. SSID's length should be between 1 and 31 characters");
    return;
  }
  if (server.arg(1).length() < 8 || server.arg(1).length() > 63)
  {
    server.send(400, "text/plain", "Invalid Key length. Key's length should be between 8 and 63 characters");
    return;
  }
#ifdef DEBUG
  Serial.print(server.arg(0));
  Serial.print(";");
  Serial.print(server.arg(1));
  Serial.println(";");
#endif
  ssid = server.arg(0);
  key = server.arg(1);
  writeNewSSID(ssid, key);
  server.send(200, "text/plain", "New SSID & Key were saved!");
}
void handleAddNode()
{
  if (server.args() != 2 || !(server.argName(0) == "Id" && server.arg(0) == ID && server.argName(1) == "Secret" && server.arg(1) == SECRET))
  {
    server.send(400, "text/plain", "Check your provided data!");
    return;
  }
  else
  {
    writeToEEPROM(NODES_COUNT_ADDR, ++nodesCount);
    server.send(201, "text/plain", "A new node was added successfully!");
  }
}
void broadcastWiFi(String s, String k)
{
  WiFi.mode(WIFI_AP);
  IPAddress ip(192, 168, 18, 56);
  IPAddress gateway(192, 168, 18, 56);
  IPAddress subnet(255, 255, 255, 0);
  WiFi.softAPConfig(ip, gateway, subnet);
  WiFi.softAP(s.c_str(), k.c_str());
#ifdef DEBUG
  Serial.print("Broadcasting, SSID=");
  Serial.print(s);
  Serial.print(", KEY=");
  Serial.println(k);
#endif
}

void connectToWiFi(String s, String k)
{
#ifdef DEBUG
  Serial.print("Connecting to ");
  Serial.println(s);
#endif
  WiFi.mode(WIFI_STA);
  WiFi.begin(s.c_str(), k.c_str());
  byte loops = 0;
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
    loops++;

    if (loops >= 60)
    {
      broadcastWiFi(SSID, KEY);
      break;
    }
  }
  if (!isWiFiBroadcasted)
  {
#ifdef DEBUG
    Serial.println("");
    Serial.println("WiFi connected");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
#endif
  }
}

void writeNewSSID(String s, String k)
{
  saveSSID(s);
  saveKey(k);
  writeToEEPROM(SSID_CHECK_ADDR, 0x1);
}

String getSSID()
{
  return getStringFromEEPROM(SSID_ADDR, 64, ';');
}

String getKey()
{
  return getStringFromEEPROM(KEY_ADDR, 64, ';');
}

void saveSSID(String s)
{
  for (byte i = 0; i < s.length(); i++)
  {
    writeToEEPROM(SSID_ADDR + i, s.c_str()[i]);
  }
  writeToEEPROM(SSID_ADDR + s.length(), (byte)';');
}

void saveKey(String k)
{
  for (byte i = 0; i < k.length(); i++)
  {
    writeToEEPROM(KEY_ADDR + i, k.c_str()[i]);
  }
  writeToEEPROM(KEY_ADDR + k.length(), (byte)';');
}

bool isThereAnSSID()
{
  return readFromEEPROM(SSID_CHECK_ADDR) == 0x1;
}

String getStringFromEEPROM(byte addr, byte maxLen, char terminator)
{
  char buf[maxLen];
  for (byte i = 0; i < maxLen; i++)
  {
    char c = (char)readFromEEPROM(addr + i);
    if (c == terminator)
    {
      buf[i] = '\0';
      break;
    }
    buf[i] = c;
  }
  return String(buf);
}

void writeToEEPROM(byte addr, byte data)
{
  EEPROM.write(addr, data);
  EEPROM.commit();
}

byte readFromEEPROM(byte addr)
{
  return EEPROM.read(addr);
}

void PostData(String data)
{
  String url = BASE_URL;
  int i = 0;
  String nodeId = "";
  while (data.c_str()[i] != ',')
  {
    nodeId += String(data[i]);
    i++;
  }
  url += nodeId + "/write";
  String d = "{\"offset\": 0, \"values\": \"" + data.substring(i + 1) + "\"}"; //TOOD::add actual offset...
#ifdef DEBUG
  Serial.println(url);
  Serial.println(d);
#endif
  httpClient.begin(url);
  httpClient.addHeader("Content-Type", "application/json");
  int httpCode = httpClient.POST(d);
  httpClient.end();
}

void SendData(String data)
{
  if (data.length() < RH_NRF24_MAX_MESSAGE_LEN)
  {
    while (data.length() < RH_NRF24_MAX_MESSAGE_LEN)
      data += "!";
    SendMessage((char *)data.c_str(), data.length());
  }
  else
  {
    int total = (data.length() / RH_NRF24_MAX_MESSAGE_LEN);
    int index = 0;
    for (int i = 0; i <= total; i++)
    {
      int len = data.length() - index + 2 < RH_NRF24_MAX_MESSAGE_LEN ? data.length() - index + 2 : RH_NRF24_MAX_MESSAGE_LEN;
      char msg[len];
      int j = 0;
      if (i > 0)
      {
        msg[0] = '+';
        j = 1;
      }
      for (j; j < len - 2; j++)
      {
        msg[j] = data.c_str()[index++];
      }
      msg[len - 2] = i == total ? '!' : '+';
      msg[len - 1] = '\0';
      SendMessage(msg, len);
      delay(10);
    }
  }
}

void SendMessage(char msg[], int len)
{
#ifdef DEBUG
  Serial.print("\nSending: ");
  Serial.print(len);
  Serial.print("\t");
  Serial.println((char *)msg);
#endif
  nrf24.send((uint8_t *)msg, len);
  nrf24.waitPacketSent();
}