#include <ESP8266WiFi.h>
#include <EEPROM.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <Time.h>

#define rck_pin 2
#define dat_pin 5
#define clk_pin 13

#define pwm_pin 4

MDNSResponder mdns;
ESP8266WebServer server ( 80 );


byte sevensegment[16] = {63,6,91,79,102,109,125,7,127,111,119,124,57,94,121,113};
byte sevensegmentN[16] = {0,6,91,79,102,109,125,7,127,111,0,0,0,0,0,0};

char s_ssid[33] = {0};
char s_password[65] = {0};
uint8_t s_timezone = 12;
uint8_t s_state = 0;
uint8_t s_summertime = 1;
uint8_t lhour = 0;
uint8_t lminute = 0;
uint8_t lsecond = 0;

bool output_enabled = true;
bool update_enabled = true;

unsigned int NTPlocalPort = 2390;
IPAddress timeServer(129, 6, 15, 28); //time.nist.gov NTP server
const int NTP_PACKET_SIZE = 48;
byte NTPpacketBuffer[ NTP_PACKET_SIZE];
WiFiUDP NTPudp;

void settings_load() {
  s_state = EEPROM.read(0);
  for (int i = 0; i<33; i++) {
    s_ssid[i] = EEPROM.read(1+i);
  }
  for (int i = 0; i<65; i++) {
    s_password[i] = EEPROM.read(1+33+i);
  }
  s_timezone = EEPROM.read(1+33+65);
  s_summertime = EEPROM.read(1+33+65+1);
}

void settings_store() {
  EEPROM.write(0,s_state); 
  for (int i = 0; i<33; i++) {
    EEPROM.write(1+i, s_ssid[i]);
  }
  for (int i = 0; i<65; i++) {
    EEPROM.write(1+33+i, s_password[i]);
  }
  EEPROM.write(1+33+65, s_timezone);
  EEPROM.write(1+33+65+1, s_summertime);
  EEPROM.commit();
}

void settings_setup() {
    uint8_t cnt = 0;
    Serial.flush();
    Serial.print("Press 's' to enter serial setup mode");
    while (cnt<100) {
      if (Serial.available()) {
        char input = Serial.read();
        Serial.println(input);
        if (input == 's' || input == 'S') {
          settings_setup_serial();
          break;
        }
      }
      delay(50);
      Serial.print(".");
      cnt++;
    }
    Serial.print("<INFO> Starting in AP mode...");
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP("WiFiClockSetup");
    Serial.print("<INFO> IP address: ");
    Serial.println(WiFi.softAPIP());
    if ( mdns.begin ( "wificlock.local", WiFi.localIP() ) ) {
      Serial.println ( "<INFO> MDNS responder started." );
    }
    server.on ("/", handleSetup);
    server.on ("/setupstore", handleSetupStore);
    server.onNotFound ( handleNotFound );
    server.begin();
    Serial.println("<INFO> HTTP server started in setup mode.");
  
    digitalWrite(rck_pin, LOW); 
    shiftOut(dat_pin, clk_pin, MSBFIRST, 119); //A    
    shiftOut(dat_pin, clk_pin, MSBFIRST, 115); //P   
    shiftOut(dat_pin, clk_pin, MSBFIRST ,0);    
    shiftOut(dat_pin, clk_pin, MSBFIRST, 0);
    digitalWrite(rck_pin, HIGH); 
  
    while (1) {
      mdns.update();
      server.handleClient();
    }
}

