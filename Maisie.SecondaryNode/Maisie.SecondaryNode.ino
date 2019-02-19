/*
    Project Maisie, Seconday Node code.
    Muntadhar Haydar (@sparkist97).
    Created on March 16th, 2018, 1520.

    Important:
        message format is: [targetId]-[sourceId]-[data]
        data should be [nodeId*],temp,hum,co2,soil,water_temp,water_level
        *The Id of the node which the readings are its.
    
*              3V3 or 5V--------VCC   (3.3V to 7V in)
*              pin D8-----------CE    (chip enable in)
*           SS pin D10----------CSN   (chip select in)
*          SCK pin D13----------SCK   (SPI clock in)
*         MOSI pin D11----------SDI   (SPI Data in)
*         MISO pin D12----------SDO   (SPI data out)
*                               IRQ   (Interrupt output, not connected)
*                  GND----------GND   (ground in)

    Water Temp Sensor, DS18B20.
    VCC, red wire, goes to VCC.
    GND, black wire, goes to GND.
    Data, blue wire, goes to the end of a ~4.7kOhms resistor, which has its other end connected to Vcc.
        The joint between the resistor and the Data wire goes to Digtial Pin #2.
*/

#include <SPI.h>
#include <RH_NRF24.h>
#include <dht.h>
#include <MQ135.h>
#include <DallasTemperature.h>

#define DEBUG

#define NODE_ID 1 //Node Zero is the Primary (master) node.
#define ONE_WIRE_BUS 2
#define DHT22_PIN 3
#define MQ135_PIN A0
#define SOIL_SENSOR_PIN A1
#define WATER_TEMP_SENSOR_PIN A2
#define WATER_LEVEL_SENSOR_PIN A3
#define REQUEST_DATA_COMMAND "SEND_YOUR_READINGS"
#define MILLIS_TO_DROP_PREV_PACKET 5000

// Singleton instance of the radio driver
RH_NRF24 nrf24;
dht DHT;
MQ135 mq = MQ135(MQ135_PIN);
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
unsigned long timer = 0, dropTimer = 0;
bool msgToSend = false, resetData = true;
char pendingMsg[RH_NRF24_MAX_MESSAGE_LEN];
String data = "", sourceId = "", targetId = "";

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
    sensors.begin();
}

void loop()
{
    if (!resetData && millis() - dropTimer >= MILLIS_TO_DROP_PREV_PACKET)
    {
        resetData = true;
    }
    uint8_t buf[RH_NRF24_MAX_MESSAGE_LEN];
    uint8_t len = sizeof(buf);
    if (nrf24.recv(buf, &len))
    {
#ifdef DEBUG
        Serial.println((char *)buf);
#endif
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
            if (sourceId != String(NODE_ID + 1) && sourceId != String(NODE_ID - 1))
            {
                return;
            }
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
        else if (buf[0] == '+' && !resetData)
        {
            i++;
            String str = String((char *)buf);
            if (str[str.length() - 2] == '+')
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
            if (data == REQUEST_DATA_COMMAND)
            {
                String msg = String(NODE_ID - 1) + "-" + String(NODE_ID) + "-" + SensorsReadings();
                SendData(msg);
                msg = String(NODE_ID + 1) + "-" + String(NODE_ID) + "-" + REQUEST_DATA_COMMAND;
                SendData(msg);
            }
            else
            {
                String msg = String(NODE_ID - 1) + "-" + String(NODE_ID) + "-" + data;
                SendData(msg);
            }
            data = targetId = sourceId = "";
        }
    }
    if (nrf24.available())
    {
    }
}

String SensorsReadings()
{
    String data = String(NODE_ID) + ",";
    int dhtCheck = DHT.read22(DHT22_PIN);
    if (dhtCheck == DHTLIB_OK)
    {
        data += String(DHT.temperature) + "," + String(DHT.humidity);
    }
    else
    {
        data += "T,H";
    }
    sensors.requestTemperatures();
    float co2 = mq.getCorrectedPPM(DHT.temperature, DHT.humidity);
    data += "," + String(co2);
    data += "," + String(analogRead(SOIL_SENSOR_PIN) / 10);
    data += "," + String(sensors.getTempCByIndex(0)); // placeholder... analogRead(WATER_TEMP_SENSOR_PIN) / 100; //Placeholder...
    data += ",1";                                     // placeholder... Water level reading.
    return data;
}

void SendData(String data)
{
#ifdef DEBUG
    Serial.print("data: ");
    Serial.print(data);
    Serial.print("\tLength: ");
    Serial.println(data.length());
#endif
    if (data.length() < RH_NRF24_MAX_MESSAGE_LEN)
    {
        while (data.length() < RH_NRF24_MAX_MESSAGE_LEN)
            data += "!";
        SendMessage(data.c_str(), data.length());
    }
    else
    {
        int total = (data.length() / RH_NRF24_MAX_MESSAGE_LEN);
        int index = 0;
        for (int i = 0; i <= total; i++)
        {
            int a = data.length() - index + 3;
            int len = a < RH_NRF24_MAX_MESSAGE_LEN ? a : RH_NRF24_MAX_MESSAGE_LEN;
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
    Serial.print("Sending: ");
    Serial.print(len);
    Serial.print("\t");
    Serial.println((char *)msg);
#endif
    nrf24.send((uint8_t *)msg, len);
    nrf24.waitPacketSent();
}