// Load Wi-Fi library
#include <WiFi.h>
#include <analogWrite.h>
#include <EEPROM.h>
#include <PubSubClient.h>
#include "Credentials.h"

#define EEPROM_SIZE 5

// WiFi
WiFiClient wifiClient;
// RGB Web Server
WiFiServer server(80);
// MQTT Client
PubSubClient mqtt;
// Variable to store the HTTP request
String header;

// Assign output variables to GPIO pins
const int RED_PIN = 13;
const int GREEN_PIN = 14;
const int BLUE_PIN = 12;
const int POT_PIN = 34;

// Variables
int redValue = 0;
int greenValue = 0;
int blueValue = 0;
int potValue = 100;
int potReads[10];
int currentPotIndex = 0;
float previousReadAverage = 0;
float readAverage = 0;
unsigned long timeOfLastLedAndMqttUpdate = 0;
unsigned long timeOfLastPotRead = 0;
unsigned long timeOfLastReboot = 0;
unsigned long rebootInterval = 86400000; // 24 Hours.
bool enabled = false;
bool valueChanged = false;
bool ledOn = true;
String enabledValueString = "off";
String redValueString = String(redValue);
String greenValueString = String(greenValue);
String blueValueString = String(blueValue);
String potValueString = String(potValue);

void setup() {
  Serial.begin(115200);
  EEPROM.begin(EEPROM_SIZE);

  // Load settings from last run
  readFromEEPROM();
  
  // Connect to wifi network
  connectWifi();

  // Start the RGB Web Server
  server.begin();

  // Start the MQTT Client
  mqtt.setClient(wifiClient);
  mqtt.setServer(MQTT_SERVER, MQTT_PORT);
  connectMqtt();

  analogWriteResolution(8);
}

void loop(){
  if(timeHasElapsed(timeOfLastReboot, rebootInterval)) {
    timeOfLastReboot = millis();
    ESP.restart();
  }

  if(WiFi.status() != WL_CONNECTED) {
    connectWifi();
  }

  if (!mqtt.connected()) {
    connectMqtt();
  }
  
  updateLeds();

  runWebServer();
}

bool timeHasElapsed(unsigned long timeOfLastExe, unsigned long delayLength) {
  return (millis() - timeOfLastExe > delayLength);
}