void settings_setup_serial() {
  ESP.wdtDisable();
  int setupstate = 0;
  while (setupstate < 3) {
    if (setupstate == 0) {
      Serial.flush();
      Serial.print("Please enter the SSID to use: ");
      bool waiting_for_input = true;
      uint8_t pos = 0;
      s_ssid[0] = 0;
      while (waiting_for_input) {
        if (Serial.available()) {
          char input = Serial.read();
          if (input == '\n') {
            waiting_for_input = false;
            break;
          }
          s_ssid[pos] = input;
          s_ssid[pos+1] = 0;
          Serial.print(input);
          pos++;
        }
        if (pos>31) {
          waiting_for_input = false;
          Serial.println("\nMaximum length for SSID reached!");
        }
      }
      Serial.print("\nSSID has been set to \"");
      Serial.print(s_ssid);
      Serial.println("\".");
      Serial.print("Is this correct? (y/N)");
      Serial.flush();
      while (!Serial.available());
      char input = Serial.read();
      Serial.println(input);
      Serial.flush();
      if (input == 'y' || input == 'Y') {
        setupstate = 1;
      }
    } else if (setupstate == 2) {
      delay(100);
      Serial.flush();
      Serial.print("Please set the timezone as 12 + offset to UTC: ");
      int intinput = Serial.parseInt();

      if ((intinput>=0) && (intinput<256)) {
        s_timezone = intinput;
      } else {
        Serial.print("\n<ERROR> Input has to be a number in the range 0 to 255.");
      }
      
      Serial.print("\nTimezone has been set to \"");
      Serial.print(s_timezone);
      Serial.println("\".");
      Serial.print("Is this correct? (y/N)");
      Serial.flush();
      while (!Serial.available());
      char input = Serial.read();
      Serial.flush();
      Serial.println(input);
      if (input == 'y' || input == 'Y') {
        Serial.print("Enable automatic summertime? (y/N)");
        Serial.flush();
        while (!Serial.available());
        char input = Serial.read();
        Serial.flush();
        Serial.println(input);
        if (input == 'y' || input == 'Y') {
          s_summertime = 1;
          Serial.println("Automatic summertime enabled!");
        } else {
          s_summertime = 0;
        }
        setupstate = 3;
      }
    } else if (setupstate == 1) {
      delay(100);
      Serial.flush();
      Serial.print("Please enter the password to use: ");
      bool waiting_for_input = true;
      uint8_t pos = 0;
      s_password[0] = 0;
      while (waiting_for_input) {
        if (Serial.available()) {
          char input = Serial.read();
          if (input == '\n') {
            waiting_for_input = false;
            Serial.print("*");
            break;
          }
          s_password[pos] = input;
          s_password[pos+1] = 0;
          Serial.print(input);
          pos++;
        }
        if (pos>63) {
          waiting_for_input = false;
          Serial.println("\nMaximum length for password reached!");
        }
      }
      Serial.print("\nPassword has been set to \"");
      Serial.print(s_password);
      Serial.println("\".");
      Serial.print("Is this correct? (y/N)");
      Serial.flush();
      while (!Serial.available());
      char input = Serial.read();
      Serial.flush();
      Serial.println(input);
      if (input == 'y' || input == 'Y') {
        setupstate = 2;
      }
    }
  }
  Serial.print("Setup complete. Writing settings to flash...");
  s_state = 1;
  settings_store();
  Serial.print("Rebooting...");
  delay(100);
  ESP.wdtEnable(100);
  ESP.reset();
}

void handleNotFound() {
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += ( server.method() == HTTP_GET ) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";

  for ( uint8_t i = 0; i < server.args(); i++ ) {
    message += " " + server.argName ( i ) + ": " + server.arg ( i ) + "\n";
  }

  server.send ( 404, "text/plain", message );
}

String getValue(String data, char separator, int index)
{
  int found = 0;
  int strIndex[] = {0, -1};
  int maxIndex = data.length()-1;

  for(int i=0; i<=maxIndex && found<=index; i++){
    if(data.charAt(i)==separator || i==maxIndex){
        found++;
        strIndex[0] = strIndex[1]+1;
        strIndex[1] = (i == maxIndex) ? i+1 : i;
    }
  }

  return found>index ? data.substring(strIndex[0], strIndex[1]) : "";
}

