// :::::::::: INCLUDES ::::::::::
#include <Wire.h> 
#include "DFRobot_RGBLCD.h"
#include <SPI.h>
#include <WiFiNINA.h>
#include "arduino_secrets.h"
#include <TimeLib.h>
#include <WiFiUdp.h>

// :::::::::: LCD DISPLAY SETUP ::::::::::
//set the LCD for 16 characters and 2 lines
DFRobot_RGBLCD lcd(16,2);
char line0[17];
char line1[17];

// :::::::::: WIFI NETWORKING SETUP ::::::::::

/* !!! Enter sensitive data in the "arduino_secrets.h" !!! */

char ssid[] = SECRET_SSID;                        // network SSID (name)
char pass[] = SECRET_PASS;                        // network password (use for WPA)

int status = WL_IDLE_STATUS;                      // the Wifi radio's status

// :::::::::: WEB SERVER SETUP ::::::::::
WiFiServer server(80);                            // start the basic web server

// :::::::::: SONAR SETUP ::::::::::
// Assign pin numbers:
const int trigPin = 12;
const int echoPin = 11;

// Define variables:
int distance, duration;

// Last sonar reading:
int lastSonarReading = 0;

// Sonar status variable:
char16_t state = "idle";

// :::::::::: PUMP RELAY SETUP ::::::::::
// Assign pin numbers and state:
const int pumpRelayPin = 9;
int pumpRelayState = HIGH;
boolean pumpOnFlag = false;

// :::::::::: VALVE RELAY SETUP ::::::::::
// Assign pin numbers and state:
const int valveRelayPin = 10;
int valveRelayState = HIGH;
boolean valveOnFlag = false;

// :::::::::: RTC SETUP ::::::::::
bool newSecondFireFlag = false;                   // rtc new second fire flag
unsigned long pitCounter = 0;                     // pit counter

// :::::::::: NTP SYNC FOR RTC SETUP ::::::::::

// Set the NTP Server:
static const char ntpServerName[] = "time.nist.gov";

// Set the time zone:
const int timeZone = +3;                          // East European Time (Sofia, Bulgaria) +2; DST +1

// Set daylight savings time:
/* TODO CODE HERE */

WiFiUDP Udp;
unsigned int localPort = 8888;                    // local port to listen for UDP packets

time_t getNtpTime();
void sendNTPpacket(IPAddress &address);

unsigned long actualTime = now();             // get actual system timestamp



void setup() {
  // ::::::::: INIT THE SERIAL CONSOLE ::::::::::
  Serial.begin(9600);
  
  // :::::::::: INIT THE LCD DISPLAY ::::::::::
  lcd.init();
  lcd.clear();

  // :::::::::: INIT THE WIFI CONNECTION ::::::::::
  // Show network init message:
  lcdNetInit();
  delay(2000);
  
  // Check for the WiFi module:
  if (WiFi.status() == WL_NO_MODULE) {
    //Serial.println("Communication with WiFi module failed!");
    lcdNetStatusWifiModuleError();
    // don't continue
    while (true);
  }
  
  // Check WiFi module firmware version:
  String fv = WiFi.firmwareVersion();
  if (fv < WIFI_FIRMWARE_LATEST_VERSION) {
    //Serial.println("Please upgrade the firmware");
  }

  // Attempt to connect to Wifi network:
  while (status != WL_CONNECTED) {
    //Serial.print("Connecting to: ");
    //Serial.println(ssid);
    lcdNetStatusConnecting();
    
    // Connect to WPA/WPA2 network:
    status = WiFi.begin(ssid, pass);
    
    // Wait 10 seconds for connection:
    delay(10000);
  }

  // Connected now, so print out the data:
  //Serial.print("You're connected to the network ");
  //serialPrintCurrentNet();
  //serialPrintWifiData();
  lcdNetStatusOk();

  // :::::::::: INIT THE WEB SERVER ::::::::::
  server.begin();

  // :::::::::: INIT THE RTC ::::::::::
  InternalRTC.attachClockInterrupt(rtc_irq);      // set user IRQ function fired each new second by internal RTC
                                                  // run only on megaAVR-0 series!
                                                  // note : use InternalRTC.detachClockInterrupt() for detach IRQ function

  //InternalRTC.attachInterrupt(pit_irq, 16);     // set user IRQ function fired by internal periodic interrupt (PIT)
                                                  // (1Hz by defaut, max 8192Hz, only power of 2 number)
                                                  // run only on megaAVR-0 series!
                                                  // note : use InternalRTC.detachInterrupt() for detach IRQ function

  // :::::::::: INIT THE NTP SYNC ::::::::::

  //Serial.println("TimeNTP Init...");
  //Serial.println("Starting UDP");
  Udp.begin(localPort);
  //Serial.print("Local port: ");
  //Serial.println(localPort);
  //Serial.println("waiting for sync");
  //if (getNtpTime() == 0) {
    //Serial.println("Communication with NTP server failed!");
    //lcdNtpSyncError();
    //setSyncProvider(getNtpTime);
    //setSyncInterval(1);
    // don't continue
    //while (true);
  //}
  setSyncProvider(getNtpTime);
  setSyncInterval(300);
  
  // :::::::::: INIT THE SONAR ::::::::::
  // Assign pin status:
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);

  // :::::::::: INIT THE PUMP RELAY ::::::::::
  pinMode(pumpRelayPin, OUTPUT);
  turnPumpOff();

  // :::::::::: INIT THE VALVE RELAY ::::::::::
  pinMode(valveRelayPin, OUTPUT);
  turnValveOff();

}



