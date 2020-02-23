#include "A4988RotatorController.h"
#include "A4988ElevationRotatorController.h"
#include "WiFi.h"

// OTA Update
#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>

// Autoconnect
#include <WebServer.h>
#include <AutoConnect.h>

#include <SPI.h>
#include <Wire.h>
#include <MechaQMC5883.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// Autoconnect
WebServer Server;
AutoConnect       Portal(Server);
AutoConnectConfig Config;       // Enable autoReconnect supported on v0.9.4
AutoConnectAux    Timezone;
// END Autoconnect

MechaQMC5883 qmc;

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
#define OLED_RESET     4 // Reset pin # (or -1 if sharing Arduino reset pin)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
char ip_address[16];

int moveSpeed = 1;
int azPos;
int elPos;
String inputString;
bool stringComplete;

const char* ssid = "karen";
const char* password =  "12a12b12c12d1";

WiFiServer wifiServer(4533);

#define BufferSize 256

# define X_EN  18           // ENA+(+5V) stepper motor enable , active low
# define X_DIR 14           // DIR+(+5v) axis stepper motor direction control
# define X_STP 13           // PUL+(+5v) axis stepper motor step control
// microstep
# define X_MICRO_1 19
# define X_MICRO_2 5
# define X_MICRO_3 12

# define EL_EN  32          //ENA+(+5V) stepper motor enable , active low
# define EL_DIR 25          //DIR+(+5v) axis stepper motor direction control
# define EL_STP 27          //PUL+(+5v) axis stepper motor step control
// microstep
# define EL_MICRO_1 4 
# define EL_MICRO_2 33 
# define EL_MICRO_3 26 

// Speed
# define TRACKING 800
# define INTERMEDIATE 400
# define FAST 200


A4988RotatorController xRot;            // Azimuth rotator Controller
A4988ElevationRotatorController elRot;  // Elevation rotato Controller (uses an ADXL345 for angle detection)

void displayAzimuth(int pos)
{
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0,0);
  display.println("AZ:" + String(pos));
}

void displayElevation(int pos)
{
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(70,0);
  display.println("EL: " + String(pos));
}

void displayLiveElevation()
{
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(70,0);
  display.println("EL:" + String(elRot.getLivePosition()));
}

void displayIPAddress(char *ip_address)
{
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0,20);
  display.println("IP: " + String(ip_address));
}

void displayFooterMessage(char *ip_address)
{
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0,45);
  display.println(String(ip_address));
}

void unwind()
{
  xRot.unwind();
}

// All slide functions move the rotator but don't alter the position counter
void slideUp(int distance)
{
  Serial.println("Up: ");
  elRot.slide(0-distance); 
}

void slideDown(int distance)
{
  Serial.println("Down: ");
  elRot.slide(distance); 
}

void slideRight(int distance)
{
  Serial.print("Slide right ");
  Serial.println(distance);
  xRot.on();
  xRot.slide(0-distance); 
}

void slideLeft(int distance)
{
  Serial.print("Slide left ");
  Serial.println(distance);
  xRot.on();
  xRot.slide(distance); 
}

// Find North
int home()
{
  int dir;
  int newHeading;
  
  delay(200);
  Serial.println("Find North...");
  display.clearDisplay();

  azPos = getHeading();
  xRot.slideTo(azPos);   // Leave the 'rotator' at 0 and move the antennas (or, move but don't update the rotator's position)
  
  display.fillScreen(BLACK);
  displayAzimuth(azPos);
  displayIPAddress(ip_address);
  displayFooterMessage("A message!");
  display.display();

  Serial.println("Start: " + azPos);
  xRot.setNewPosition(0);
  xRot.move();
  Serial.println("End: " + azPos);

  newHeading = getHeading();
  
  display.setTextSize(2);
  display.setTextColor(WHITE);
  display.setCursor(0,0);
  display.fillScreen(BLACK);
  displayAzimuth(newHeading);
  displayIPAddress(ip_address);
  display.display();

  return newHeading;
}

void displayTurnRight()
{
  displayFooterMessage("Turning right.");
}

void displayTurnLeft()
{
  displayFooterMessage("Turning left.");
}

void displayFoundNorth()
{
  displayFooterMessage("Pointing North.");
}

int getHeading()
{
  int x, y, z;
  int azimuth;

  xRot.off();
  elRot.off();
  delay(20);
  //float azimuth; //is supporting float too
  qmc.read(&x, &y, &z, &azimuth);
  xRot.on();
  elRot.on();

  return azimuth;
}

int getDirectonToTurn(int heading)
{
  if(heading > 358 || heading < 2) {
    return 0;
  } else if(heading <= 180) {
    return -1;
  } else if(heading > 180) {
    return 1;
  }
}