void handleSetupStore() {
  String message = "Setup:\n";
  for ( uint8_t i = 0; i < server.args(); i++ ) {
    message += " " + server.argName ( i ) + ": " + server.arg ( i ) + "\n";
    if (server.argName(i)=="ssid") {
      message += " SSID set to "+server.arg(i)+"\n";
      String valuestr = String(server.arg(i));
      valuestr.toCharArray(s_ssid, 33);
    }
    if (server.argName(i)=="password") {
      message += " Password set to "+server.arg(i)+"\n";
      String valuestr = String(server.arg(i));
      valuestr.toCharArray(s_password, 65);
    }
    if (server.argName(i)=="timezone") {
      String valuestr = String(server.arg(i));
      message += " Timezone set to "+server.arg(i)+"\n";
      int timezoneint = valuestr.toInt();
      if ((timezoneint>=0) && (timezoneint<256)) {
        s_timezone = timezoneint;
      } else {
        Serial.println("<ERROR> timezone out of bounds.");
        message += "ERROR: Timezone out of bounds\n";
      };
    }
    if (server.argName(i)=="summertime") {
      String valuestr = String(server.arg(i));
      message += " Summertime set to "+server.arg(i)+"\n";
      int summertimeint = valuestr.toInt();
      if ((summertimeint>=0) && (summertimeint<256)) {
        s_summertime = summertimeint;
      } else {
        Serial.println("<ERROR> summertime out of bounds.");
        message += "ERROR: Summertime out of bounds\n";
      };
    }
    if (server.argName(i)=="store") {
      s_state = 1;
      message += " Save to flash!\n";
      settings_store();
    }
  }

  server.send ( 200, "text/plain", message );
  delay(1000);
  ESP.reset();
}

int cval = 0;
bool binval = false;
uint8_t bval[4] = {0};
int brightness = 1024;
bool autobrightness = true;

void handleCommand() {
  String message = "Command:\n";
  for ( uint8_t i = 0; i < server.args(); i++ ) {
    message += " " + server.argName ( i ) + ": " + server.arg ( i ) + "\n";
    if (server.argName(i)=="value") {
      message += " Value set to "+server.arg(i)+"\n";
      String valuestr = String(server.arg(i));
      cval = valuestr.toInt();
      binval = false;
      update_enabled = false;
      Serial.print("<DEBUG> Set output to custom value '");
      Serial.print(cval);
      Serial.println("'.");
    }
    if (server.argName(i)=="binval") {
      message += " Value set to "+server.arg(i)+"\n";
      String valuestr = String(server.arg(i));
      int ch1 = getValue(valuestr, 'x', 0).toInt();
      int ch2 = getValue(valuestr, 'x', 1).toInt();
      int ch3 = getValue(valuestr, 'x', 2).toInt();
      int ch4 = getValue(valuestr, 'x', 3).toInt();
      if ((ch1>=0) && (ch1<256)) {
        bval[0] = ch1;
      } else {
        Serial.println("<DEBUG> byte 1 out of bounds.");
        message += "Byte 1 out of bounds\n";
      };
      if ((ch2>=0) && (ch2<256)) {
        bval[1] = ch2;
      } else {
        Serial.println("<DEBUG> byte 2 out of bounds.");
        message += "Byte 2 out of bounds\n";
      };
      if ((ch3>=0) && (ch3<256)) {
        bval[2] = ch3;
      } else {
        Serial.println("<DEBUG> byte 3 out of bounds.");
        message += "Byte 3 out of bounds\n";
      };
      if ((ch4>=0) && (ch4<256)) {
        bval[3] = ch4;
      } else {
        Serial.println("<DEBUG> byte 4 out of bounds.");
        message += "Byte 4 out of bounds\n";
      };

      binval = true;
      update_enabled = false;
      Serial.print("<DEBUG> Set output to custom binary value '");
      Serial.print(ch1);
      Serial.print(",");
      Serial.print(ch2);
      Serial.print(",");
      Serial.print(ch3);
      Serial.print(",");
      Serial.print(ch4);
      Serial.println("'.");
    }
    if (server.argName(i)=="brightness" && server.arg(i)=="auto") {
      message += "Automatic brightness enabled.\n";
      autobrightness = true;
    } else if (server.argName(i)=="brightness") {
      message += " Brightness set to "+server.arg(i)+"\n";
      String valuestr = String(server.arg(i));
      brightness = valuestr.toInt();
      autobrightness = false;
      Serial.print("<DEBUG> Set brightness to value '");
      Serial.print(brightness);
      Serial.println("'.");
    }
    if (server.argName(i)=="output" && server.arg(i)=="on") {
      message += " Output enabled.\n";
      output_enabled = true;
    }
    if (server.argName(i)=="output" && server.arg(i)=="off") {
      message += " Output disabled.\n";
      output_enabled = false;
    }
    if (server.argName(i)=="update" && server.arg(i)=="on") {
      message += " Clock enabled.\n";
      update_enabled = true;
    }
    if (server.argName(i)=="update" && server.arg(i)=="off") {
      message += " Clock disabled.\n";
      update_enabled = false;
    }
    if (server.argName(i)=="reset") {
      ESP.reset();
    }
    if (server.argName(i)=="factory") {
      s_state = 0; 
      s_ssid[0] = 0;
      s_password[0] = 0;
      s_timezone = 12;
      settings_store();
    }
  }

  server.send ( 200, "text/plain", message );
}

