// WiFi Manager and OTA support
#include "EasyWiFi.h"

EasyWiFi easyWifi;

//========== User code ==========
#include <HttpClient.h>
#include <Ambient.h>
#include "DataToMaker.h"
#include <WEMOS_SHT3X.h>
#include <Ticker.h>
#include "user_config.h"

const unsigned int channelId = AMBIENT_CHANNEL;
const char* writeKey = AMBIENT_WRITEKEY;

#define NUM_SENS 4
// sensorId1 "Temperature";
// sensorId2 "Humidity";
// sensorId3 "Weight";
// sensorId4 "Motion";

WiFiClient client;
Ambient ambient;

SHT3X sht30(0x45);

const char* webhookKey = MAKER_WEBHOOKKEY;

// declare new maker event
DataToMaker webhook(webhookKey);

const char* event_gotobed = MAKER_EVENT_gotobed;
const char* event_getup = MAKER_EVENT_getup;

int bedstate[2];
int bedhist = 0;
int bedmax = 0;
#define BED_ON 1
#define BED_OFF 0

#define BED_ON_LAG 20
#define BED_OFF_LAG 30

#define adc2bed(a) ((a) > 200 ? BED_ON : BED_OFF)

unsigned long lastupload = 0;

// D5 for PIR sensor Negative logic
#define PIN_PIR 14

Ticker pirTicker;
volatile int pirhist = 0;
volatile int pircount = 0;
volatile int pirreset = 0;

#define PIR_OFF 1
#define PIR_ON 0

void pirSample()
{
  int pir = digitalRead(PIN_PIR);
  if(pirreset){
    pircount = 0;
    pirhist = 0;
    pirreset = 0;
  }
  pircount++;
  if(pir == PIR_ON) pirhist++;
}


void setup() {
  Serial.begin(9600);
  Serial.println("\n\nBooting");

  easyWifi.connect();
  easyWifi.ota_begin();

  //=========== User setup =============
  Serial.println("\n** Starting multiple datastream upload to Ambient... **");

  ambient.begin(channelId, writeKey, &client);

  bedstate[1] = bedstate[0] = adc2bed(analogRead(A0));
  bedhist = 0;
  bedmax = 0;
  pirhist = 0;
  pircount = 0;
  pirreset = 0;
  pinMode(PIN_PIR, INPUT);
  pirTicker.attach_ms(100, pirSample);

  // Start up the library
}

void loop()
{
  easyWifi.loop();

  float sensorValue[NUM_SENS];

  int sht30ok = 0;
  if(sht30.get()==0){
    sensorValue[0] = sht30.cTemp;
    sensorValue[1] = sht30.humidity;
    sht30ok = 1;
  }
  else{
    easyWifi.led(1);
    sensorValue[0] = 0.0;
    sensorValue[1] = 0.0;
  }
  int bedsens = analogRead(A0);
  if(bedsens > 1000) bedsens = 1000;
  if(bedsens > bedmax) bedmax = bedsens;
  sensorValue[2] = bedmax;

  sensorValue[3] = (float)pirhist / pircount * 100.0;
  
  for(int i = 0; i < 4; i++){
    ambient.set(i + 1, sensorValue[i]);
//    webhook.setValue(i + 1, sensorValue[i]);
    Serial.print("Read sensor value ");
    Serial.println(sensorValue[i]);
  }
  webhook.setValue(1, sensorValue[0]);
  webhook.setValue(2, sensorValue[2]);
  webhook.setValue(3, sensorValue[3]);

  char *event = NULL;
  int changed = 0;
  bedstate[1] = adc2bed(bedsens);
  if(bedstate[1] == bedstate[0]){
    bedhist = 0;
  }
  else{
    bedhist++;
    int th = (bedstate[1] == BED_ON ? BED_ON_LAG : BED_OFF_LAG);
    if(bedhist >= th){
      changed = 1;
      bedstate[0] = bedstate[1];
      bedhist = 0;
      if(bedstate[0] == BED_ON){
        Serial.println("Go to bed");
        event = (char *)event_gotobed;
      }
      else{
        Serial.println("Get up");
        event = (char *)event_getup;
      }
    }
  }

#if 1
  if(changed){
    Serial.println("Uploading to Webhook");
    bool ret = webhook.connect();
    if(ret){
      easyWifi.led(1);
      webhook.post(event);
      easyWifi.led(0);
    }
    Serial.print("Webhook send returned ");
    Serial.println(ret);
  }
#endif

  if(sht30ok && (millis() - lastupload >= (unsigned long)(30 * 1000))){
    Serial.println("Uploading to Ambient");
    easyWifi.led(1);
#if 1
    bool ret = ambient.send();
#else
    bool ret = 1;
#endif
    easyWifi.led(0);
    if(ret){
      lastupload = millis();
      bedmax = 0;
      pirreset = 1;
    }
    Serial.print("Ambient send returned ");
    Serial.println(ret);
  }

  delay(1000);
}

/*
 * EOF
 */
