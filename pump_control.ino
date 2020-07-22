// :::::::::: LCD display ::::::::::
#include <Wire.h> 
#include "DFRobot_RGBLCD.h"

//set the LCD for 16 characters and 2 lines
DFRobot_RGBLCD lcd(16,2);
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
#define trigPin 12
#define echoPin 11

// Define variables:
int distance, duration;

// :::::::::: Pump relay ::::::::::
// Assign pin numbers and state:
const int pumpRelayPin = 9;
int pumpRelayState = HIGH;

// :::::::::: Valve relay ::::::::::
// Assign pin numbers and state:
const int valveRelayPin = 10;
int valveRelayState = HIGH;

// :::::::::: Timing ::::::::::
// Assign timings for the loop:
unsigned long lastRun = 0;            // will store last time Sensor was updated
unsigned long lastCheckTime = 0;
long onTime = 10000;                  // set milliseconds of on-time 120000
long offTime = 30000;                 // set milliseconds of off-time 7200000
bool isPumpWorking = false;




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

  // Connected now, so print out the data:
  Serial.print("You're connected to the network ");
  printCurrentNet();
  printWifiData();
  lcdNetStatusOk();

  // :::::::::: Start the server ::::::::::
  server.begin();
  
  // :::::::::: SENSOR ::::::::::
  // Assign pin status:
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);

  // :::::::::: PUMP RELAY ::::::::::
  pinMode(pumpRelayPin, OUTPUT);
  digitalWrite(pumpRelayPin, pumpRelayState);

  // :::::::::: VALVE RELAY ::::::::::
  pinMode(valveRelayPin, OUTPUT);
  digitalWrite(valveRelayPin, valveRelayState); 

}



void loop() {
  // :::::::::: WEB SERVER ::::::::::
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
        //performRequest(currentLine);
      }
    }
    
    // close the connection:
    client.stop();
    Serial.println("Client disconnected");
  }

  // :::::::::: IRRIGATION TANK CONTROL ::::::::::

  //delay(100);

  // Distance readout:
  int dist = sonar();

  // Timing:
  unsigned long currentMillis = millis();

  // :::::::::: VALVE CONTROL ::::::::::
  // Change the desired distances in cm:
  if (dist > 200 || dist <= 0) {
    pumpRelayState = HIGH;
    valveRelayState = HIGH;
    serialOutOfRange();
    lcdOutOfRange();
    digitalWrite(pumpRelayPin, pumpRelayState);
    digitalWrite(valveRelayPin, valveRelayState);
  }

  else if (dist <= 20) {
    pumpRelayState = HIGH;
    valveRelayState = HIGH;
    serialTankFull();
    lcdTankFull();
    digitalWrite(pumpRelayPin, pumpRelayState);
    digitalWrite(valveRelayPin, valveRelayState);
 }

  else {
    valveRelayState = LOW;
    serialValveFilling();
    lcdValveFilling();
    digitalWrite(valveRelayPin, valveRelayState);
  }

  // :::::::::: PUMP CONTROL ::::::::::
  // Change to desired distance in cm:
  if (lastRun == 0 || (!isPumpWorking && currentMillis >= lastRun + offTime)) {
    int dist = sonar();
    
    if (200 >= dist && dist > 20) {
      pumpRelayState = LOW;
      serialPumpFilling();
      lcdPumpFilling();
      lastRun = currentMillis;
      isPumpWorking = true;
      digitalWrite(pumpRelayPin, pumpRelayState);
    }

  }
  
  if(isPumpWorking && currentMillis > lastRun + onTime){
    pumpRelayState = HIGH;
    serialPumpWaiting();
    lcdPumpWaiting();
    isPumpWorking = false;
    digitalWrite(pumpRelayPin, pumpRelayState);
  }

}



// :::::::::: SERIAL CONSOLE FUNCTIONS ::::::::::
void serialOutOfRange() {
  Serial.print(sonar());
  Serial.println("cm: OUT OF RANGE!!!");
  Serial.println("|P:OFF|  |V:OFF|");
}

void serialTankFull() {
  Serial.print(sonar());
  Serial.println("cm: TANK FULL!!!");
  Serial.println("|P:OFF|  |V:OFF|");
}

void serialPumpFilling() {
  Serial.print(sonar());
  Serial.println("cm: FILLING...");
  Serial.println("PUMP: ON");
}

void serialValveFilling() {
  Serial.print(sonar());
  Serial.println("cm: FILLING...");
  Serial.println("VALVE: ON");
}

void serialPumpWaiting() {
  Serial.println("PUMP: WAITING...");
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
  lcd.setCursor(0,0);
  lcd.print(F(" CONNECTING TO: "));
  lcd.setCursor(5,1);
  lcd.print(ssid);
  lcd.blink();
}

