#include <ESP8266WiFi.h>
#include "HTTPSRedirect.h"
#include <ESP8266WiFiMulti.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include <BlynkSimpleEsp8266.h>

#define BLYNK_PRINT Serial
#define SMSensor A0
#define Relay D1

char auth[] = "";

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

void blynkConnect()
{
  Blynk.begin(auth, "", "");
}
 
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
  Serial.print("WiFi connected with IP address: ");
  Serial.println(WiFi.localIP());

  blynkConnect();
  Serial.println("Blynk Server Connected...");
}

void loop() {
  blynkConnect();
  Serial.println("Blynk Server Connected...");

  HTTPSRedirect client(httpsPort);
  timeClient.begin();
  client.setInsecure();
  Serial.println("Connected Insecurely...");
  
  sensor_analog = analogRead(SMSensor);
  moisture_percentage = ( 100 - ( (sensor_analog/1023.00) * 100 ) );
  Serial.println("Read sensor data...");

  Blynk.virtualWrite(V1, moisture_percentage);
  if (moisture_percentage > 96){
    url = String("/macros/s/") + GScriptId + "/exec?relay=NA&tmp=0&status=Sensor_Failure";
    while (!client.connected())           
      client.connect(host, httpsPort);
      Serial.print( "Step 1 : Retrying to connect Google server..." );
    client.printRedir(url, host, googleRedirHost);      
    timeClient.update();
    blynkConnect();
    Blynk.notify("Sensor Failure@ " + timeClient.getFormattedTime() + "!!");
  }
 
  while (moisture_percentage >= 42.0){
    delay(10*30000UL);
    sensor_analog = analogRead(SMSensor);
    moisture_percentage = ( 100 - ( (sensor_analog/1023.00) * 100 ) );
    Blynk.virtualWrite(V1, moisture_percentage);
    Serial.print( "Moisture % with Relay OFF : " );
    Serial.println( moisture_percentage );
  }

  timeClient.update();
  if (timeClient.getHours() > 4 && timeClient.getHours() < 16){
     relayStatus = "ON";
     Blynk.notify("Watering Plants Now @ " + timeClient.getFormattedTime() + "!!");
     url = String("/macros/s/") + GScriptId + "/exec?tmp=" + moisture_percentage + "&relay=" + relayStatus;
     while (!client.connected())           
       client.connect(host, httpsPort);
       Serial.print( "Step 2 : Retrying to connect Google server..." );
       
     client.printRedir(url, host, googleRedirHost);      
     digitalWrite (Relay, HIGH);

     int motorStart = millis();
     int motorElapsed = millis() - motorStart;
     while ((moisture_percentage < 42) and (motorElapsed <= 6*60000UL)){
       delay(1*30000UL);
       sensor_analog = analogRead(SMSensor);
       moisture_percentage = ( 100 - ( (sensor_analog/1023.00) * 100 ) );
       blynkConnect();
       Blynk.virtualWrite(V1, moisture_percentage);
       Serial.print( "Moisture % with Relay ON : " );
       Serial.println( moisture_percentage );
       motorElapsed = millis() - motorStart;
       Serial.print( "motorOnfor : " );
       Serial.println(motorElapsed);
     }
   
     relayStatus = "OFF";
     blynkConnect();
     Blynk.virtualWrite(V1, moisture_percentage);
     timeClient.update();
     Blynk.notify("Watering Plants Done @ " + timeClient.getFormattedTime() + "!!");
     url = String("/macros/s/") + GScriptId + "/exec?tmp=" + moisture_percentage + "&relay=" + relayStatus;
     while (!client.connected())           
       client.connect(host, httpsPort);
       Serial.print( "Step 3 : Retrying to connect Google server..." );

     client.printRedir(url, host, googleRedirHost);      
     digitalWrite (Relay, LOW);
     delay(5000);
  }
 
  int systemElapsed = millis() - systemStarted;
  if (systemElapsed >= 60*60000UL){
    url = String("/macros/s/") + GScriptId + "/exec?relay=NA&status=Restart" + "&tmp=" + moisture_percentage;
    while (!client.connected())           
      client.connect(host, httpsPort);
      Serial.print( "Step 1 : Retrying to connect Google server..." );
    client.printRedir(url, host, googleRedirHost);
    timeClient.update();
    blynkConnect();
    Blynk.notify("Restarting NODEMCU NOW @ " + timeClient.getFormattedTime() + "!!");
    ESP.restart();
  }
  Blynk.run(); 
  delay(5000);
}