void setup() {// *************************************************************     setup
  Serial.begin(115200);         // Start the Serial communication to send messages to the computer

// OTA Update
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.println("Connection Failed! Rebooting...");
    delay(2000);
    ESP.restart();
  }

  ArduinoOTA
    .onStart([]() {
      String type;
      if (ArduinoOTA.getCommand() == U_FLASH)
        type = "sketch";
      else // U_SPIFFS
        type = "filesystem";

      // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
      Serial.println("Start updating " + type);
    })
    .onEnd([]() {
      Serial.println("\nEnd");
    })
    .onProgress([](unsigned int progress, unsigned int total) {
      Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
    })
    .onError([](ota_error_t error) {
      Serial.printf("Error[%u]: ", error);
      if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
      else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
      else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
      else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
      else if (error == OTA_END_ERROR) Serial.println("End Failed");
    });

  ArduinoOTA.begin();
// END OTA Update

// Autoconnect
  // Enable saved past credential by autoReconnect option,
  // even once it is disconnected.
  Config.autoReconnect = true;
  Portal.config(Config);

  Portal.join({ Timezone });        // Register aux. page

  // Behavior a root path of ESP8266WebServer.
  Server.on("/", rootPage);
  Server.on("/start", startPage);   // Set NTP server trigger handler

  // Establish a connection with an autoReconnect option.
  if (Portal.begin()) {
    Serial.println("WiFi connected: " + WiFi.localIP().toString());
  }
// END Autoconnect
  delay(10);
  Wire.setClock(1000000);
  Wire.begin();

  pinMode (X_EN, OUTPUT);
  pinMode (X_DIR, OUTPUT);
  pinMode (X_STP, OUTPUT);
  pinMode (EL_EN, OUTPUT);
  pinMode (EL_DIR, OUTPUT);
  pinMode (EL_STP, OUTPUT);
  pinMode(X_MICRO_1, OUTPUT);
  pinMode(X_MICRO_2, OUTPUT);
  pinMode(X_MICRO_3, OUTPUT);
  pinMode(EL_MICRO_1, OUTPUT);
  pinMode(EL_MICRO_2, OUTPUT);
  pinMode(EL_MICRO_3, OUTPUT);
  
  digitalWrite (X_EN, HIGH); //ENA low=enabled
  digitalWrite (X_DIR, LOW); //DIR
  digitalWrite (X_STP, LOW); //PUL
  digitalWrite (EL_EN, HIGH); //ENA low=enabled
  digitalWrite (EL_DIR, LOW); //DIR
  digitalWrite (EL_STP, LOW); //PUL

  xRot.setDefaults();
  xRot.setPins(X_EN, X_DIR, X_STP, X_MICRO_1, X_MICRO_2, X_MICRO_3);              // setPins(int enable_pin, int direction_pin, int step_pin, int microstep_pin_1, int microstep_pin_2, int microstep_pin_3);
  xRot.setTrackingSpeed(TRACKING);
  xRot.setIntermediateSpeed(INTERMEDIATE);
  xRot.setFastSpeed(FAST);
  xRot.setMicrostep(1, 0, 0);
  xRot.flipRotation(1);

  elRot.setDefaults();
  elRot.setPins(EL_EN, EL_DIR, EL_STP, EL_MICRO_1, EL_MICRO_2, EL_MICRO_3);       // setPins(int en_5v, int dir_5v, int stp_5v, EL_micro_1, EL_micro_2, EL_micro_3);
  elRot.setTrackingSpeed(TRACKING);
  elRot.setIntermediateSpeed(INTERMEDIATE);
  elRot.setFastSpeed(FAST);
  elRot.flipRotation(1);
  elRot.adxlOn();

// Low  Low    Low      Full step
// High Low    Low      Half step
// Low  High   Low      Quarter step
// High High   Low      Eighth step
// High High   High     Sixteenth step
  xRot.setMicrostep(1, 1, 1);
  elRot.setMicrostep(1, 1, 1);

  azPos = 0;
  elPos = 0;

  delay(100);
  sprintf(ip_address, "%d.%d.%d.%d", WiFi.localIP()[0], WiFi.localIP()[1], WiFi.localIP()[2], WiFi.localIP()[3] );
  sprintf(ip_address, "%d.%d.%d.%d", WiFi.localIP()[0], WiFi.localIP()[1], WiFi.localIP()[2], WiFi.localIP()[3] );
  Serial.println("Connected to the WiFi network");
  Serial.print("My IP address is: ");
  Serial.println(ip_address);
  Serial.println("Set up Gpredict to connect to this IP address using port 4533.");

  wifiServer.begin();

  // Compass
  qmc.init();
  qmc.setMode(Mode_Continuous,ODR_100Hz,RNG_2G,OSR_256);
/*  
  Mode : Mode_Standby / Mode_Continuous

  ODR : ODR_10Hz / ODR_50Hz / ODR_100Hz / ODR_200Hz
  ouput data update rate

  RNG : RNG_2G / RNG_8G
  magneticfield measurement range

  OSR : OSR_512 / OSR_256 / OSR_128 / OSR_64
  over sampling rate
*/

//   SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { // Address 0x3C for 128x32
    Serial.println(F("SSD1306 allocation failed"));
    for(;;); // Don't proceed, loop forever
  }
  display.display();
  delay(5000);
  while(home() > 0){}
  displayFoundNorth();

  elRot.moveTo(0);
  Serial.print("Found horizontal. Angle is: ");
  Serial.println(elRot.getLivePosition());
  displayAzimuth(azPos);
  displayElevation(elRot.getLivePosition());
  display.display();
}

