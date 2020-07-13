// :::::::::: LCD display ::::::::::
#include <Wire.h> 
#include "DFRobot_RGBLCD.h"

DFRobot_RGBLCD lcd(16,2); //set the LCD for 16 characters and 2 lines

char line0[17];
char line1[17];

// :::::::::: WiFi Networking ::::::::::
// Enter sensitive data in the "arduino_secrets.h"
#include <SPI.h>
#include <WiFiNINA.h>
#include "arduino_secrets.h"

char ssid[] = SECRET_SSID;    // network SSID (name)
char pass[] = SECRET_PASS;    // network password (use for WPA)

int status = WL_IDLE_STATUS;  // the Wifi radio's status

WiFiServer server(80);        // start the basic web server

// :::::::::: Distance sensor ::::::::::
// Assign pin numbers:
const int trigPin = 12;
const int echoPin = 11;

// Define variables:
long duration;
int distance;

// :::::::::: Pump relay ::::::::::
// Assign pin numbers and state:
const int relayPin = 10;
int relayState = HIGH;



void setup() {
  // ::::::::: Initialize serial console ::::::::::
  Serial.begin(9600);
  
  // :::::::::: Initialize the LCD ::::::::::
  lcd.init();
  lcd.clear();

  // :::::::::: Initialize the network ::::::::::
  // Show network init message:
  lcdNetInit();
  delay(2000);
  
  // Check for the WiFi module:
  if (WiFi.status() == WL_NO_MODULE) {
    Serial.println("Communication with WiFi module failed!");
    lcdNetStatusWifiModuleError();
    // don't continue
    while (true);
  }
  
  // Check WiFi module firmware version:
  String fv = WiFi.firmwareVersion();
  if (fv < WIFI_FIRMWARE_LATEST_VERSION) {
    Serial.println("Please upgrade the firmware");
    lcdNetStatusWiFiModuleFirmware();
    delay(10000);
  }

  // Attempt to connect to Wifi network:
  while (status != WL_CONNECTED) {
    Serial.print("Connecting to: ");
    Serial.println(ssid);
    lcdNetStatusConnecting();
    // Connect to WPA/WPA2 network:
    status = WiFi.begin(ssid, pass);
    // Wait 10 seconds for connection:
    delay(10000);
  }
  
  // :::::::::: Start the server ::::::::::
  server.begin();
  
  // Connected now, so print out the data:
  Serial.print("You're connected to the network ");
  printCurrentNet();
  printWifiData();
  lcdNetStatusOk();
  
  // :::::::::: SENSOR ::::::::::
  // Assign pin status:
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);

  // :::::::::: RELAY ::::::::::
  pinMode(relayPin, OUTPUT);
  digitalWrite(relayPin, relayState); 
}



void loop() {
  // :::::::::: NETWORKING ::::::::::
  
  // Serial to Server:
  WiFiClient client = server.available();
  if (client) {
    Serial.println("New client connected");
    String currentLine = "";
    while (client.connected()) {
      if (client.available()) {
        char c = client.read();
        Serial.write(c);
        if (c == '\n') {

          // If the current line is blank, you got two newline characters in a row.
          // That's the end of the client HTTP request, so send a response:
          if (currentLine.length() == 0) {

            // Call web client function:
            showWebPage(client);
            
            // Break out of the while loop:
            break;
          } else {
            currentLine = "";
          }
        } else if (c != '\r') {
          currentLine += c;
        }
        
        // Check to see if the client request was "GET /H" or "GET /L":
        performRequest(currentLine);
      }
    }
    
    // close the connection:
    client.stop();
    Serial.println("Client disconnected");
  }
  
  // :::::::::: PUMP CONTROL ::::::::::
  
  // Distance readout:
  int dist = sensorReadDistance();
  
  // Change the desired distances in cm:
  if (dist > 200 || dist <= 0) {
    relayState = HIGH;
    Serial.print(dist);
    Serial.println("cm - out of range ");
    Serial.println("PUMP: OFF");
    lcdSensorOut();
  }

  else if (dist <= 20) {
    relayState = HIGH;
    Serial.print(dist);
    Serial.println("cm - tank full");
    Serial.println("PUMP: OFF ");
    lcdSensorFull();
 }

  else {
    relayState = LOW;
    Serial.print(dist);
    Serial.println("cm - filling");
    Serial.println("PUMP: ON ");
    lcdSensorFilling();
  }

  digitalWrite(relayPin, relayState);
  
}



