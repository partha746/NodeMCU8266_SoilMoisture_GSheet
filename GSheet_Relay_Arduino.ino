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
const char *GScriptId = "";

WidgetLED led(V0);
WidgetTerminal terminal(V1);
  
ESP8266WiFiMulti wifiMulti;
WiFiUDP ntpUDP;

const long utcOffsetInSeconds = 19800;
NTPClient timeClient(ntpUDP, "pool.ntp.org", utcOffsetInSeconds);

const char* host = "script.google.com";
const char* googleRedirHost = "script.googleusercontent.com";
const int httpsPort = 443;

//echo | openssl s_client -connect script.google.com:443 |& openssl x509 -fingerprint -noout | cut -d ":" -f2- | tr ":" " "
//const char* fingerprint = "37 83 9B 99 A1 C9 7D 64 9B 3D 93 1F F0 55 EB A5 F1 49 34 34";

String relayStatus;
String url;
float moisture_percentage;
int sensor_analog;
int systemStarted = millis();
float mois_thresh = 40.0; // Moisture below this should run motor
float sensorErrorThresh = 96.0; // Moisture reading more than this is sensor failure
int minTime = 4; //24 Hrs clock time // Time after to start watering plants
int maxTime = 16; //24 Hrs clock time // Time after to stop watering plants
long chkNWPTimer = 8*30000UL; // 4 mins {Check not watering plants timer}
long chkWPTimer = 1*30000UL; // 30 secs {Check watering plants timer}
long maxWPTimer = 6*60000UL; // 6 mins {Max watering plants timer}
long rebootTimer = 4*60*60000UL; // 4 Hrs {Reboot timer}

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

  led.off();

  terminal.clear();
  terminal.print("Connecting to wifi: ");
  while (wifiMulti.run() != WL_CONNECTED) {
    delay(500);
    terminal.print(".");
  }
  String ipAddr = WiFi.localIP().toString();
  delay(1000);
  terminal.printf("Connected to SSID: %s & IP address: %s\n", WiFi.SSID().c_str(), WiFi.localIP().toString().c_str());
}

void reportSensorError(float smReading){
  HTTPSRedirect client(httpsPort);
  timeClient.begin();
  client.setInsecure();

  if (smReading > sensorErrorThresh){
    url = String("/macros/s/") + GScriptId + "/exec?relay=NA&tmp=0&status=Sensor_Failure";
    while (!client.connected())           
      client.connect(host, httpsPort);
      Serial.print( "Step 1 : Retrying to connect Google server..." );
    client.printRedir(url, host, googleRedirHost);      
    timeClient.update();
    blynkConnect();
    Blynk.notify("Sensor Failure@ " + timeClient.getFormattedTime() + "!!");
  }
}

void loop() {
  blynkConnect();
  
  HTTPSRedirect client(httpsPort);
  timeClient.begin();
  client.setInsecure();
  terminal.println("Connected Insecurely...");
  
  sensor_analog = analogRead(SMSensor);
  moisture_percentage = ( 100 - ( (sensor_analog/1023.00) * 100 ) );

  reportSensorError(moisture_percentage);
  
  while (moisture_percentage >= mois_thresh ){
    long counterStart = millis();
    long counterElapsed = millis() - counterStart;
    while(counterElapsed <= chkNWPTimer){
      counterElapsed = millis() - counterStart;
      Serial.println(counterElapsed);
      delay(30500UL);
    }
    sensor_analog = analogRead(SMSensor);
    moisture_percentage = ( 100 - ( (sensor_analog/1023.00) * 100 ) );
    reportSensorError(moisture_percentage);
    blynkConnect();
    Blynk.virtualWrite(V1, moisture_percentage);
    terminal.print( "Moisture % %s with Relay OFF : ");
    terminal.println( moisture_percentage );
    if (timeClient.getHours() > minTime && timeClient.getHours() < maxTime){
      moisture_percentage = mois_thresh + 1;
    }
  }

  timeClient.update();
  if (timeClient.getHours() > minTime && timeClient.getHours() < maxTime){
     relayStatus = "ON";
     led.on();
     Blynk.notify("Watering Plants Now @ " + timeClient.getFormattedTime() + "!!");
     url = String("/macros/s/") + GScriptId + "/exec?tmp=" + moisture_percentage + "&relay=" + relayStatus;
     while (!client.connected())           
       client.connect(host, httpsPort);
       Serial.print( "Step 2 : Retrying to connect Google server..." );
       
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
       terminal.print( "Moisture % %s with Relay ON : ");
       terminal.println( moisture_percentage );
       motorElapsed = millis() - motorStart;
     }
   
     relayStatus = "OFF";
     led.off();
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
  if (systemElapsed >= rebootTimer){
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
  terminal.flush();
  Blynk.run(); 
  delay(5000);
}