void loop() { 
  // :::::::::: WEB SERVER ::::::::::
  // Serial to Server:
  WiFiClient client = server.available();
  if (client) {
    //Serial.println("New client connected");
    String currentLine = "";
    while (client.connected()) {
      if (client.available()) {
        char c = client.read();
        //Serial.write(c);
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
    //Serial.println("Client disconnected");
  }

  // :::::::::: SHOW CLOCK ON SERIAL CONSOLE ::::::::::
  if( newSecondFireFlag ) {                       // make actions if flag is fired:
    newSecondFireFlag = false;                    // unfire flag
    actualTime = now();                           // get actual system timestamp

    // Display current system date and time:
    //Serial.print(year(actualTime));
    //Serial.print("-");
    //Serial.print(month(actualTime));
    //Serial.print("-");
    //Serial.print(day(actualTime));
    //Serial.print(" ");
    //Serial.print(hour(actualTime));
    //Serial.print(":");
    //Serial.print(minute(actualTime));
    //Serial.print(":");
    //Serial.println(second(actualTime));

    // Display current milliseconds:
    //Serial.println(now());

  // :::::::::: IRRIGATION TANK CONTROL ::::::::::
    // Distance readout:
    lastSonarReading = sonar();

    // Change the desired distances in cm:
    if (lastSonarReading > 400 || lastSonarReading <= 0) {
      state = "oor";
      turnPumpOff();
      turnValveOff();
      //serialTankState();
      lcdTankState();
    }

    else if (lastSonarReading < 15) {
      state = "full";
      turnPumpOff();
      turnValveOff();
      //serialTankState();
      lcdTankState();
    }

    else {
      state = "filling";
      int currentHour = hour(actualTime);
      int currentMinute = minute(actualTime);
      
      // :::::::::: PUMP CONTROL ::::::::::
      if (currentHour % 2 == 0 && (currentMinute >= 0 && currentMinute < 2)) {
        turnPumpOn();
      }

      else {
        turnPumpOff();
        lcdWaiting();
      }

      // :::::::::: VALVE CONTROL ::::::::::
      

      if ((currentHour >= 5 && currentHour <= 7) || (currentHour >= 21 && currentHour <= 23)) {           // 5; 7; 21; 23
        turnValveOn();
      }

      else {
        turnValveOff();
        lcdWaiting();
      }

      //serialTankState();
      lcdTankState();
      
    }
  }
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

void lcdNtpSyncError() {
  lcd.stopBlink();
  lcd.clear();
  lcd.setRGB(0,255,0);
  lcd.setCursor(0,0);
  lcd.print(F("NTP SYNC FAILED!"));
  lcd.setCursor(0,1);
  lcd.print("    RETRYING    ");
  delay(5000);
  lcd.clear();
}

void lcdTankState() {
  if (state == "filling") {
    if (pumpOnFlag) {
      lcd.setRGB(0,255,0);
      lcd.setCursor(0,0);
      lcd.print(lastSonarReading);
      lcd.println(F("cm: FILLING..."));
      lcd.setCursor(0,1);
      lcd.print(F("|P: ON| "));
    }

    else {
      lcd.setCursor(0,1);
      lcd.print(F("|P:WAIT|"));
    }

    if (valveOnFlag) {
      lcd.setRGB(0,255,0);
      lcd.setCursor(0,0);
      lcd.print(lastSonarReading);
      lcd.println(F("cm: FILLING..."));
      lcd.setCursor(8,1);
      lcd.print(F(" |V: ON|"));
    }

    else {
      lcd.setCursor(8,1);
      lcd.print(F("|V:WAIT|"));
    }
  }

  else if (state == "full") {
    lcd.setRGB(255,0,0);
    lcd.setCursor(0,0);
    lcd.print(lastSonarReading);
    lcd.println(F("cm: TANK FULL!!!"));
    lcd.setCursor(0,1);
    lcd.print(F("|P:OFF|  |V:OFF|"));
  }

  else if (state == "oor") {
    lcd.setRGB(255,0,0);
    lcd.setCursor(0,0);
    lcd.print(F(" OUT OF RANGE!!!"));
    lcd.setCursor(0,1);
    lcd.print(F("|P:OFF|  |V:OFF|"));
  }

}

void lcdWaiting() {
  if (!pumpOnFlag && !valveOnFlag) {
    lcd.setRGB(255,255,0);
    lcd.setCursor(0,0);
    lcd.print(F("  SYSTEM IDLE:  "));
  }
}

// :::::::::: RTC FUNCTIONS ::::::::::
void pit_irq() {                                  // executed all pediodic interrupt
     
  pitCounter++;                                   // increment pit counter
}

void rtc_irq() {                                  // executed at each RTC second interrupt
     
  newSecondFireFlag = true;                       // fire flag
}

// :::::::::: NTP FUNCTIONS ::::::::::
const int NTP_PACKET_SIZE = 48;                   // NTP time is in the first 48 bytes of message
byte packetBuffer[NTP_PACKET_SIZE];               // buffer to hold incoming & outgoing packets

time_t getNtpTime() {
    IPAddress ntpServerIP;                        // NTP server's ip address

    while (Udp.parsePacket() > 0) ;               // discard any previously received packets
    //Serial.println("Transmit NTP Request");
  
    // Get a random server from the pool:
    WiFi.hostByName(ntpServerName, ntpServerIP);
    //Serial.print(ntpServerName);
    //Serial.print(": ");
    //Serial.println(ntpServerIP);
    sendNTPpacket(ntpServerIP);
    uint32_t beginWait = millis();
    while (millis() - beginWait < 1500) {
      int size = Udp.parsePacket();
      if (size >= NTP_PACKET_SIZE) {
        //Serial.println("Receive NTP Response");
        Udp.read(packetBuffer, NTP_PACKET_SIZE);  // read packet into the buffer
        unsigned long secsSince1900;

        // Convert four bytes starting at location 40 to a long integer:
        secsSince1900 =  (unsigned long)packetBuffer[40] << 24;
        secsSince1900 |= (unsigned long)packetBuffer[41] << 16;
        secsSince1900 |= (unsigned long)packetBuffer[42] << 8;
        secsSince1900 |= (unsigned long)packetBuffer[43];
        return secsSince1900 - 2208988800UL + timeZone * SECS_PER_HOUR;
      }
    }
    //Serial.println("No NTP Response :-(");
    return 0;                                     // return 0 if unable to get the time
}

void sendNTPpacket(IPAddress &address) {          // send an NTP request to the time server at the given address
  // Set all bytes in the buffer to 0:
  memset(packetBuffer, 0, NTP_PACKET_SIZE);

  // Initialize values needed to form NTP request:
  packetBuffer[0] = 0b11100011;                   // LI, Version, Mode
  packetBuffer[1] = 0;                            // Stratum, or type of clock
  packetBuffer[2] = 6;                            // Polling Interval
  packetBuffer[3] = 0xEC;                         // Peer Clock Precision

  // 8 bytes of zero for Root Delay & Root Dispersion:
  packetBuffer[12] = 49;
  packetBuffer[13] = 0x4E;
  packetBuffer[14] = 49;
  packetBuffer[15] = 52;

  // All NTP fields have been given values, now
  // you can send a packet requesting a timestamp:
  Udp.beginPacket(address, 123);                  // NTP requests are to port 123
  Udp.write(packetBuffer, NTP_PACKET_SIZE);
  Udp.endPacket();
}

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

// :::::::::: RELAY FUNCTIONS ::::::::::
// Pump :
void turnPumpOn() {
  pumpRelayState = digitalRead(pumpRelayPin);     // read current pump state
  if (pumpRelayState == HIGH) {                   // if pump is off
    digitalWrite(pumpRelayPin, LOW);              // turn pump on
    pumpOnFlag = true;
  }
}

void turnPumpOff() {
  pumpRelayState = digitalRead(pumpRelayPin);     // read current pump state
  if (pumpRelayState == LOW) {                    // if pump is on
    digitalWrite(pumpRelayPin, HIGH);             // turn pump off
    pumpOnFlag = false;
  }
}

// Valve relay:
void turnValveOn() {
  valveRelayState = digitalRead(valveRelayPin);   // read current valve state
  if (valveRelayState == HIGH) {                  // if valve is off
    digitalWrite(valveRelayPin, LOW);             // turn valve on
    valveOnFlag = true;
    
  }
}

void turnValveOff() {
  valveRelayState = digitalRead(valveRelayPin);   // read current valve state
  if (valveRelayState == LOW) {                   // if valve is on
    digitalWrite(valveRelayPin, HIGH);            // turn valve off
    valveOnFlag = false;
  }
}

// :::::::::: SERVER FUNCTIONS ::::::::::
// Setup web page:
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
  //client.println("<meta http-equiv='refresh' content='10'>");
  client.println("</head>");
  client.println("<body style='background-color:LightGrey;'>");
  client.println("<h1>IRRIGATION TANK PUMP/VALVE STATUS</h1>");
  client.println("<table border=1 style='text-align:center'>");
  client.println("<tr><th>Component</th><th>Status</th></tr>");

  // PUMP relay:
  client.print("<tr><td>PUMP</td><td>");
  if (digitalRead(pumpRelayPin)) {
    client.print("<font style='color:red;'>OFF</font>");
  } else {
    client.print("<font style='color:green;'>ON</font>");
  }
  //client.println("</td><td><a href='/pump_relay/on'>ON</a> / <a href='/pump_relay/off'>OFF</a></td></tr>");

  // VALVE relay:
  client.print("<tr><td>VALVE</td><td>");
  if (digitalRead(valveRelayPin)) {
    client.print("<font style='color:red;'>OFF</font>");
  } else {
    client.print("<font style='color:green;'>ON</font>");
  }
  //client.println("</td><td><a href='/valve_relay/on'>ON</a> / <a href='/valve_relay/off'>OFF</a></td></tr>");

  client.println("</table>");

  // TANK LEVEL:
  client.println("<h1>IRRIGATION TANK CURRENT LEVEL</h1>");
  client.println("<table border=1 style='text-align:center'>");
  client.println("<tr><th>Tank</th><th>Level</th></tr>");

  client.print("<tr><td>IRRIGATION TANK</td><td>");
  client.print("<font style='color:black;'>");
  client.print(lastSonarReading);
  client.print("</font><font style='color:black;>'>cm</font>");
  client.println("</table>");

  client.println("<h1>CURRENT TIME:</h1>");
  client.print("<font style='font-size:60px;color:black;'>");
  client.print(hour(actualTime));
  client.print(":");
  client.print(minute(actualTime));
  client.print(":");
  client.println(second(actualTime));
  client.print("</font>");
  
  // The HTTP response ends with another blank line:
  client.println();
}
/* Setup headers:
void performRequest(String line) {
  if (line.endsWith("GET /pump_relay/on")) {
    digitalWrite(pumpRelayPin, LOW);
  }
  
  else if (line.endsWith("GET /pump_relay/off")) {
    digitalWrite(pumpRelayPin, HIGH);
  }
  
  else if (line.endsWith("GET /valve_relay/on")) {
    digitalWrite(valveRelayPin, LOW);
  }
  
  else if (line.endsWith("GET /valve_relay/off")) {
    digitalWrite(valveRelayPin, HIGH);
  }
}
*/

/* :::::::::: SERIAL CONSOLE FUNCTIONS (DEBUGGING ONLY) ::::::::::
// Wifi serial console functions:
void serialPrintWifiData() {
  // Print your board's IP address:
  IPAddress ip = WiFi.localIP();
  Serial.print("IP Address: ");
  Serial.println(ip);
}
void serialPrintCurrentNet() {
  // Print the SSID of the network you're attached to:
  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());
  // Print the received signal strength:
  long rssi = WiFi.RSSI();
  Serial.print("signal strength (RSSI):");
  Serial.println(rssi);
}
// Sonar serial console functions:
void serialTankState() {
  if (state == "filling") {
    Serial.print(lastSonarReading);
    Serial.println("cm: FILLING...");
    if (pumpOnFlag) {
      Serial.print("|P: ON|  ");
    }

    else {
      Serial.print("|P: WAIT|  ");
    }

    if (valveOnFlag) {
      Serial.println("|V: ON|");
    }

    else {
      Serial.println("|V: WAIT|");
    }
  }

  else if (state == "full") {
    Serial.print(lastSonarReading);
    Serial.println("cm: TANK FULL");
    Serial.println("|P: OFF|  |V: OFF|");
  }

  else if (state == "oor") {
    Serial.print(lastSonarReading);
    Serial.println("cm: OUT OF RANGE");
    Serial.println("|P: OFF|  |V: OFF|");
  }

}
*/
