#include <ESP8266WiFi.h>
#include "HTTPSRedirect.h"
#include <ESP8266WiFiMulti.h>
#include <WiFiUdp.h>
#include <NTPClient.h>

#define SMSensor A0
#define Relay D1

ESP8266WiFiMulti wifiMulti;
WiFiUDP ntpUDP;

const long utcOffsetInSeconds = 19800;
NTPClient timeClient(ntpUDP, "pool.ntp.org", utcOffsetInSeconds);

const char* host = "script.google.com";
const char* googleRedirHost = "script.googleusercontent.com";
const char *GScriptId = "";
const int httpsPort = 443;

//echo | openssl s_client -connect script.google.com:443 |& openssl x509 -fingerprint -noout
//const char* fingerprint = "37 83 9B 99 A1 C9 7D 64 9B 3D 93 1F F0 55 EB A5 F1 49 34 34";

String relayStatus;
String url;
float moisture_percentage;
int sensor_analog;
int systemStarted = millis();
 
void setup() {
  Serial.begin(9600);
  WiFi.mode(WIFI_STA);

  wifiMulti.addAP("", "");
  wifiMulti.addAP("", "");
  
  pinMode(Relay, OUTPUT);
  pinMode(SMSensor, INPUT);
  
  Serial.println();
  Serial.print("Connecting to wifi: ");
  while (wifiMulti.run() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected with IP address: ");
  Serial.println(WiFi.localIP());
}

void loop() {
  HTTPSRedirect client(httpsPort);
  timeClient.begin();

  client.setInsecure();
  sensor_analog = analogRead(SMSensor);
  moisture_percentage = ( 100 - ( (sensor_analog/1023.00) * 100 ) );

  if (moisture_percentage > 96){
    url = String("/macros/s/") + GScriptId + "/exec?relay=NA&tmp=0&status=Sensor_Failure";
    while (!client.connected())           
      client.connect(host, httpsPort);
    client.printRedir(url, host, googleRedirHost);      
  }
 
  while (moisture_percentage >= 42){
    delay(8*30000UL);
    sensor_analog = analogRead(SMSensor);
    moisture_percentage = ( 100 - ( (sensor_analog/1023.00) * 100 ) );
    Serial.print( "Moisture % with Relay OFF : " );
    Serial.println( moisture_percentage );
  }

  timeClient.update();
  if (timeClient.getHours() < 16 &&  timeClient.getHours() > 4){
     relayStatus = "ON";
     url = String("/macros/s/") + GScriptId + "/exec?tmp=" + moisture_percentage + "&relay=" + relayStatus;
     while (!client.connected())           
       client.connect(host, httpsPort);
     client.printRedir(url, host, googleRedirHost);      
     digitalWrite (Relay, HIGH);

     int motorStart = millis();
     int motorElapsed = millis() - motorStart;
     while ((moisture_percentage < 42) and (motorElapsed <= 6*60000UL)){
       delay(2*30000UL);
       sensor_analog = analogRead(SMSensor);
       moisture_percentage = ( 100 - ( (sensor_analog/1023.00) * 100 ) );
       Serial.print( "Moisture % with Relay ON : " );
       Serial.println( moisture_percentage );
       motorElapsed = millis() - motorStart;
       Serial.print( "motorOnfor : " );
       Serial.println(motorElapsed);
     }
   
     relayStatus = "OFF";
     url = String("/macros/s/") + GScriptId + "/exec?tmp=" + moisture_percentage + "&relay=" + relayStatus;
     while (!client.connected())           
       client.connect(host, httpsPort);
     client.printRedir(url, host, googleRedirHost);      
     digitalWrite (Relay, LOW);
     delay(5000);
  }
 
  int systemElapsed = millis() - systemStarted;
  if (systemElapsed >= 60*60000UL){
    url = String("/macros/s/") + GScriptId + "/exec?relay=NA&status=Restart" + "&tmp=" + moisture_percentage;
    while (!client.connected())           
      client.connect(host, httpsPort);
    client.printRedir(url, host, googleRedirHost);      
    ESP.restart();
  }
}
