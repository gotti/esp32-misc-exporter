#include <Arduino.h>
#include "MHZ19_uart.h"
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <ATC_MiThermometer.h>
#include "secret.hpp"

const int rx_pin = 16; // Serial rx pin no
const int tx_pin = 17; // Serial tx pin no

const char *co2_html_template = "%s#TYPE mhz19b_co2_ppm gauge\nmhz19b_co2_ppm %d\n";
const char *tmp_html_template = "%s#TYPE atc_temperature_celsius gauge\natc_temperature_celsius{sensor_name=\"%s\"} %f\n";
const char *hum_html_template = "%s#TYPE atc_humidty_percent gauge\natc_humidty_percent{sensor_name=\"%s\"} %f\n";
const char *bat_html_template = "%s#TYPE atc_bat_percent gauge\natc_bat_percent{sensor_name=\"%s\"} %d\n";

AsyncWebServer server(80);

ATC_MiThermometer miThermometer(knownBLEAddresses);

int temp = -1;
int co2ppm = -1;
float temps[3] = {0, 0.0, 0.0};
float humiditys[3] = {0.0, 0.0, 0.0};
uint8_t batterys[3] = {0, 0, 0};
MHZ19_uart mhz19;

void setup()
{
    Serial.begin(115200);
    mhz19.begin(rx_pin, tx_pin);
    mhz19.setAutoCalibration(true);

    Serial.println("MH-Z19 is warming up now.");
    delay(10 * 1000);

    Serial.print("Connecting to ");
    Serial.println(ssid);
    WiFi.begin(ssid, password);

    Serial.println();
    Serial.println("WiFi connected.");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());

    server.on("/metrics", HTTP_ANY, [](AsyncWebServerRequest *request)
              {
                  Serial.print("status: ");
                  Serial.println(mhz19.getStatus());

                  char buf[810];
                  memset(buf, 0, 810);

                  if (co2ppm != -1)
                  {
                      sprintf(buf, co2_html_template, "", co2ppm);
                  }
                  if (temp != -1)
                  {
                      sprintf(buf, tmp_html_template, buf, temp);
                  }

                  for (int i = 0; i < 3; i++)
                  {
                      if (temps[i] != 0.0)
                      {
                          sprintf(buf, tmp_html_template, buf, device_name[i], temps[i]);
                      }
                      if (humiditys[i] != 0.0)
                      {
                          sprintf(buf, hum_html_template, buf, device_name[i], humiditys[i]);
                      }
                      if (batterys[i] != 0)
                      {
                          sprintf(buf, bat_html_template, buf, device_name[i], batterys[i]);
                      }
                  }
                  request->send(200, "text/plain", buf); // 値をクライアントに返す
              });

    Serial.println("setup start");
    miThermometer.begin();
    server.begin();
    Serial.println("setup end");
}

uint8_t *findServiceData(uint8_t *data, size_t length, uint8_t *foundBlockLength)
{
    uint8_t *rightBorder = data + length;
    while (data < rightBorder)
    {
        uint8_t blockLength = *data + 1;
        Serial.printf("blockLength: 0x%02x\n", blockLength);
        if (blockLength < 5)
        {
            data += blockLength;
            continue;
        }
        uint8_t blockType = *(data + 1);
        uint16_t serviceType = *(uint16_t *)(data + 2);
        Serial.printf("blockType: 0x%02x, 0x%04x\n", blockType, serviceType);
        if (blockType == 0x16)
        { // https://www.bluetooth.com/specifications/assigned-numbers/generic-access-profile/
            // Serial.printf("blockType: 0x%02x, 0x%04x\n", blockType, serviceType);
            /* 16-bit UUID for Members 0xFE95 Xiaomi Inc. https://btprodspecificationrefs.blob.core.windows.net/assigned-values/16-bit%20UUID%20Numbers%20Document.pdf */
            if (serviceType == 0xfe95 || serviceType == 0x181a)
            { // mi or custom service
                Serial.printf("blockLength: 0x%02x\n", blockLength);
                Serial.printf("blockType: 0x%02x, 0x%04x\n", blockType, serviceType);
                *foundBlockLength = blockLength;
                return data;
            }
        }
        data += blockLength;
    }
    return nullptr;
}

void loop()
{
    co2ppm = mhz19.getCO2PPM();
    miThermometer.resetData();

    unsigned found = miThermometer.getData(10);

    for (int i = 0; i < miThermometer.data.size(); i++)
    {
        if (miThermometer.data[i].valid)
        {
            temps[i] = miThermometer.data[i].temperature / 100.0;
            humiditys[i] = miThermometer.data[i].humidity / 100.0;
            batterys[i] = miThermometer.data[i].batt_level;
            Serial.println();
        }
    }
    miThermometer.clearScanResults();
    delay(1000);
}