void connectWifi() {
  // Connect to Wi-Fi network with SSID and password
  Serial.print("Connecting to ");
  Serial.println(WIFI_NAME);
  WiFi.begin(WIFI_NAME, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  // Print local IP address
  Serial.println("");
  Serial.println("WiFi connected.");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void connectMqtt() {
  while (!mqtt.connected()) {
    Serial.println("Connecting to MQTT...");
    
    if (mqtt.connect(MQTT_ID, MQTT_USER, MQTT_PASSWORD )) {
      Serial.println("connected");
    } else {
      Serial.print("failed with state ");
      Serial.print(mqtt.state());
      delay(500);
    }
  }
}

void readFromEEPROM() {
  redValue = EEPROM.read(0);
  greenValue = EEPROM.read(1);
  blueValue = EEPROM.read(2);
  potValue = EEPROM.read(3);
  enabled = EEPROM.read(4);

  redValueString = String(redValue);
  greenValueString = String(greenValue);
  blueValueString = String(blueValue);
  potValueString = String(potValue);
  ledOn = (potValue >= 5);
}

bool timeHasElapsed(unsigned long timeOfLastExe, int delayLength) {
  return (millis() - timeOfLastExe > delayLength);
}

float calculateAverage (int * array, int len)  // assuming array is int.
{
  long sum = 0L ;  // sum will be larger than an item, long for safety.
  for (int i = 0 ; i < len ; i++)
    sum += array [i] ;
  return  ((float) sum) / len ;  // average will be fractional, so float may be appropriate.
}

void readDebouncedPot() {
  if(timeHasElapsed(timeOfLastPotRead, 25)) {
    potReads[currentPotIndex] = analogRead(POT_PIN);
    if(currentPotIndex == 9) {
      currentPotIndex = 0;
    } else {
      currentPotIndex++;
    }
    readAverage = calculateAverage(potReads, 10);
    timeOfLastPotRead = millis();
  }
}

void updateLeds() {
  readDebouncedPot();

  if(abs(readAverage - previousReadAverage) > 10){
    potValue = map(readAverage, 0, 4095, 0, 100);
    EEPROM.write(3, potValue);
    EEPROM.commit();

    previousReadAverage = readAverage;
    potValueString = String(potValue);
    valueChanged = true;
  }

  if(valueChanged) {
    if(timeHasElapsed(timeOfLastLedAndMqttUpdate, 50)) {
      
      if(potValue >= 5 && !ledOn){
        mqtt.publish(MQTT_TOPIC, "On");
        ledOn = true;
      } else if(potValue < 5 && ledOn) {
        mqtt.publish(MQTT_TOPIC, "Off");
        ledOn = false;
      }

      if(enabled) {
        analogWrite(RED_PIN, redValue * potValue / 100);
        analogWrite(GREEN_PIN, greenValue * potValue / 100);
        analogWrite(BLUE_PIN, blueValue * potValue / 100);
      } else {
        analogWrite(RED_PIN, 0);
        analogWrite(GREEN_PIN, 0);
        analogWrite(BLUE_PIN, 0);
      }
      
      timeOfLastLedAndMqttUpdate = millis();
      valueChanged = false;
    }
  }
}

void runWebServer() {
  WiFiClient webClient = server.available();   // Listen for incoming clients

  if (webClient) {                             // If a new client connects,
    Serial.println("New Client.");          // print a message out in the serial port
    String currentLine = "";                // make a String to hold incoming data from the client
    while (webClient.connected()) {            // loop while the client's connected
      if (webClient.available()) {             // if there's bytes to read from the client,
        char c = webClient.read();             // read a byte, then
        Serial.write(c);                    // print it out the serial monitor
        header += c;
        if (c == '\n') {                    // if the byte is a newline character
          // if the current line is blank, you got two newline characters in a row.
          // that's the end of the client HTTP request, so send a response:
          if (currentLine.length() == 0) {
            // HTTP headers always start with a response code (e.g. HTTP/1.1 200 OK)
            // and a content-type so the client knows what's coming, then a blank line:
            webClient.println("HTTP/1.1 200 OK");
            webClient.println("Content-type:text/html");
            webClient.println("Connection: close");
            webClient.println();

            if(header.indexOf("GET /?e") >= 0) {
              int pos1 = header.indexOf('e')+2;
              int pos2 = header.indexOf('r')+2;
              int pos3 = header.indexOf('g')+2;
              int pos4 = header.indexOf('b')+2;

              enabledValueString = header.substring(pos1, pos2-3);
              
              if(enabledValueString != "on" && enabledValueString != "off") {
                enabledValueString = "on";
              }
              
              redValueString = header.substring(pos2, pos3-3);
              greenValueString = header.substring(pos3, pos4-3);
              blueValueString = header.substring(pos4);
              
              enabled=enabledValueString == "on";
              redValue=redValueString.toInt();
              greenValue=greenValueString.toInt();
              blueValue=blueValueString.toInt();

              EEPROM.write(0, redValue);
              EEPROM.write(1, greenValue);
              EEPROM.write(2, blueValue);
              EEPROM.write(3, potValue);
              EEPROM.write(4, enabled);
              EEPROM.commit();
            }

            valueChanged = true;

            // Display the HTML web page
            webClient.println("<!DOCTYPE html><html>");
            webClient.println("<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
            webClient.println("<link rel=\"icon\" href=\"data:,\">");
            webClient.println("<link rel=\"stylesheet\" href=\"https://maxcdn.bootstrapcdn.com/bootstrap/3.4.0/css/bootstrap.min.css\">");
            
            webClient.println("</head>");
            
            webClient.print("<center>");
            webClient.print("<body><h1>RGB Face Lights</h1>"); 
            webClient.print("<br>");
            webClient.print("<br>");
            
            webClient.print("<form style=width:33% method=get>");
            
            webClient.print("<h2>Intensity</h2>");
            webClient.print("<h4>"+ potValueString +"%</h4>");
            
            webClient.print("<br>");

            webClient.print("<h2>Enabled</h2>");
            webClient.print("<div class=form-group row>");
            webClient.print("<label for=enabledCheck class=\"col-sm-2 col-form-label\">Enable:</label>");
            webClient.print("<div class=col-sm-10>");
            webClient.print("<input type=text class=form-control value="+ enabledValueString +" name=e id=enabledCheck>");
            webClient.print("</div>");
            webClient.print("</div>");
            
            webClient.print("<br>");
            
            webClient.print("<h2>Controls</h2>");
            
            webClient.print("<div class=form-group row>");
            webClient.print("<label for=redInput class=\"col-sm-2 col-form-label\">Red:</label>");
            webClient.print("<div class=col-sm-10>");
            webClient.print("<input type=text class=form-control value="+ redValueString +" name=r id=redInput>");
            webClient.print("</div>");
            webClient.print("</div>");

            webClient.print("<div class=form-group row>");
            webClient.print("<label for=greenInput class=\"col-sm-2 col-form-label\">Green:</label>");
            webClient.print("<div class=col-sm-10>");
            webClient.print("<input type=text class=form-control value="+ greenValueString +" name=g id=greenInput>");
            webClient.print("</div>");
            webClient.print("</div>");

            webClient.print("<div class=form-group row>");
            webClient.print("<label for=blueInput class=\"col-sm-2 col-form-label\">Blue:</label>");
            webClient.print("<div class=col-sm-10>");
            webClient.print("<input type=text class=form-control value="+ blueValueString +" name=b id=blueInput>");
            webClient.print("</div>");
            webClient.print("</div>");
            
            webClient.print("<br>");
            webClient.print("<br>");

            webClient.print("<input class=\"btn btn-primary mb-2\" type=submit value=submit></form>");
                                   
            webClient.println("<center>");
            webClient.println("</body></html>");
            
            // The HTTP response ends with another blank line
            webClient.println();
            // Break out of the while loop
            break;
          } else { // if you got a newline, then clear currentLine
            currentLine = "";
          }
        } else if (c != '\r') {  // if you got anything else but a carriage return character,
          currentLine += c;      // add it to the end of the currentLine
        }
      }

      if(WiFi.status() != WL_CONNECTED) {
        connectWifi();
      }
    
      if (!mqtt.connected()) {
        connectMqtt();
      }
  
      updateLeds();
    }
    // Clear the header variable
    header = "";
    // Close the connection
    webClient.stop();
    Serial.println("Web Client disconnected.");
    Serial.println("");
  }
}