void handleRoot() {
  char temp[1300];

  snprintf ( temp, 1300,

"<html>\
  <head>\
    <title>RN+ WiFi clock</title>\
    <style>\
      body { background-color: #363636; font-family: Arial, Helvetica, Sans-Serif; Color: #FFFFFF; }\
      A { color: #FFFFFF; }\
      .logo1 { color: 00AEEF;}\
    </style>\
  </head>\
  <body>\
    <h1><span class='logo1'>R</span>N+ WiFi clock</h1>\
    <hr />\
    <p>Local time: %02d:%02d:%02d</p>\
    <hr />\
    <p><a href='/command?output=on'>Enable output</a></p>\
    <p><a href='/command?output=off'>Disable output</a></p>\
    <p><a href='/command?update=on'>Enable clock</a></p>\
    <p><a href='/command?update=off'>Disable clock</a></p>\
    <form action='/command' method='post'>Value: <input type='text' name='value' value='0'><input type='submit'></form>\
    <form action='/command' method='post'>Binary value: <input type='text' name='binval' value='0x0x0x0'><input type='submit'></form>\
    <form action='/command' method='post'>Brightness: <input type='text' name='brightness' value='0'><input type='submit'></form>\
    <p><a href='/command?brightness=auto'>Enable automatic brightness</a></p>\
    <p><a href='/setup'>Go to setup</a></p>\
    <p><a href='/command?reset=true'>Reboot</a></p>\
    <p>(timezone offset is set to %03d)</p>\
  </body>\
</html>",

    lhour, lminute, lsecond, s_timezone
  );
  server.send ( 200, "text/html", temp );
}

void handleSetup() {
  char temp[1200];

  snprintf ( temp, 1200,

"<html>\
  <head>\
    <title>RN+ WiFi clock Setup</title>\
    <style>\
      body { background-color: #cccccc; font-family: Arial, Helvetica, Sans-Serif; Color: #000088; }\
    </style>\
  </head>\
  <body>\
    <h1>RN+ WiFi clock - Setup</h1>\
    <form action='/setupstore' method='post'>SSID: <input type='text' name='ssid' value=''><br />\
    Password: <input type='text' name='password' value=''><br />\
    Timezone (12+offset to UTC): <input type='text' name='timezone' value='12'><br />\
    Automatic summertime adjust: <input type='radio' name='summertime' value='1'>Yes&nbsp;<input type='radio' name='summertime' value='0'>No<br />\
    <input type='submit' value='Save configuration'><input type='hidden' name='store' value='yes'></form>\
  </body>\
</html>"
  );
  server.send ( 200, "text/html", temp );
}