// :::::::::: LCD FUNCTIONS ::::::::::
void lcdNetInit() {
  // print a network init message to the LCD
  lcd.setRGB(255,255,0);
  lcd.setCursor(4,0);
  lcd.print(F("NETWORK"));
  lcd.setCursor(2,1);
  lcd.print(F("INITALIZING:"));
  lcd.blink();
}

void lcdNetStatusConnecting() {
  // print network connecting message to LCD
  lcd.clear();
  lcd.setRGB(255,255,0);
  lcd.setCursor(1,0);
  lcd.print(F("CONNECTING TO:"));
  lcd.setCursor(5,1);
  lcd.print(ssid);
  lcd.blink();
}

void lcdNetStatusOk() {
  lcd.stopBlink();
  lcd.clear();
  lcd.setRGB(0,255,0);
  lcd.setCursor(0,0);
  lcd.print(F(" CONNECTED TO:"));
  lcd.setCursor(5,1);
  lcd.print(WiFi.SSID());
  delay(5000);
  lcd.clear();
}

void lcdNetStatusWifiModuleError() {
  lcd.stopBlink();
  lcd.clear();
  lcd.setRGB(255,0,0);
  lcd.setCursor(0,0);
  lcd.print(F("NETWORK MODULE:"));
  lcd.setCursor(4,1);
  lcd.print(F("ERROR"));
  lcd.blink();
}

void lcdNetStatusWiFiModuleFirmware() {
  lcd.stopBlink();
  lcd.clear();
  lcd.setRGB(255,0,0);
  lcd.setCursor(0,0);
  lcd.print(F("NETWORK MODULE:"));
  lcd.setCursor(0,1);
  lcd.print(F("UPDATE FIRMWARE!"));
  lcd.blink();
}

void lcdSensorOut() { 
  lcd.clear();
  lcd.setRGB(255,0,0);
  lcd.setCursor(2,0);
  lcd.print(F("Out of range"));
  lcd.setCursor(3,1);
  lcd.print(F("PUMP: OFF"));
}

void lcdSensorFull() {
  lcd.clear();
  lcd.setRGB(255,0,0);
  lcd.setCursor(0,0);
  lcd.print(sensorReadDistance());
  lcd.println(F("cm - tank full"));
  lcd.setCursor(3,1);
  lcd.print(F("PUMP: OFF"));
}

void lcdSensorFilling() {
  lcd.clear();
  lcd.setRGB(0,255,0);
  lcd.setCursor(0,0);
  lcd.print(sensorReadDistance());
  lcd.println(F("cm - filling"));
  lcd.setCursor(4,1);
  lcd.print(F("PUMP: ON"));
}

// :::::::::: NETWORK FUNCTIONS ::::::::::
void printWifiData() {
  // print your board's IP address:
  IPAddress ip = WiFi.localIP();
  Serial.print("IP Address: ");
  Serial.println(ip);
}

void printCurrentNet() {
  // print the SSID of the network you're attached to:
  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());

  // print the received signal strength:
  long rssi = WiFi.RSSI();
  Serial.print("signal strength (RSSI):");
  Serial.println(rssi);
}

// :::::::::: SERVER FUNCTIONS ::::::::::
void showWebPage(WiFiClient client) {
  // HTTP headers always start with a response code (e.g. HTTP/1.1 200 OK)
  // and a content-type so the client knows what's coming, then a blank line:
  client.println("HTTP/1.1 200 OK");
  client.println("Content-type:text/html");
  client.println();

  // the content of the HTTP response follows the header:
  client.println("<h1>PUMP Remote Control</h1>");
  client.println("<table border=1 style='text-align:center'>");
  client.println("<tr><th>Component</th><th>Status</th><th>Control</th></tr>");

  // PUMP relay:
  client.print("<tr><td>PUMP</td><td>");
  if (digitalRead(relayPin)) {
    client.print("<font style='color:red;'>OFF</font>");
  } else {
    client.print("<font style='color:green;'>ON</font>");
  }
  client.println("</td><td><a href='/relay/on'>ON</a> / <a href='/relay/off'>OFF</a></td></tr>");

  client.println("</table>");

  // The HTTP response ends with another blank line
  client.println();
}

void performRequest(String line) {
  if (line.endsWith("GET /relay/on")) {
    digitalWrite(relayPin, LOW);
  } else if (line.endsWith("GET /relay/off")) {
    digitalWrite(relayPin, HIGH);
  }
}

// :::::::::: SENSOR FUNCTIONS ::::::::::
int sensorReadDistance() {
  // Clear the trigPin:
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  // Send signal:
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  // Take a reading:
  duration = pulseIn(echoPin, HIGH);
  // Calculate and return the distance:
  distance = (duration/2)/29.154;
  return distance;
}
