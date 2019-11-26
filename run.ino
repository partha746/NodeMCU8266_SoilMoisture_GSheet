#include <ESP8266WiFi.h>
#include "HTTPSRedirect.h"
#include <ESP8266WiFiMulti.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include <BlynkSimpleEsp8266.h>
#include "wifi_passphrares.h" //includes variables SSID1, WifiPass, auth[] & *GScriptId

#define BLYNK_PRINT Serial
#define SMSensor A0
#define Relay D1

WidgetLED led(V0);
WidgetTerminal terminal(V1);
  
ESP8266WiFiMulti wifiMulti;
WiFiUDP ntpUDP;

const long utcOffsetInSeconds = 19800;
NTPClient timeClient(ntpUDP, "pool.ntp.org", utcOffsetInSeconds);

const char* host = "script.google.com";
const char* googleRedirHost = "script.googleusercontent.com";
const int httpsPort = 443;

String relayStatus;
String url;
float moisture_percentage;
int sensor_analog;
int systemStarted = millis();
float mois_thresh = 35.0; // Moisture below this should run motor
float sensorErrorThresh = 96.0; // Moisture reading more than this is sensor failure
int minTime = 8; //24 Hrs clock time // Time after to start watering plants
int maxTime = 12; //24 Hrs clock time // Time after to stop watering plants
long chkNWPTimer = 8*30000UL; // 4 mins {Check not watering plants timer}
long chkNWPOTTimer = 15*60000UL; // 15 mins {Check not watering plants timer out of time Limit}
long chkWPTimer = 1*30000UL; // 30 secs {Check watering plants timer}
long maxWPTimer = 4*60000UL; // 4 mins {Max watering plants timer}
long rebootTimer = 1*60*60000UL; // 1 Hrs {NodeMCU soft Reboot timer}
long reWaterTimer = 1*60*30000UL; // 30 Mins {Rewater once watered}

void blynkConnect()
{
  Blynk.begin(auth, SSID1, WifiPass);
}
 
void setup() {
  Serial.begin(9600);
  WiFi.mode(WIFI_STA);

  wifiMulti.addAP(SSID1, WifiPass);
  
  pinMode(Relay, OUTPUT);
  pinMode(SMSensor, INPUT);

  Serial.println("Connecting to wifi: ");
  while (wifiMulti.run() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.printf("Connected to SSID: %s & IP address: %s\n", WiFi.SSID().c_str(), WiFi.localIP().toString().c_str());
}

int reportSensorError(float smReading){
  HTTPSRedirect client(httpsPort);
  timeClient.begin();
  client.setInsecure();

  if (smReading > sensorErrorThresh){
    url = String("/macros/s/") + GScriptId + "/exec?relay=NA&tmp=0&status=Sensor_Failure";
    while (!client.connected())           
      client.connect(host, httpsPort);
    client.printRedir(url, host, googleRedirHost);      
    timeClient.update();
    blynkConnect();
    Blynk.notify("Sensor Failure@ " + timeClient.getFormattedTime() + "!!");
    return 1;
  }
  else{
    return 0;
  }
}

void ESPReboot(){
  HTTPSRedirect client(httpsPort);
  timeClient.begin();
  client.setInsecure();
  int systemElapsed = millis() - systemStarted;
  if (systemElapsed >= rebootTimer){
    url = String("/macros/s/") + GScriptId + "/exec?relay=NA&status=Restart" + "&tmp=" + moisture_percentage;
    while (!client.connected())           
      client.connect(host, httpsPort);
    delay(2000);
    client.printRedir(url, host, googleRedirHost);
    timeClient.update();
    delay(2000);
    blynkConnect();
    Blynk.notify("Restarting NODEMCU NOW @ " + timeClient.getFormattedTime() + "!!");
    ESP.restart();
  }
}

void loop() {
  blynkConnect();
  Blynk.run(); 
  
  HTTPSRedirect client(httpsPort);
  timeClient.begin();
  client.setInsecure();

  led.off();
  
  sensor_analog = analogRead(SMSensor);
  moisture_percentage = ( 100 - ( (sensor_analog/1023.00) * 100 ) );

  timeClient.update();
  if (timeClient.getHours() >= minTime && timeClient.getHours() <= maxTime){
    while (moisture_percentage >= mois_thresh ){
      long counterStart = millis();
      long counterElapsed = millis() - counterStart;
      while(counterElapsed <= chkNWPTimer){
        blynkConnect();
        counterElapsed = millis() - counterStart;
        delay(30500UL);
        ESPReboot();
      }
      sensor_analog = analogRead(SMSensor);
      moisture_percentage = ( 100 - ( (sensor_analog/1023.00) * 100 ) );
      reportSensorError(moisture_percentage);
      if (moisture_percentage < sensorErrorThresh){
        blynkConnect();
        Blynk.virtualWrite(V1, moisture_percentage);
        delay(2000);
        Blynk.virtualWrite(V2, moisture_percentage);
      }
    }
  }
  else{
    long counterStart = millis();
    long counterElapsed = millis() - counterStart;
    while(counterElapsed <= chkNWPOTTimer){
      counterElapsed = millis() - counterStart;
      delay(30500UL);
      ESPReboot();
    }
    sensor_analog = analogRead(SMSensor);
    moisture_percentage = ( 100 - ( (sensor_analog/1023.00) * 100 ) );
    reportSensorError(moisture_percentage);
    if (moisture_percentage < sensorErrorThresh){
      blynkConnect();
      Blynk.virtualWrite(V1, moisture_percentage);
      delay(2000);
      Blynk.virtualWrite(V2, moisture_percentage);
    }
  }

  timeClient.update();
  if (timeClient.getHours() >= minTime && timeClient.getHours() <= maxTime){
     relayStatus = "ON";
     blynkConnect();
     led.on();
     Blynk.notify("Watering Plants Now @ " + timeClient.getFormattedTime() + "!!");
     url = String("/macros/s/") + GScriptId + "/exec?tmp=" + moisture_percentage + "&relay=" + relayStatus;
     while (!client.connected())           
       client.connect(host, httpsPort);
     delay(2000);
     client.printRedir(url, host, googleRedirHost);      
     digitalWrite (Relay, HIGH);

     int motorStart = millis();
     int motorElapsed = millis() - motorStart;
     while ((moisture_percentage < mois_thresh) and (motorElapsed <= maxWPTimer)){
       delay(chkWPTimer);
       sensor_analog = analogRead(SMSensor);
       moisture_percentage = ( 100 - ( (sensor_analog/1023.00) * 100 ) );
       blynkConnect();
       Blynk.virtualWrite(V1, moisture_percentage);
       delay(2000);
       Blynk.virtualWrite(V2, moisture_percentage);
       motorElapsed = millis() - motorStart;
     }
   
     relayStatus = "OFF";
     led.off();
     blynkConnect();
     Blynk.virtualWrite(V1, moisture_percentage);
     delay(2000);
     Blynk.virtualWrite(V2, moisture_percentage);
     timeClient.update();
     Blynk.notify("Watering Plants Done @ " + timeClient.getFormattedTime() + "!!");
     url = String("/macros/s/") + GScriptId + "/exec?tmp=" + moisture_percentage + "&relay=" + relayStatus;
     while (!client.connected())           
       client.connect(host, httpsPort);
     delay(2000);
     client.printRedir(url, host, googleRedirHost);      
     digitalWrite (Relay, LOW);
     delay(reWaterTimer);
  }
  Blynk.run(); 
}