unsigned long sendNTPpacket(IPAddress& address)
{
  Serial.println("sending NTP packet...");
  // set all bytes in the buffer to 0
  memset(NTPpacketBuffer, 0, NTP_PACKET_SIZE);
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)
  NTPpacketBuffer[0] = 0b11100011;   // LI, Version, Mode
  NTPpacketBuffer[1] = 0;     // Stratum, or type of clock
  NTPpacketBuffer[2] = 6;     // Polling Interval
  NTPpacketBuffer[3] = 0xEC;  // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  NTPpacketBuffer[12]  = 49;
  NTPpacketBuffer[13]  = 0x4E;
  NTPpacketBuffer[14]  = 49;
  NTPpacketBuffer[15]  = 52;

  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:
  NTPudp.beginPacket(address, 123); //NTP requests are to port 123
  NTPudp.write(NTPpacketBuffer, NTP_PACKET_SIZE);
  NTPudp.endPacket();
}

void setup() {
  Serial.begin(115200);
  Serial.println("\n\n7-Segment WiFi clock\nRN+ 2015\n\nFirmware version: 1.0\n");

  pinMode(rck_pin, OUTPUT);
  pinMode(dat_pin, OUTPUT);
  pinMode(clk_pin, OUTPUT);
  pinMode(pwm_pin, OUTPUT);

  EEPROM.begin(sizeof(s_state)+sizeof(s_ssid)+sizeof(s_password)+sizeof(s_timezone)+sizeof(s_summertime));

  //Read state, ssid and password from flash
  settings_load();

  if (s_state==0) {
    Serial.println("<INFO> The device has not been configured yet.");
    settings_setup();
  } else if (s_state>1) {
    Serial.println("<ERROR> Unknown state. Resetting to factory default settings...");
    s_state = 0; 
    s_ssid[0] = 0;
    s_password[0] = 0;
    s_timezone = 12;
    settings_store();
    ESP.reset();
  } else {
    Serial.print("<INFO> Device has been configured to connect to network \"");
    Serial.print(s_ssid);
    Serial.println("\".");
    Serial.print("<DEBUG> WiFi password is \"");
    Serial.print(s_password);
    Serial.println("\".");
    Serial.flush();
    uint8_t cnt = 0;
    Serial.flush();
    Serial.print("Press 's' to enter serial setup mode");
    while (cnt<100) {
      if (Serial.available()) {
        char input = Serial.read();
        Serial.println(input);
        if (input == 's' || input == 'S') {
          settings_setup_serial();
          break;
        }
      }
      delay(10);
      Serial.print(".");
      cnt++;
    }
    Serial.print("\n<INFO> Connecting to the WiFi network");
    WiFi.mode(WIFI_STA);
    WiFi.begin(s_ssid,s_password);
    while (WiFi.status() != WL_CONNECTED) {
      Serial.print(".");
      delay(10);
    }
    Serial.println(" connected");
    Serial.print("<INFO> IP address: ");
    Serial.println(WiFi.localIP());
  }

  if ( mdns.begin ( "wificlock.local", WiFi.localIP() ) ) {
    Serial.println ( "<INFO> MDNS responder started." );
  }

  /*server.on ("/", []() {
    server.send(200, "text/plain", "RN+ WiFi clock");
  } );*/
  server.on ("/", handleRoot);
  server.on ("/command", handleCommand);
  server.on ("/setup", handleSetup);
  server.on ("/setupstore", handleSetupStore);
  server.onNotFound ( handleNotFound );
  server.begin();
  Serial.println("<INFO> HTTP server started.");

  NTPudp.begin(NTPlocalPort);
  sendNTPpacket(timeServer);
  Serial.println("<INFO> NTP client started.");

  //pinMode(rck_pin, OUTPUT);
  //pinMode(dat_pin, OUTPUT);
  //pinMode(clk_pin, OUTPUT);
  //pinMode(14, OUTPUT);
}