void lcdNetStatusOk() {
  lcd.stopBlink();
  lcd.clear();
  lcd.setRGB(0,255,0);
  lcd.setCursor(0,0);
  lcd.print(F(" CONNECTED  TO: "));
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

void lcdOutOfRange() { 
  lcd.setRGB(255,0,0);
  lcd.setCursor(0,0);
  lcd.print(F(" OUT OF RANGE!!!"));
  lcd.setCursor(0,1);
  lcd.print(F("|P:OFF|  |V:OFF|"));
}

void lcdTankFull() {
  lcd.setRGB(255,0,0);
  lcd.setCursor(0,0);
  lcd.print(sonar());
  lcd.println(F("cm: TANK FULL!!!"));
  lcd.setCursor(0,1);
  lcd.print(F("|P:OFF|  |V:OFF|"));
}

void lcdPumpFilling() {
  lcd.setRGB(0,255,0);
  lcd.setCursor(0,0);
  lcd.print(sonar());
  lcd.println(F("cm: FILLING..."));
  lcd.setCursor(0,1);
  lcd.print(F("|P: ON| "));
}

void lcdValveFilling() {
  lcd.setRGB(0,255,0);
  lcd.setCursor(0,0);
  lcd.print(sonar());
  lcd.println(F("cm: FILLING..."));
  lcd.setCursor(8,1);
  lcd.print(F(" |V: ON|"));
}

void lcdPumpWaiting() { 
  lcd.setCursor(0,1);
  lcd.print(F("|P:WAIT|"));
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
  client.println("HTTP/3 200 OK");
  client.println("Content-type:text/html");
  client.println();

  // the content of the HTTP response follows the header:
  client.println("<!DOCTYPE HTML>");
  client.println("<html>");
  client.println("<head>");
  client.println("<meta http-equiv='refresh' content='10'>");
  client.println("<link href='https://fonts.googleapis.com/css2?family=Share+Tech+Mono&amp;display=swap' rel='stylesheet'>");
  client.println("</head>");
  client.println("<body style='background-color:LightGrey;'>");
  client.println("<h1>IRRIGATION TANK REMOTE CONTROL</h1>");
  client.println("<table border=1 style='text-align:center'>");
  client.println("<tr><th>Component</th><th>Status</th><th>Control</th></tr>");

  // PUMP relay:
  client.print("<tr><td>PUMP</td><td>");
  if (digitalRead(pumpRelayPin)) {
    client.print("<font style='font-family:'Share Tech Mono'; color:red;'>OFF</font>");
  } else {
    client.print("<font style='color:green;'>ON</font>");
  }
  client.println("</td><td><a href='/pump_relay/on'>ON</a> / <a href='/pump_relay/off'>OFF</a></td></tr>");

  // VALVE relay:
  client.print("<tr><td>VALVE</td><td>");
  if (digitalRead(valveRelayPin)) {
    client.print("<font style='color:red;'>OFF</font>");
  } else {
    client.print("<font style='color:green;'>ON</font>");
  }
  client.println("</td><td><a href='/valve_relay/on'>ON</a> / <a href='/valve_relay/off'>OFF</a></td></tr>");

  client.println("</table>");

  // Level meter:
  int level = sonar();

  client.println("<h1>IRRIGATION TANK CURRENT LEVEL</h1>");
  client.println("<table border=1 style='text-align:center'>");
  client.println("<tr><th>Tank</th><th>Level</th></tr>");

  client.print("<tr><td>IRRIGATION TANK</td><td>");
  if (200 >= level && level > 100) {
    client.print("<font style='color:red;'>");
    client.print(level);
    client.print("</font><font style='color:black;>'>cm</font>");
  } else if (100 >= level && level > 50) {
    client.print("<font style='color:gold;'>");
    client.print(level);
    client.print("</font><font style='color:black;>'>cm</font>");
  } else if (50 >= level && level > 20) {
    client.println("<font style='color:green;'>");
    client.print(level);
    client.print("</font><font style='color:black;>'> cm</font>");
  } else {
    client.println("<font style='color:pink;'>ERROR</font>");
  }
  
  // The HTTP response ends with another blank line:
  client.println();
}

//void performRequest(String line) {
  //if (line.endsWith("GET /pump_relay/on")) {
    //digitalWrite(pumpRelayPin, LOW);
  //} else if (line.endsWith("GET /pump_relay/off")) {
    //digitalWrite(pumpRelayPin, HIGH);
  //} else if (line.endsWith("GET /valve_relay/on")) {
    //digitalWrite(valveRelayPin, LOW);
  //} else if (line.endsWith("GET /valve_relay/off")) {
    //digitalWrite(valveRelayPin, HIGH);
  //}
//}

// :::::::::: SENSOR FUNCTIONS ::::::::::
int sonar() {
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