void loop()
{
  double angleAz;
  double angleEl;
  String data;

  ArduinoOTA.handle();      // Check for an OTA update
  Portal.handleClient();    // Handle the Autoconnect portal

//    int ping = elRot.getLivePosition(); // Test code to check A4988 is working
    
  WiFiClient client = wifiServer.available();
  if (client) {
    while (client.connected()) {

      if (client.available()) {
        data = client.readStringUntil('\n');
        data.trim();

        if(data.indexOf("hm") != -1) {
          while(home() > 0){}
          displayFoundNorth();
        } else if(data == "u10") {
          slideUp(10);
        } else if(data.indexOf("u5") != -1) {
          slideUp(5);
        } else if(data.indexOf("d10") != -1) {
          slideDown(10);
        } else if(data.indexOf("d5") != -1) {
          slideDown(5);
        } else if(data.indexOf("l10") != -1) {
          slideLeft(10);
        } else if(data.indexOf("l5") != -1) {
          slideLeft(5);
        } else if(data.indexOf("r10") != -1) {
          slideRight(10);
        } else if(data.indexOf("r5") != -1) {
          slideRight(5);
        } else if(data.indexOf("q") != -1 || data.indexOf(" hm") != -1) {
          Serial.println("Zeroing elevation.");
          elRot.moveTo(0);    // Home elevation
          elPos = 0;
          Serial.println("All done, going home.");
          int nowPos = elRot.getLivePosition();
          Serial.print("Elevation is now at: ");
          Serial.println(nowPos);
          unwind();
          while(home() > 0){}         // Home azimuth
          elRot.off();                // Take a rest
          xRot.off();
        } else if(data.indexOf(" uw") != -1) {
          Serial.println("Unwinding.");
          elRot.moveTo(0);
          elPos = 0;
          int nowPos = elRot.getLivePosition();
          Serial.print("Elevation is now at: ");
          Serial.println(nowPos);
          unwind();
        }

        if (data.length() > 3) {
          String az = data.substring(2);
          az = az.substring(0, az.indexOf(' '));
          String el = data.substring(data.indexOf(' ')); 
          el = el.substring(el.indexOf(' ') + 1);
          el = el.substring(el.indexOf(' ') + 1);

          if(el.length() > 3) {
            angleEl = el.toFloat();
            elPos = angleEl;
            Serial.print("Converted ");
            Serial.print(el);
            Serial.print(" to ");
            Serial.println(elPos);
            elRot.on();
            elRot.moveTo(elPos);
                
            angleAz = az.toFloat();
            azPos = (int)angleAz;
            Serial.print("Converted ");
            Serial.print(az);
            Serial.print(" to ");
            Serial.println(azPos);
            xRot.on();
            xRot.setNewPosition(azPos);
            xRot.move();

            displayAzimuth(azPos);
            displayElevation(elRot.getLivePosition());
            displayLiveElevation();
            display.display();
          }
        }

        client.print("azPos + " " + elPos");                                      // Send rotator position back to Gpredict
        Serial.print("Position sent back as Azimuth: ");
        Serial.print(azPos);
        Serial.print(" Elevation: ");
        Serial.println(elPos);

        displayAzimuth(azPos);
        displayElevation(elPos);
        display.display();
      }
    }

    client.stop();
  }
}

// Start of wifi management code (Autoconnect)
void rootPage() {
  String  content =
    "<html>"
    "<head>"
    "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">"
    "<script type=\"text/javascript\">"
    "setTimeout(\"location.reload()\", 1000);"
    "</script>"
    "</head>"
    "<body>"
    "<h2 align=\"center\" style=\"color:blue;margin:20px;\">Hello, world</h2>"
    "<h3 align=\"center\" style=\"color:gray;margin:10px;\">{{DateTime}}</h3>"
    "<p style=\"text-align:center;\">Reload the page to update the time.</p>"
    "<p></p><p style=\"padding-top:15px;text-align:center\">" AUTOCONNECT_LINK(COG_24) "</p>"
    "</body>"
    "</html>";
  static const char *wd[7] = { "Sun","Mon","Tue","Wed","Thr","Fri","Sat" };
  struct tm *tm;
  time_t  t;
  char    dateTime[26];

  t = time(NULL);
  tm = localtime(&t);
  sprintf(dateTime, "%04d/%02d/%02d(%s) %02d:%02d:%02d.",
    tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
    wd[tm->tm_wday],
    tm->tm_hour, tm->tm_min, tm->tm_sec);
  content.replace("{{DateTime}}", String(dateTime));
  Server.send(200, "text/html", content);
}

void startPage() {
  // The /start page just constitutes timezone,
  // it redirects to the root page without the content response.
  Server.sendHeader("Location", String("http://") + Server.client().localIP().toString() + String("/"));
  Server.send(302, "text/plain", "");
  Server.client().flush();
  Server.client().stop();
}
// End of wifi management code (Autoconnect)