//uint8_t brightness = 0;

uint8_t anA[3][12] = {{1,0,0,0,0,0,0,0,0,8,16,32},{0,1,1,1,1,1,1, 1, 1, 9,25,57},{57,25, 9, 1,1,1,1,1,1,1,1,0}};
uint8_t anB[3][12] = {{0,1,0,0,0,0,0,0,8,0, 0, 0},{0,0,1,1,1,1,1, 1, 9, 9, 9, 9},{ 9, 9, 9, 9,1,1,1,1,1,1,0,0}};
uint8_t anC[3][12] = {{0,0,1,0,0,0,0,8,0,0, 0, 0},{0,0,0,1,1,1,1, 9, 9, 9, 9, 9},{ 9, 9, 9, 9,9,1,1,1,1,0,0,0}};
uint8_t anD[3][12] = {{0,0,0,1,2,4,8,0,0,0, 0, 0},{0,0,0,0,1,3,7,15,15,15,15,15},{15,15,15,15,7,3,1,0,0,0,0,0}};


long nextSecond = 0;
long nextUpdate = 0;
void loop() {  
  mdns.update();
  server.handleClient();
  
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("<ERROR> Lost connection to WiFi network. Rebooting...");
    delay(100);
    ESP.reset();
  }

  if (millis()>nextUpdate) {
    nextUpdate = millis()+50;

    if (autobrightness) {
      if (lhour>=21) {
        brightness = 324; //Avond
      } else {
        if (lhour<8) {
          brightness = 24; //Nacht
        } else {
          brightness = 524; //Dag
        }
      }
    }
    
    analogWrite(pwm_pin, 1024-brightness);
    digitalWrite(rck_pin, LOW);
    if (output_enabled) {
      if (update_enabled) {
        int clockval = lhour*100+lminute;
        shiftOut(dat_pin, clk_pin, MSBFIRST, sevensegment[(clockval/1000)%10]);    
        shiftOut(dat_pin, clk_pin, MSBFIRST, sevensegment[(clockval/100)%10]);    
        shiftOut(dat_pin, clk_pin, MSBFIRST, sevensegment[(clockval/10)%10]);    
        shiftOut(dat_pin, clk_pin, MSBFIRST, sevensegment[clockval%10]);    
      } else {
        if (binval) {
          shiftOut(dat_pin, clk_pin, MSBFIRST, bval[0]);    
          shiftOut(dat_pin, clk_pin, MSBFIRST, bval[1]);    
          shiftOut(dat_pin, clk_pin, MSBFIRST ,bval[2]);    
          shiftOut(dat_pin, clk_pin, MSBFIRST, bval[3]);
        } else {
          shiftOut(dat_pin, clk_pin, MSBFIRST, sevensegment[(cval/1000)%10]);    
          shiftOut(dat_pin, clk_pin, MSBFIRST, sevensegment[(cval/100)%10]);    
          shiftOut(dat_pin, clk_pin, MSBFIRST, sevensegment[(cval/10)%10]);    
          shiftOut(dat_pin, clk_pin, MSBFIRST, sevensegment[cval%10]);   
        }
      }
    } else {
      shiftOut(dat_pin, clk_pin, MSBFIRST, 0);    
      shiftOut(dat_pin, clk_pin, MSBFIRST, 0);    
      shiftOut(dat_pin, clk_pin, MSBFIRST, 0);    
      shiftOut(dat_pin, clk_pin, MSBFIRST, 0);    
    }
    digitalWrite(rck_pin, HIGH); 
  }
  
  if (millis()>nextSecond) {
    nextSecond = millis()+1000;
    lsecond++;
    if (lsecond>59) {
      lsecond = 0;
      lminute++;
      sendNTPpacket(timeServer);
    }
    if (lminute>59) {
      lminute = 0;
      lhour++;
    }
     int cb = NTPudp.parsePacket();
    if (cb) {
      Serial.print("packet received, length=");
      Serial.println(cb);
      // We've received a packet, read the data from it
      NTPudp.read(NTPpacketBuffer, NTP_PACKET_SIZE); // read the packet into the buffer
  
      //the timestamp starts at byte 40 of the received packet and is four bytes,
      // or two words, long. First, esxtract the two words:
  
      unsigned long highWord = word(NTPpacketBuffer[40], NTPpacketBuffer[41]);
      unsigned long lowWord = word(NTPpacketBuffer[42], NTPpacketBuffer[43]);
      // combine the four bytes (two words) into a long integer
      // this is NTP time (seconds since Jan 1 1900):
      unsigned long secsSince1900 = highWord << 16 | lowWord;
      Serial.print("Seconds since Jan 1 1900 = " );
      Serial.println(secsSince1900);
  
      // now convert NTP time into everyday time:
      Serial.print("Unix time = ");
      // Unix time starts on Jan 1 1970. In seconds, that's 2208988800:
      const unsigned long seventyYears = 2208988800UL;
      // subtract seventy years:
      unsigned long epoch = secsSince1900 - seventyYears;
      // print Unix time:
      Serial.println(epoch);

      time_t t = epoch;
      
      
      // print the hour, minute and second:
      /*Serial.print("The UTC time is ");       // UTC is the time at Greenwich Meridian (GMT)
      Serial.print((epoch  % 86400L) / 3600); // print the hour (86400 equals secs per day)
      lhour = ((epoch  % 86400L) / 3600) + s_timezone - 12;*/

      Serial.print("UTC date and time: ");
      Serial.print(day(t));
      Serial.print("-");
      Serial.print(month(t));
      Serial.print("-");
      Serial.print(year(t));
      Serial.print(" ");
      Serial.print(hour(t));
      Serial.print(":");
      Serial.print(minute(t));
      Serial.print(":");
      Serial.println(second(t));

     if (s_summertime) {
       Serial.print("Summertime correction: ");
       int beginDSTDate=  (31 - (5* year(t) /4 + 4) % 7);
       int beginDSTMonth=3;
       int endDSTDate= (31 - (5 * year(t) /4 + 1) % 7);
       int endDSTMonth=10;
       // DST is valid as:
       if (((month(t) > beginDSTMonth) && (month(t) < endDSTMonth))
       || ((month(t) == beginDSTMonth) && (day(t) >= beginDSTDate))
       || ((month(t) == endDSTMonth) && (day(t) <= endDSTDate))) {
         Serial.println("YES");
         t+= 3600;
       } else{
         Serial.println("NO");
       }
     }

     //Timezone adjust
     t+= 3600*(s_timezone-12);

      Serial.print("Local date and time: ");
      Serial.print(day(t));
      Serial.print("-");
      Serial.print(month(t));
      Serial.print("-");
      Serial.print(year(t));
      Serial.print(" ");
      Serial.print(hour(t));
      Serial.print(":");
      Serial.print(minute(t));
      Serial.print(":");
      Serial.println(second(t));  
      
      /*
      Serial.print(':');
      if ( ((epoch % 3600) / 60) < 10 ) {
        // In the first 10 minutes of each hour, we'll want a leading '0'
        Serial.print('0');
      }
      Serial.print((epoch  % 3600) / 60); // print the minute (3600 equals secs per minute)
      lminute = (epoch  % 3600) / 60;
      Serial.print(':');
      if ( (epoch % 60) < 10 ) {
        // In the first 10 seconds of each minute, we'll want a leading '0'
        Serial.print('0');
      }
      Serial.println(epoch % 60); // print the second
      lsecond = epoch % 60;
    }*/
      lsecond = second(t);
      lminute = minute(t);
      lhour = hour(t);
    }
  }
}
